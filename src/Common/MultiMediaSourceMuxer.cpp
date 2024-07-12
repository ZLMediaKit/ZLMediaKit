﻿/*
* Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
*
* This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
*
* Use of this source code is governed by MIT-like license that can be found in the
* LICENSE file in the root of the source tree. All contributing project authors
* may be found in the AUTHORS file in the root of the source tree.
*/

#include <math.h>
#include "Common/config.h"
#include "MultiMediaSourceMuxer.h"

using namespace std;
using namespace toolkit;

namespace toolkit {
    StatisticImp(mediakit::MultiMediaSourceMuxer);
}

namespace mediakit {

namespace {
class MediaSourceForMuxer : public MediaSource {
public:
    MediaSourceForMuxer(const MultiMediaSourceMuxer::Ptr &muxer)
        : MediaSource("muxer", muxer->getMediaTuple()) {
        MediaSource::setListener(muxer);
    }
    int readerCount() override { return 0; }
};
} // namespace

class FramePacedSender : public FrameWriterInterface, public std::enable_shared_from_this<FramePacedSender> {
public:
    using OnFrame = std::function<void(const Frame::Ptr &frame)>;
    // 最小缓存100ms数据
    static constexpr auto kMinCacheMS = 100;

    FramePacedSender(uint32_t paced_sender_ms, OnFrame cb) {
        _paced_sender_ms = paced_sender_ms;
        _cb = std::move(cb);
    }

    void resetTimer(const EventPoller::Ptr &poller) {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        std::weak_ptr<FramePacedSender> weak_self = shared_from_this();
        _timer = std::make_shared<Timer>(_paced_sender_ms / 1000.0f, [weak_self]() {
            if (auto strong_self = weak_self.lock()) {
                strong_self->onTick();
                return true;
            }
            return false;
        }, poller);
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        if (!_timer) {
            setCurrentStamp(frame->dts());
            resetTimer(EventPoller::getCurrentPoller());
        }

        _cache.emplace_back(frame->dts() + _cache_ms, Frame::getCacheAbleFrame(frame));
        return true;
    }

private:
    void onTick() {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto dst = _cache.empty() ? 0 : _cache.back().first;
        while (!_cache.empty()) {
            auto &front = _cache.front();
            if (getCurrentStamp() < front.first) {
                // 还没到消费时间
                break;
            }
            // 时间到了，该消费frame了
            _cb(front.second);
            _cache.pop_front();
        }

        if (_cache.empty() && dst) {
            // 消费太快，需要增加缓存大小
            setCurrentStamp(dst);
            _cache_ms += kMinCacheMS;
        }

        // 消费太慢，需要强制flush数据
        if (_cache.size() > 25 * 5) {
            WarnL << "Flush frame paced sender cache: " << _cache.size();
            while (!_cache.empty()) {
                auto &front = _cache.front();
                _cb(front.second);
                _cache.pop_front();
            }
            setCurrentStamp(dst);
        }
    }

    uint64_t getCurrentStamp() { return _ticker.elapsedTime() + _stamp_offset; }

