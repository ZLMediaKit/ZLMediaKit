/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "PlayerProxy.h"
#include "Common/config.h"
#include "Extension/AAC.h"
#include "Rtmp/RtmpMediaSource.h"
#include "Rtmp/RtmpPlayer.h"
#include "Rtsp/RtspMediaSource.h"
#include "Rtsp/RtspPlayer.h"
#include "Util/MD5.h"
#include "Util/logger.h"
#include "Util/mini.h"

using namespace toolkit;
using namespace std;

namespace mediakit {

PlayerProxy::PlayerProxy(
    const string &vhost, const string &app, const string &stream_id, const ProtocolOption &option, int retry_count,
    const EventPoller::Ptr &poller, int reconnect_delay_min, int reconnect_delay_max, int reconnect_delay_step)
    : MediaPlayer(poller)
    , _option(option) {
    _tuple.vhost = vhost;
    _tuple.app = app;
    _tuple.stream = stream_id;
    _retry_count = retry_count;

    setOnClose(nullptr);
    setOnConnect(nullptr);
    setOnDisconnect(nullptr);
    
    _reconnect_delay_min = reconnect_delay_min > 0 ? reconnect_delay_min : 2;
    _reconnect_delay_max = reconnect_delay_max > 0 ? reconnect_delay_max : 60;
    _reconnect_delay_step = reconnect_delay_step > 0 ? reconnect_delay_step : 3;
    _live_secs = 0;
    _live_status = 1;
    _repull_count = 0;
    (*this)[Client::kWaitTrackReady] = false;
}

void PlayerProxy::setPlayCallbackOnce(function<void(const SockException &ex)> cb) {
    _on_play = std::move(cb);
}

void PlayerProxy::setOnClose(function<void(const SockException &ex)> cb) {
    _on_close = cb ? std::move(cb) : [](const SockException &) {};
}

void PlayerProxy::setOnDisconnect(std::function<void()> cb) {
    _on_disconnect = cb ? std::move(cb) : [] () {};
}

void PlayerProxy::setOnConnect(std::function<void(const TranslationInfo&)> cb) {
    _on_connect = cb ? std::move(cb) : [](const TranslationInfo&) {};
}

void PlayerProxy::setTranslationInfo()
{
    _transtalion_info.byte_speed = _media_src ? _media_src->getBytesSpeed() : -1;
    _transtalion_info.start_time_stamp = _media_src ? _media_src->getCreateStamp() : 0;
    _transtalion_info.stream_info.clear();
    auto tracks = _muxer->getTracks();
    for (auto &track : tracks) {
        _transtalion_info.stream_info.emplace_back();
        auto &back = _transtalion_info.stream_info.back();
        back.bitrate = track->getBitRate();
        back.codec_type = track->getTrackType();
        back.codec_name = track->getCodecName();
        switch (back.codec_type) {
            case TrackAudio : {
                auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
                back.audio_sample_rate = audio_track->getAudioSampleRate();
                back.audio_channel = audio_track->getAudioChannel();
                back.audio_sample_bit = audio_track->getAudioSampleBit();
                break;
            }
            case TrackVideo : {
                auto video_track = dynamic_pointer_cast<VideoTrack>(track);
                back.video_width = video_track->getVideoWidth();
                back.video_height = video_track->getVideoHeight();
                back.video_fps = video_track->getVideoFps();
                break;
            }
            default:
                break;
        }
    }
}

void PlayerProxy::play(const string &strUrlTmp) {
    weak_ptr<PlayerProxy> weakSelf = shared_from_this();
    std::shared_ptr<int> piFailedCnt(new int(0)); // 连续播放失败次数
    setOnPlayResult([weakSelf, strUrlTmp, piFailedCnt](const SockException &err) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }

        if (strongSelf->_on_play) {
            strongSelf->_on_play(err);
            strongSelf->_on_play = nullptr;
        }

        if (!err) {
            // 取消定时器,避免hls拉流索引文件因为网络波动失败重连成功后出现循环重试的情况
            strongSelf->_timer.reset();
            strongSelf->_live_ticker.resetTime();
            strongSelf->_live_status = 0;
            // 播放成功
            *piFailedCnt = 0; // 连续播放失败次数清0
            strongSelf->onPlaySuccess();
            strongSelf->setTranslationInfo();
            strongSelf->_on_connect(strongSelf->_transtalion_info);  

            InfoL << "play " << strUrlTmp << " success";
        } else if (*piFailedCnt < strongSelf->_retry_count || strongSelf->_retry_count < 0) {
            // 播放失败，延时重试播放
            strongSelf->_on_disconnect();
            strongSelf->rePlay(strUrlTmp, (*piFailedCnt)++);
        } else {
            // 达到了最大重试次数，回调关闭
            strongSelf->_on_close(err);
        }
    });
    setOnShutdown([weakSelf, strUrlTmp, piFailedCnt](const SockException &err) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }

        // 注销直接拉流代理产生的流：#532
        strongSelf->setMediaSource(nullptr);

        if (strongSelf->_muxer) {
            auto tracks = strongSelf->MediaPlayer::getTracks(false);
            for (auto &track : tracks) {
                track->delDelegate(strongSelf->_muxer.get());
            }

            GET_CONFIG(bool, reset_when_replay, General::kResetWhenRePlay);
            if (reset_when_replay) {
                strongSelf->_muxer.reset();
            } else {
                strongSelf->_muxer->resetTracks();
            }
        }

        if (*piFailedCnt == 0) {
            // 第一次重拉更新时长
            strongSelf->_live_secs += strongSelf->_live_ticker.elapsedTime() / 1000;
            strongSelf->_live_ticker.resetTime();
            TraceL << " live secs " << strongSelf->_live_secs;
        }

        // 播放异常中断，延时重试播放
        if (*piFailedCnt < strongSelf->_retry_count || strongSelf->_retry_count < 0) {
            strongSelf->_repull_count++;
            strongSelf->rePlay(strUrlTmp, (*piFailedCnt)++);
        } else {
            // 达到了最大重试次数，回调关闭
            strongSelf->_on_close(err);
        }
    });
    try {
        MediaPlayer::play(strUrlTmp);
    } catch (std::exception &ex) {
        ErrorL << ex.what();
        onPlayResult(SockException(Err_other, ex.what()));
        return;
    }
    _pull_url = strUrlTmp;
    setDirectProxy();
}

