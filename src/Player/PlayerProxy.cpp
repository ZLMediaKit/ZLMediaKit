/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Common/config.h"
#include "PlayerProxy.h"
#include "Util/mini.h"
#include "Util/MD5.h"
#include "Util/logger.h"
#include "Extension/AAC.h"

using namespace toolkit;
using namespace std;

namespace mediakit {

PlayerProxy::PlayerProxy(const string &vhost, const string &app, const string &stream_id, const ProtocolOption &option,
                         int retry_count, const EventPoller::Ptr &poller) : MediaPlayer(poller) , _option(option) {
    _vhost = vhost;
    _app = app;
    _stream_id = stream_id;
    _retry_count = retry_count;
    _on_close = [](const SockException &) {};
    (*this)[Client::kWaitTrackReady] = false;
}

void PlayerProxy::setPlayCallbackOnce(const function<void(const SockException &ex)> &cb) {
    _on_play = cb;
}

void PlayerProxy::setOnClose(const function<void(const SockException &ex)> &cb) {
    _on_close = cb ? cb : [](const SockException &) {};
}

void PlayerProxy::play(const string &strUrlTmp) {
    weak_ptr<PlayerProxy> weakSelf = shared_from_this();
    std::shared_ptr<int> piFailedCnt(new int(0)); //连续播放失败次数
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
            // 播放成功
            *piFailedCnt = 0;//连续播放失败次数清0
            strongSelf->onPlaySuccess();
        } else if (*piFailedCnt < strongSelf->_retry_count || strongSelf->_retry_count < 0) {
            // 播放失败，延时重试播放
            strongSelf->rePlay(strUrlTmp, (*piFailedCnt)++);
        } else {
            //达到了最大重试次数，回调关闭
            strongSelf->_on_close(err);
        }
    });
    setOnShutdown([weakSelf, strUrlTmp, piFailedCnt](const SockException &err) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }

        //注销直接拉流代理产生的流：#532
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
        //播放异常中断，延时重试播放
        if (*piFailedCnt < strongSelf->_retry_count || strongSelf->_retry_count < 0) {
            strongSelf->rePlay(strUrlTmp, (*piFailedCnt)++);
        } else {
            //达到了最大重试次数，回调关闭
            strongSelf->_on_close(err);
        }
    });
    MediaPlayer::play(strUrlTmp);
    _pull_url = strUrlTmp;
    setDirectProxy();
}

void PlayerProxy::setDirectProxy() {
    MediaSource::Ptr mediaSource;
    if (dynamic_pointer_cast<RtspPlayer>(_delegate)) {
        //rtsp拉流
        GET_CONFIG(bool, directProxy, Rtsp::kDirectProxy);
        if (directProxy) {
            mediaSource = std::make_shared<RtspMediaSource>(_vhost, _app, _stream_id);
        }
    } else if (dynamic_pointer_cast<RtmpPlayer>(_delegate)) {
        //rtmp拉流,rtmp强制直接代理
        mediaSource = std::make_shared<RtmpMediaSource>(_vhost, _app, _stream_id);
    }
    if (mediaSource) {
        setMediaSource(mediaSource);
        mediaSource->setListener(shared_from_this());
    }
}

PlayerProxy::~PlayerProxy() {
    _timer.reset();
    // 避免析构时, 忘记回调api请求
     if(_on_play) {
        _on_play(SockException(Err_shutdown, "player proxy close"));
        _on_play = nullptr;
    }
}

void PlayerProxy::rePlay(const string &strUrl, int iFailedCnt) {
    auto iDelay = MAX(2 * 1000, MIN(iFailedCnt * 3000, 60 * 1000));
    weak_ptr<PlayerProxy> weakSelf = shared_from_this();
    _timer = std::make_shared<Timer>(iDelay / 1000.0f, [weakSelf, strUrl, iFailedCnt]() {
        //播放失败次数越多，则延时越长
        auto strongPlayer = weakSelf.lock();
        if (!strongPlayer) {
            return false;
        }
        WarnL << "重试播放[" << iFailedCnt << "]:" << strUrl;
        strongPlayer->MediaPlayer::play(strUrl);
        strongPlayer->setDirectProxy();
        return false;
    }, getPoller());
}

bool PlayerProxy::close(MediaSource &sender, bool force) {
    if (!force && totalReaderCount()) {
        return false;
    }

    //通知其停止推流
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
    WarnL << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
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

toolkit::EventPoller::Ptr PlayerProxy::getOwnerPoller(MediaSource &sender) {
    return getPoller();
}

void PlayerProxy::onPlaySuccess() {
    GET_CONFIG(bool, reset_when_replay, General::kResetWhenRePlay);
    if (dynamic_pointer_cast<RtspMediaSource>(_media_src)) {
        //rtsp拉流代理
        if (reset_when_replay || !_muxer) {
            _option.enable_rtsp = false;
            _muxer = std::make_shared<MultiMediaSourceMuxer>(_vhost, _app, _stream_id, getDuration(), _option);
        }
    } else if (dynamic_pointer_cast<RtmpMediaSource>(_media_src)) {
        //rtmp拉流代理
        if (reset_when_replay || !_muxer) {
            _option.enable_rtmp = false;
            _muxer = std::make_shared<MultiMediaSourceMuxer>(_vhost, _app, _stream_id, getDuration(), _option);
        }
    } else {
        //其他拉流代理
        if (reset_when_replay || !_muxer) {
            _muxer = std::make_shared<MultiMediaSourceMuxer>(_vhost, _app, _stream_id, getDuration(), _option);
        }
    }
    _muxer->setMediaListener(shared_from_this());

    auto videoTrack = getTrack(TrackVideo, false);
    if (videoTrack) {
        //添加视频
        _muxer->addTrack(videoTrack);
        //视频数据写入_mediaMuxer
        videoTrack->addDelegate(_muxer);
    }

    auto audioTrack = getTrack(TrackAudio, false);
    if (audioTrack) {
        //添加音频
        _muxer->addTrack(audioTrack);
        //音频数据写入_mediaMuxer
        audioTrack->addDelegate(_muxer);
    }

    //添加完毕所有track，防止单track情况下最大等待3秒
    _muxer->addTrackCompleted();

    if (_media_src) {
        //让_muxer对象拦截一部分事件(比如说录像相关事件)
        _media_src->setListener(_muxer);
    }
}

} /* namespace mediakit */