    void setCurrentStamp(uint64_t stamp) {
        _stamp_offset = stamp;
        _ticker.resetTime();
    }

private:
    uint32_t _paced_sender_ms;
    uint32_t _cache_ms = kMinCacheMS;
    uint64_t _stamp_offset = 0;
    OnFrame _cb;
    Ticker _ticker;
    Timer::Ptr _timer;
    std::recursive_mutex _mtx;
    std::list<std::pair<uint64_t, Frame::Ptr>> _cache;
};

static std::shared_ptr<MediaSinkInterface> makeRecorder(MediaSource &sender, const vector<Track::Ptr> &tracks, Recorder::type type, const ProtocolOption &option){
    auto recorder = Recorder::createRecorder(type, sender.getMediaTuple(), option);
    for (auto &track : tracks) {
        recorder->addTrack(track);
    }
    recorder->addTrackCompleted();
    return recorder;
}

static string getTrackInfoStr(const TrackSource *track_src){
    _StrPrinter codec_info;
    auto tracks = track_src->getTracks(true);
    for (auto &track : tracks) {
        track->update();
        auto codec_type = track->getTrackType();
        codec_info << track->getCodecName();
        switch (codec_type) {
            case TrackAudio : {
                auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
                codec_info << "["
                           << audio_track->getAudioSampleRate() << "/"
                           << audio_track->getAudioChannel() << "/"
                           << audio_track->getAudioSampleBit() << "] ";
                break;
            }
            case TrackVideo : {
                auto video_track = dynamic_pointer_cast<VideoTrack>(track);
                codec_info << "["
                           << video_track->getVideoWidth() << "/"
                           << video_track->getVideoHeight() << "/"
                           << round(video_track->getVideoFps()) << "] ";
                break;
            }
            default:
                break;
        }
    }
    return std::move(codec_info);
}

const ProtocolOption &MultiMediaSourceMuxer::getOption() const {
    return _option;
}

const MediaTuple &MultiMediaSourceMuxer::getMediaTuple() const {
    return _tuple;
}

std::string MultiMediaSourceMuxer::shortUrl() const {
    auto ret = getOriginUrl(MediaSource::NullMediaSource());
    if (!ret.empty()) {
        return ret;
    }
    return _tuple.shortUrl();
}

void MultiMediaSourceMuxer::forEachRtpSender(const std::function<void(const std::string &ssrc)> &cb) const {
    for (auto &pr : _rtp_sender) {
        cb(pr.first);
    }
}

MultiMediaSourceMuxer::MultiMediaSourceMuxer(const MediaTuple& tuple, float dur_sec, const ProtocolOption &option): _tuple(tuple) {
    if (!option.stream_replace.empty()) {
        // 支持在on_publish hook中替换stream_id
        _tuple.stream = option.stream_replace;
    }
    _poller = EventPollerPool::Instance().getPoller();
    _create_in_poller = _poller->isCurrentThread();
    _option = option;
    _dur_sec = dur_sec;
    setMaxTrackCount(option.max_track);

    if (option.enable_rtmp) {
        _rtmp = std::make_shared<RtmpMediaSourceMuxer>(_tuple, option, std::make_shared<TitleMeta>(dur_sec));
    }
    if (option.enable_rtsp) {
        _rtsp = std::make_shared<RtspMediaSourceMuxer>(_tuple, option, std::make_shared<TitleSdp>(dur_sec));
    }
    if (option.enable_hls) {
        _hls = dynamic_pointer_cast<HlsRecorder>(Recorder::createRecorder(Recorder::type_hls, _tuple, option));
    }
    if (option.enable_hls_fmp4) {
        _hls_fmp4 = dynamic_pointer_cast<HlsFMP4Recorder>(Recorder::createRecorder(Recorder::type_hls_fmp4, _tuple, option));
    }
    if (option.enable_mp4) {
        _mp4 = Recorder::createRecorder(Recorder::type_mp4, _tuple, option);
    }
    if (option.enable_ts) {
        _ts = dynamic_pointer_cast<TSMediaSourceMuxer>(Recorder::createRecorder(Recorder::type_ts, _tuple, option));
    }
    if (option.enable_fmp4) {
        _fmp4 = dynamic_pointer_cast<FMP4MediaSourceMuxer>(Recorder::createRecorder(Recorder::type_fmp4, _tuple, option));
    }

    //音频相关设置
    enableAudio(option.enable_audio);
    enableMuteAudio(option.add_mute_audio);
}

void MultiMediaSourceMuxer::setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener) {
    setDelegate(listener);

    auto self = shared_from_this();
    //拦截事件
    if (_rtmp) {
        _rtmp->setListener(self);
    }
    if (_rtsp) {
        _rtsp->setListener(self);
    }
    if (_ts) {
        _ts->setListener(self);
    }
    if (_fmp4) {
        _fmp4->setListener(self);
    }
    if (_hls_fmp4) {
        _hls_fmp4->setListener(self);
    }
    if (_hls) {
        _hls->setListener(self);
    }
}