void PlayerProxy::setDirectProxy() {
    MediaSource::Ptr mediaSource;
    if (dynamic_pointer_cast<RtspPlayer>(_delegate)) {
        // rtsp拉流
        GET_CONFIG(bool, directProxy, Rtsp::kDirectProxy);
        if (directProxy) {
            mediaSource = std::make_shared<RtspMediaSource>(_tuple);
        }
    } else if (dynamic_pointer_cast<RtmpPlayer>(_delegate)) {
        // rtmp拉流,rtmp强制直接代理
        mediaSource = std::make_shared<RtmpMediaSource>(_tuple);
    }
    if (mediaSource) {
        setMediaSource(mediaSource);
    }
}

PlayerProxy::~PlayerProxy() {
    _timer.reset();
    // 避免析构时, 忘记回调api请求
    if (_on_play) {
        _on_play(SockException(Err_shutdown, "player proxy close"));
        _on_play = nullptr;
    }
}

void PlayerProxy::rePlay(const string &strUrl, int iFailedCnt) {
    auto iDelay = MAX(_reconnect_delay_min * 1000, MIN(iFailedCnt * _reconnect_delay_step * 1000, _reconnect_delay_max * 1000));
    weak_ptr<PlayerProxy> weakSelf = shared_from_this();
    _timer = std::make_shared<Timer>(
        iDelay / 1000.0f,
        [weakSelf, strUrl, iFailedCnt]() {
            // 播放失败次数越多，则延时越长
            auto strongPlayer = weakSelf.lock();
            if (!strongPlayer) {
                return false;
            }
            WarnL << "重试播放[" << iFailedCnt << "]:" << strUrl;
            strongPlayer->MediaPlayer::play(strUrl);
            strongPlayer->setDirectProxy();
            return false;
        },
        getPoller());
}