void MultiMediaSourceMuxer::setTrackListener(const std::weak_ptr<Listener> &listener) {
    _track_listener = listener;
}

int MultiMediaSourceMuxer::totalReaderCount() const {
    return (_rtsp ? _rtsp->readerCount() : 0) +
           (_rtmp ? _rtmp->readerCount() : 0) +
           (_ts ? _ts->readerCount() : 0) +
           (_fmp4 ? _fmp4->readerCount() : 0) +
           (_mp4 ? _option.mp4_as_player : 0) +
           (_hls ? _hls->readerCount() : 0) +
           (_hls_fmp4 ? _hls_fmp4->readerCount() : 0) +
           (_ring ? _ring->readerCount() : 0);
}

void MultiMediaSourceMuxer::setTimeStamp(uint32_t stamp) {
    if (_rtmp) {
        _rtmp->setTimeStamp(stamp);
    }
    if (_rtsp) {
        _rtsp->setTimeStamp(stamp);
    }
}

int MultiMediaSourceMuxer::totalReaderCount(MediaSource &sender) {
    auto listener = getDelegate();
    if (!listener) {
        return totalReaderCount();
    }
    try {
        return listener->totalReaderCount(sender);
    } catch (MediaSourceEvent::NotImplemented &) {
        //listener未重载totalReaderCount
        return totalReaderCount();
    }
}

//此函数可能跨线程调用
bool MultiMediaSourceMuxer::setupRecord(MediaSource &sender, Recorder::type type, bool start, const string &custom_path, size_t max_second) {
    CHECK(getOwnerPoller(MediaSource::NullMediaSource())->isCurrentThread(), "Can only call setupRecord in it's owner poller");
    onceToken token(nullptr, [&]() {
        if (_option.mp4_as_player && type == Recorder::type_mp4) {
            //开启关闭mp4录制，触发观看人数变化相关事件
            onReaderChanged(sender, totalReaderCount());
        }
    });
    switch (type) {
        case Recorder::type_hls : {
            if (start && !_hls) {
                //开始录制
                _option.hls_save_path = custom_path;
                auto hls = dynamic_pointer_cast<HlsRecorder>(makeRecorder(sender, getTracks(), type, _option));
                if (hls) {
                    //设置HlsMediaSource的事件监听器
                    hls->setListener(shared_from_this());
                }
                _hls = hls;
            } else if (!start && _hls) {
                //停止录制
                _hls = nullptr;
            }
            return true;
        }
        case Recorder::type_mp4 : {
            if (start && !_mp4) {
                //开始录制
                _option.mp4_save_path = custom_path;
                _option.mp4_max_second = max_second;
                _mp4 = makeRecorder(sender, getTracks(), type, _option);
            } else if (!start && _mp4) {
                //停止录制
                _mp4 = nullptr;
            }
            return true;
        }
        case Recorder::type_hls_fmp4: {
            if (start && !_hls_fmp4) {
                //开始录制
                _option.hls_save_path = custom_path;
                auto hls = dynamic_pointer_cast<HlsFMP4Recorder>(makeRecorder(sender, getTracks(), type, _option));
                if (hls) {
                    //设置HlsMediaSource的事件监听器
                    hls->setListener(shared_from_this());
                }
                _hls_fmp4 = hls;
            } else if (!start && _hls_fmp4) {
                //停止录制
                _hls_fmp4 = nullptr;
            }
            return true;
        }
        case Recorder::type_fmp4: {
            if (start && !_fmp4) {
                auto fmp4 = dynamic_pointer_cast<FMP4MediaSourceMuxer>(makeRecorder(sender, getTracks(), type, _option));
                if (fmp4) {
                    fmp4->setListener(shared_from_this());
                }
                _fmp4 = fmp4;
            } else if (!start && _fmp4) {
                _fmp4 = nullptr;
            }
            return true;
        }
        case Recorder::type_ts: {
            if (start && !_ts) {
                auto ts = dynamic_pointer_cast<TSMediaSourceMuxer>(makeRecorder(sender, getTracks(), type, _option));
                if (ts) {
                    ts->setListener(shared_from_this());
                }
                _ts = ts;
            } else if (!start && _ts) {
                _ts = nullptr;
            }
            return true;
        }
        default : return false;
    }
}

//此函数可能跨线程调用
bool MultiMediaSourceMuxer::isRecording(MediaSource &sender, Recorder::type type) {
    switch (type) {
        case Recorder::type_hls: return !!_hls;
        case Recorder::type_mp4: return !!_mp4;
        case Recorder::type_hls_fmp4: return !!_hls_fmp4;
        case Recorder::type_fmp4: return !!_fmp4;
        case Recorder::type_ts: return !!_ts;
        default: return false;
    }
}

void MultiMediaSourceMuxer::startSendRtp(MediaSource &sender, const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) {
#if defined(ENABLE_RTPPROXY)
    createGopCacheIfNeed();

    auto ring = _ring;
    auto ssrc = args.ssrc;
    auto ssrc_multi_send = args.ssrc_multi_send;
    auto tracks = getTracks(false);
    auto poller = getOwnerPoller(sender);
    auto rtp_sender = std::make_shared<RtpSender>(poller);

    weak_ptr<MultiMediaSourceMuxer> weak_self = shared_from_this();

    rtp_sender->startSend(args, [ssrc,ssrc_multi_send, weak_self, rtp_sender, cb, tracks, ring, poller](uint16_t local_port, const SockException &ex) mutable {
        cb(local_port, ex);
        auto strong_self = weak_self.lock();
        if (!strong_self || ex) {
            return;
        }

        for (auto &track : tracks) {
            rtp_sender->addTrack(track);
        }
        rtp_sender->addTrackCompleted();
        rtp_sender->setOnClose([weak_self, ssrc](const toolkit::SockException &ex) {
            if (auto strong_self = weak_self.lock()) {
                // 可能归属线程发生变更
                strong_self->getOwnerPoller(MediaSource::NullMediaSource())->async([=]() {
                    WarnL << "stream:" << strong_self->shortUrl() << " stop send rtp:" << ssrc << ", reason:" << ex;
                    strong_self->_rtp_sender.erase(ssrc);
                    NOTICE_EMIT(BroadcastSendRtpStoppedArgs, Broadcast::kBroadcastSendRtpStopped, *strong_self, ssrc, ex);
                });
            }
        });

        auto reader = ring->attach(poller);
        reader->setReadCB([rtp_sender](const Frame::Ptr &frame) {
            rtp_sender->inputFrame(frame);
        });

        // 可能归属线程发生变更
        strong_self->getOwnerPoller(MediaSource::NullMediaSource())->async([=]() {
            if(!ssrc_multi_send) {
                strong_self->_rtp_sender.erase(ssrc);
            }
            strong_self->_rtp_sender.emplace(ssrc,reader);
        });
    });
#else
    cb(0, SockException(Err_other, "该功能未启用，编译时请打开ENABLE_RTPPROXY宏"));
#endif//ENABLE_RTPPROXY
}

bool MultiMediaSourceMuxer::stopSendRtp(MediaSource &sender, const string &ssrc) {
#if defined(ENABLE_RTPPROXY)
    if (ssrc.empty()) {
        //关闭全部
        auto size = _rtp_sender.size();
        _rtp_sender.clear();
        return size;
    }
    //关闭特定的
    return _rtp_sender.erase(ssrc);
#else
    return false;
#endif//ENABLE_RTPPROXY
}

vector<Track::Ptr> MultiMediaSourceMuxer::getMediaTracks(MediaSource &sender, bool trackReady) const {
    return getTracks(trackReady);
}