bool PlayerProxy::close(MediaSource &sender) {
    // 通知其停止推流
    weak_ptr<PlayerProxy> weakSelf = dynamic_pointer_cast<PlayerProxy>(shared_from_this());
    getPoller()->async_first([weakSelf]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->_muxer.reset();
        strongSelf->setMediaSource(nullptr);
        strongSelf->teardown();
    });
    _on_close(SockException(Err_shutdown, "closed by user"));
    WarnL << "close media: " << sender.getUrl();
    return true;
}

int PlayerProxy::totalReaderCount() {
    return (_muxer ? _muxer->totalReaderCount() : 0) + (_media_src ? _media_src->readerCount() : 0);
}

int PlayerProxy::totalReaderCount(MediaSource &sender) {
    return totalReaderCount();
}

MediaOriginType PlayerProxy::getOriginType(MediaSource &sender) const {
    return MediaOriginType::pull;
}

string PlayerProxy::getOriginUrl(MediaSource &sender) const {
    return _pull_url;
}

std::shared_ptr<SockInfo> PlayerProxy::getOriginSock(MediaSource &sender) const {
    return getSockInfo();
}

float PlayerProxy::getLossRate(MediaSource &sender, TrackType type) {
    return getPacketLossRate(type);
}

TranslationInfo PlayerProxy::getTranslationInfo() {
    return _transtalion_info;
}

void PlayerProxy::onPlaySuccess() {
    GET_CONFIG(bool, reset_when_replay, General::kResetWhenRePlay);
    if (dynamic_pointer_cast<RtspMediaSource>(_media_src)) {
        // rtsp拉流代理
        if (reset_when_replay || !_muxer) {
            _option.enable_rtsp = false;
            _muxer = std::make_shared<MultiMediaSourceMuxer>(_tuple, getDuration(), _option);
        }
    } else if (dynamic_pointer_cast<RtmpMediaSource>(_media_src)) {
        // rtmp拉流代理
        if (reset_when_replay || !_muxer) {
            _option.enable_rtmp = false;
            _muxer = std::make_shared<MultiMediaSourceMuxer>(_tuple, getDuration(), _option);
        }
    } else {
        // 其他拉流代理
        if (reset_when_replay || !_muxer) {
            _muxer = std::make_shared<MultiMediaSourceMuxer>(_tuple, getDuration(), _option);
        }
    }
    _muxer->setMediaListener(shared_from_this());

    auto videoTrack = getTrack(TrackVideo, false);
    if (videoTrack) {
        // 添加视频
        _muxer->addTrack(videoTrack);
        // 视频数据写入_mediaMuxer
        videoTrack->addDelegate(_muxer);
    }

    auto audioTrack = getTrack(TrackAudio, false);
    if (audioTrack) {
        // 添加音频
        _muxer->addTrack(audioTrack);
        // 音频数据写入_mediaMuxer
        audioTrack->addDelegate(_muxer);
    }

    // 添加完毕所有track，防止单track情况下最大等待3秒
    _muxer->addTrackCompleted();

    if (_media_src) {
        // 让_muxer对象拦截一部分事件(比如说录像相关事件)
        _media_src->setListener(_muxer);
    }
}

int PlayerProxy::getStatus() {
    return _live_status.load();
}
uint64_t PlayerProxy::getLiveSecs() {
    if (_live_status == 0) {
        return _live_secs + _live_ticker.elapsedTime() / 1000;
    }
    return _live_secs;
}

uint64_t PlayerProxy::getRePullCount() {
    return _repull_count;
}

} /* namespace mediakit */