EventPoller::Ptr MultiMediaSourceMuxer::getOwnerPoller(MediaSource &sender) {
    auto listener = getDelegate();
    if (!listener) {
        return _poller;
    }
    try {
        auto ret = listener->getOwnerPoller(sender);
        if (ret != _poller) {
            WarnL << "OwnerPoller changed " << _poller->getThreadName() << " -> " << ret->getThreadName() << " : " << shortUrl();
            _poller = ret;
            if (_paced_sender) {
                _paced_sender->resetTimer(_poller);
            }
        }
        return ret;
    } catch (MediaSourceEvent::NotImplemented &) {
        // listener未重载getOwnerPoller
        return _poller;
    }
}

std::shared_ptr<MultiMediaSourceMuxer> MultiMediaSourceMuxer::getMuxer(MediaSource &sender) const {
    return const_cast<MultiMediaSourceMuxer*>(this)->shared_from_this();
}

bool MultiMediaSourceMuxer::onTrackReady(const Track::Ptr &track) {
    auto &stamp = _stamps[track->getIndex()];
    if (_dur_sec > 0.01) {
        // 点播
        stamp.setPlayBack();
    }

    bool ret = false;
    if (_rtmp) {
        ret = _rtmp->addTrack(track) ? true : ret;
    }
    if (_rtsp) {
        ret = _rtsp->addTrack(track) ? true : ret;
    }
    if (_ts) {
        ret = _ts->addTrack(track) ? true : ret;
    }
    if (_fmp4) {
        ret = _fmp4->addTrack(track) ? true : ret;
    }
    if (_hls) {
        ret = _hls->addTrack(track) ? true : ret;
    }
    if (_hls_fmp4) {
        ret = _hls_fmp4->addTrack(track) ? true : ret;
    }
    if (_mp4) {
        ret = _mp4->addTrack(track) ? true : ret;
    }
    return ret;
}

void MultiMediaSourceMuxer::onAllTrackReady() {
    CHECK(!_create_in_poller || getOwnerPoller(MediaSource::NullMediaSource())->isCurrentThread());

    if (_option.paced_sender_ms) {
        std::weak_ptr<MultiMediaSourceMuxer> weak_self = shared_from_this();
        _paced_sender = std::make_shared<FramePacedSender>(_option.paced_sender_ms, [weak_self](const Frame::Ptr &frame) {
            if (auto strong_self = weak_self.lock()) {
                strong_self->onTrackFrame_l(frame);
            }
        });
    }

    setMediaListener(getDelegate());

    if (_rtmp) {
        _rtmp->addTrackCompleted();
    }
    if (_rtsp) {
        _rtsp->addTrackCompleted();
    }
    if (_ts) {
        _ts->addTrackCompleted();
    }
    if (_mp4) {
        _mp4->addTrackCompleted();
    }
    if (_fmp4) {
        _fmp4->addTrackCompleted();
    }
    if (_hls) {
        _hls->addTrackCompleted();
    }
    if (_hls_fmp4) {
        _hls_fmp4->addTrackCompleted();
    }

    auto listener = _track_listener.lock();
    if (listener) {
        listener->onAllTrackReady();
    }

#if defined(ENABLE_RTPPROXY)
    GET_CONFIG(bool, gop_cache, RtpProxy::kGopCache);
    if (gop_cache) {
        createGopCacheIfNeed();
    }
#endif

    Stamp *first = nullptr;
    for (auto &pr : _stamps) {
        if (!first) {
            first = &pr.second;
        } else {
            pr.second.syncTo(*first);
        }
    }
    InfoL << "stream: " << shortUrl() << " , codec info: " << getTrackInfoStr(this);
}

void MultiMediaSourceMuxer::createGopCacheIfNeed() {
    if (_ring) {
        return;
    }
    weak_ptr<MultiMediaSourceMuxer> weak_self = shared_from_this();
    auto src = std::make_shared<MediaSourceForMuxer>(weak_self.lock());
    _ring = std::make_shared<RingType>(1024, [weak_self, src](int size) {
        if (auto strong_self = weak_self.lock()) {
            // 切换到归属线程
            strong_self->getOwnerPoller(MediaSource::NullMediaSource())->async([=]() {
                strong_self->onReaderChanged(*src, strong_self->totalReaderCount());
            });
        }
    });
}

void MultiMediaSourceMuxer::resetTracks() {
    MediaSink::resetTracks();

    if (_rtmp) {
        _rtmp->resetTracks();
    }
    if (_rtsp) {
        _rtsp->resetTracks();
    }
    if (_ts) {
        _ts->resetTracks();
    }
    if (_fmp4) {
        _fmp4->resetTracks();
    }
    if (_hls_fmp4) {
        _hls_fmp4->resetTracks();
    }
    if (_hls) {
        _hls->resetTracks();
    }
    if (_mp4) {
        _mp4->resetTracks();
    }
}

bool MultiMediaSourceMuxer::onTrackFrame(const Frame::Ptr &frame_in) {
    auto frame = frame_in;
    if (_option.modify_stamp != ProtocolOption::kModifyStampOff) {
        // 时间戳不采用原始的绝对时间戳
        frame = std::make_shared<FrameStamp>(frame, _stamps[frame->getIndex()], _option.modify_stamp);
    }
    return _paced_sender ? _paced_sender->inputFrame(frame) : onTrackFrame_l(frame);
}

bool MultiMediaSourceMuxer::onTrackFrame_l(const Frame::Ptr &frame_in) {
    auto frame = frame_in;
    bool ret = false;
    if (_rtmp) {
        ret = _rtmp->inputFrame(frame) ? true : ret;
    }
    if (_rtsp) {
        ret = _rtsp->inputFrame(frame) ? true : ret;
    }
    if (_ts) {
        ret = _ts->inputFrame(frame) ? true : ret;
    }

    if (_hls) {
        ret = _hls->inputFrame(frame) ? true : ret;
    }

    if (_hls_fmp4) {
        ret = _hls_fmp4->inputFrame(frame) ? true : ret;
    }

    if (_mp4) {
        ret = _mp4->inputFrame(frame) ? true : ret;
    }
    if (_fmp4) {
        ret = _fmp4->inputFrame(frame) ? true : ret;
    }
    if (_ring) {
        // 此场景由于直接转发，可能存在切换线程引起的数据被缓存在管道，所以需要CacheAbleFrame
        frame = Frame::getCacheAbleFrame(frame);
        if (frame->getTrackType() == TrackVideo) {
            // 视频时，遇到第一帧配置帧或关键帧则标记为gop开始处
            auto video_key_pos = frame->keyFrame() || frame->configFrame();
            _ring->write(frame, video_key_pos && !_video_key_pos);
            if (!frame->dropAble()) {
                _video_key_pos = video_key_pos;
            }
        } else {
            // 没有视频时，设置is_key为true，目的是关闭gop缓存
            _ring->write(frame, !haveVideo());
        }
    }
    return ret;
}

bool MultiMediaSourceMuxer::isEnabled(){
    GET_CONFIG(uint32_t, stream_none_reader_delay_ms, General::kStreamNoneReaderDelayMS);
    if (!_is_enable || _last_check.elapsedTime() > stream_none_reader_delay_ms) {
        //无人观看时，每次检查是否真的无人观看
        //有人观看时，则延迟一定时间检查一遍是否无人观看了(节省性能)
        _is_enable = (_rtmp ? _rtmp->isEnabled() : false) ||
                     (_rtsp ? _rtsp->isEnabled() : false) ||
                     (_ts ? _ts->isEnabled() : false) ||
                     (_fmp4 ? _fmp4->isEnabled() : false) ||
                     (_ring ? (bool)_ring->readerCount() : false)  ||
                     (_hls ? _hls->isEnabled() : false) ||
                     (_hls_fmp4 ? _hls_fmp4->isEnabled() : false) ||
                     _mp4;

        if (_is_enable) {
            //无人观看时，不刷新计时器,因为无人观看时每次都会检查一遍，所以刷新计数器无意义且浪费cpu
            _last_check.resetTime();
        }
    }
    return _is_enable;
}

}//namespace mediakit
