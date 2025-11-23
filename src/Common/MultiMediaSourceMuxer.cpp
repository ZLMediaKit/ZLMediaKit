/*
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
#include "Thread/WorkThreadPool.h"

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
    // 最小缓存100ms数据  [AUTO-TRANSLATED:7b2fcb0d]
    // Minimum cache 100ms data
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
        auto &last_dts = _last_dts[frame->getTrackType()];
        if (last_dts > frame->dts()) {
            // 时间戳回退了，点播流？
            WarnL << "Dts decrease: " << last_dts << "->" << frame->dts() << ", flush all paced sender cache: " << _cache.size();
            flushCache(frame->dts());
        }
        _cache.emplace(frame->dts(), Frame::getCacheAbleFrame(frame));
        last_dts = frame->dts();
        return true;
    }

private:
    void onTick() {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto max_dts = _cache.empty() ? 0 : _cache.rbegin()->first;
        while (!_cache.empty()) {
            auto front = _cache.begin();
            if (getCurrentStamp() < front->first + _cache_ms) {
                // 还没到消费时间  [AUTO-TRANSLATED:09fb4c3d]
                // Not yet time to consume
                break;
            }
            // 时间到了，该消费frame了  [AUTO-TRANSLATED:2f007931]
            // Time is up, it's time to consume the frame
            _cb(front->second);
            _cache.erase(front);
        }

        if (_cache.empty() && max_dts) {
            // 消费太快，需要增加缓存大小  [AUTO-TRANSLATED:c05bfbcd]
            // Consumption is too fast, need to increase cache size
            setCurrentStamp(max_dts);
            _cache_ms += kMinCacheMS;
        }

        // 消费太慢，需要强制flush数据  [AUTO-TRANSLATED:5613625e]
        // Consumption is too slow, need to force flush data
        if (_cache.size() > 25 * 5) {
            WarnL << "Flush frame paced sender cache: " << _cache.size();
            flushCache(max_dts);
        }
    }

    void flushCache(uint64_t dts) {
        while (!_cache.empty()) {
            auto front = _cache.begin();
            _cb(front->second);
            _cache.erase(front);
        }
        setCurrentStamp(dts);
        _cache_ms = kMinCacheMS;
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
    uint64_t _last_dts[2] = {0, 0};
    OnFrame _cb;
    Ticker _ticker;
    Timer::Ptr _timer;
    std::recursive_mutex _mtx;
    std::multimap<uint64_t, Frame::Ptr> _cache;
};

std::shared_ptr<MediaSinkInterface> MultiMediaSourceMuxer::makeRecorder(Recorder::type type) {
    auto recorder = Recorder::createRecorder(type, getMediaTuple(), _option);
    for (auto &track : getTracks()) {
        recorder->addTrack(track);
    }
    recorder->addTrackCompleted();
    if (_ring) {
        _ring->flushGop([&](const Frame::Ptr &frame) {
            recorder->inputFrame(frame);
        });
    }
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
    return codec_info;
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
#if defined(ENABLE_RTPPROXY)
void MultiMediaSourceMuxer::forEachRtpSender(const std::function<void(const std::string &ssrc, const RtpSender &sender)> &cb) const {
    for (auto &pr : _rtp_sender) {
        auto sender = std::get<1>(pr.second).lock();
        if (sender) {
            cb(pr.first, *sender);
        }
    }
}
#endif // ENABLE_RTPPROXY
MultiMediaSourceMuxer::MultiMediaSourceMuxer(const MediaTuple& tuple, float dur_sec, const ProtocolOption &option): _tuple(tuple) {
    if (!option.stream_replace.empty()) {
        // 支持在on_publish hook中替换stream_id  [AUTO-TRANSLATED:375eb2ff]
        // Support replacing stream_id in on_publish hook
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

    // 音频相关设置  [AUTO-TRANSLATED:6ee58d57]
    // Audio related settings
    enableAudio(option.enable_audio);
    enableMuteAudio(option.add_mute_audio);
}

void MultiMediaSourceMuxer::setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener) {
    setDelegate(listener);

    auto self = shared_from_this();
    // 拦截事件  [AUTO-TRANSLATED:100ca068]
    // Intercept events
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
        // listener未重载totalReaderCount  [AUTO-TRANSLATED:f098007e]
        // Listener did not reload totalReaderCount
        return totalReaderCount();
    }
}

// 此函数可能跨线程调用  [AUTO-TRANSLATED:e8c5f74d]
// This function may be called across threads
bool MultiMediaSourceMuxer::setupRecord(Recorder::type type, bool start, const string &custom_path, size_t max_second) {
    CHECK(getOwnerPoller(MediaSource::NullMediaSource())->isCurrentThread(), "Can only call setupRecord in it's owner poller");
    onceToken token(nullptr, [&]() {
        if (_option.mp4_as_player && type == Recorder::type_mp4) {
            // 开启关闭mp4录制，触发观看人数变化相关事件  [AUTO-TRANSLATED:b63a8deb]
            // Turn on/off mp4 recording, trigger events related to changes in the number of viewers
            onReaderChanged(MediaSource::NullMediaSource(), totalReaderCount());
        }
    });
    switch (type) {
        case Recorder::type_hls : {
            if (start && !_hls) {
                // 开始录制  [AUTO-TRANSLATED:36d99250]
                // Start recording
                _option.hls_save_path = custom_path;
                auto hls = dynamic_pointer_cast<HlsRecorder>(makeRecorder(type));
                if (hls) {
                    // 设置HlsMediaSource的事件监听器  [AUTO-TRANSLATED:69990c92]
                    // Set the event listener for HlsMediaSource
                    hls->setListener(shared_from_this());
                }
                _hls = hls;
            } else if (!start && _hls) {
                // 停止录制  [AUTO-TRANSLATED:3dee9292]
                // Stop recording
                _hls = nullptr;
            }
            return true;
        }
        case Recorder::type_mp4 : {
            if (start && !_mp4) {
                // 开始录制  [AUTO-TRANSLATED:36d99250]
                // Start recording
                _option.mp4_save_path = custom_path;
                _option.mp4_max_second = max_second;
                _mp4 = makeRecorder(type);
            } else if (!start && _mp4) {
                // 停止录制  [AUTO-TRANSLATED:3dee9292]
                // Stop recording
                _mp4 = nullptr;
            }
            return true;
        }
        case Recorder::type_hls_fmp4: {
            if (start && !_hls_fmp4) {
                // 开始录制  [AUTO-TRANSLATED:36d99250]
                // Start recording
                _option.hls_save_path = custom_path;
                auto hls = dynamic_pointer_cast<HlsFMP4Recorder>(makeRecorder(type));
                if (hls) {
                    // 设置HlsMediaSource的事件监听器  [AUTO-TRANSLATED:69990c92]
                    // Set the event listener for HlsMediaSource
                    hls->setListener(shared_from_this());
                }
                _hls_fmp4 = hls;
            } else if (!start && _hls_fmp4) {
                // 停止录制  [AUTO-TRANSLATED:3dee9292]
                // Stop recording
                _hls_fmp4 = nullptr;
            }
            return true;
        }
        case Recorder::type_fmp4: {
            if (start && !_fmp4) {
                auto fmp4 = dynamic_pointer_cast<FMP4MediaSourceMuxer>(makeRecorder(type));
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
                auto ts = dynamic_pointer_cast<TSMediaSourceMuxer>(makeRecorder(type));
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

std::string MultiMediaSourceMuxer::startRecord(const std::string &file_path, uint32_t back_time_ms, uint32_t forward_time_ms) {
#if !defined(ENABLE_MP4)
    throw std::invalid_argument("mp4相关功能未打开，请开启ENABLE_MP4宏后编译再测试");
#else
    if (!_ring) {
        throw std::runtime_error("frame gop cache disabled, start record event video failed");
    }
    auto path = Recorder::getRecordPath(Recorder::type_mp4, _tuple, _option.mp4_save_path);
    path += file_path;
    TraceL << "mp4 save path: " << path;

    auto muxer = std::make_shared<MP4Muxer>();
    muxer->openMP4(path);
    for (auto &track : MediaSink::getTracks()) {
        muxer->addTrack(track);
    }
    muxer->addTrackCompleted();

    std::list<Frame::Ptr> history;
    _ring->flushGop([&](const Frame::Ptr &frame) { history.emplace_back(frame); });
    if (!history.empty()) {
        auto now_dts = history.back()->dts();

        decltype(history)::iterator pos = history.end();
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            auto &frame = *it;
            if (frame->getTrackType() != TrackVideo || (!frame->configFrame() && !frame->keyFrame())) {
                continue;
            }
            // 如果视频关键帧到末尾的时长超过一定的时间，那前面的数据应该全部删除
            if (frame->dts() + back_time_ms < now_dts) {
                pos = it.base();
                --pos;
                break;
            }
        }
        if (pos != history.end()) {
            // 移除前面过多的数据
            TraceL << "clear history video: " << history.front()->dts() << " -> " << (*pos)->dts();
            history.erase(history.begin(), pos);
        }
        if (!history.empty()) {
            auto &front = history.front();
            InfoL << "start record: " << path
                  << ", start_dts: " << front->dts() << ", key_frame: " << front->keyFrame() << ", config_frame: " << front->configFrame()
                  << ", now_dts: " << now_dts;
        }

        for (auto &frame : history) {
            muxer->inputFrame(frame);
        }
    }

    auto reader = _ring->attach(MultiMediaSourceMuxer::getOwnerPoller(MediaSource::NullMediaSource()), false);
    uint64_t now_dts = 0;
    int selected_index = -1;
    Ticker ticker;
    bool is_live_stream = _dur_sec < 0.01;
    reader->setReadCB([muxer, now_dts, selected_index, forward_time_ms, reader, path, ticker, is_live_stream](const Frame::Ptr &frame) mutable {
        // 循环引用自身
        if (!now_dts) {
            now_dts = frame->dts();
            selected_index = frame->getIndex();
        }
        // 新增兜底机制，如果直播录制任务时长超过预期时间3秒，不管数据时间戳是否增长是否达到预期，都强制停止录制
        if ((frame->getIndex() == selected_index && now_dts + forward_time_ms < frame->dts()) || (is_live_stream && ticker.createdTime() > forward_time_ms + 3000)) {
            InfoL << "stop record: " << path << ", end dts: " << frame->dts();
            WorkThreadPool::Instance().getPoller()->async([muxer]() { muxer->closeMP4(); });
            reader = nullptr;
            return;
        }
        muxer->inputFrame(frame);
    });
    std::weak_ptr<RingType::RingReader> weak_reader = reader;
    reader->setDetachCB([weak_reader]() {
        if (auto strong_reader = weak_reader.lock()) {
            // 防止循环引用
            strong_reader->setReadCB(nullptr);
        }
    });

    return path;
#endif
}

// 此函数可能跨线程调用  [AUTO-TRANSLATED:e8c5f74d]
// This function may be called across threads
bool MultiMediaSourceMuxer::isRecording(Recorder::type type) {
    switch (type) {
        case Recorder::type_hls: return !!_hls;
        case Recorder::type_mp4: return !!_mp4;
        case Recorder::type_hls_fmp4: return !!_hls_fmp4;
        case Recorder::type_fmp4: return !!_fmp4;
        case Recorder::type_ts: return !!_ts;
        default: return false;
    }
}

void MultiMediaSourceMuxer::startSendRtp(const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) {
#if defined(ENABLE_RTPPROXY)
    createGopCacheIfNeed(1);

    auto ring = _ring;
    auto ssrc = args.ssrc;
    auto ssrc_multi_send = args.ssrc_multi_send;
    auto tracks = getTracks(false);
    auto poller = getOwnerPoller(MediaSource::NullMediaSource());
    auto rtp_sender = std::make_shared<RtpSender>(poller);

    weak_ptr<MultiMediaSourceMuxer> weak_self = shared_from_this();

    rtp_sender->setOnClose([weak_self, ssrc](const toolkit::SockException &ex) {
        if (auto strong_self = weak_self.lock()) {
            // 可能归属线程发生变更  [AUTO-TRANSLATED:2b379e30]
            // The owning thread may change
            strong_self->getOwnerPoller(MediaSource::NullMediaSource())->async([=]() {
                WarnL << "stream:" << strong_self->shortUrl() << " stop send rtp:" << ssrc << ", reason:" << ex;
                strong_self->_rtp_sender.erase(ssrc);
                NOTICE_EMIT(BroadcastSendRtpStoppedArgs, Broadcast::kBroadcastSendRtpStopped, *strong_self, ssrc, ex);
            });
        }
    });

    rtp_sender->startSend(*this, args, [ssrc,ssrc_multi_send, weak_self, rtp_sender, cb, tracks, ring, poller](uint16_t local_port, const SockException &ex) mutable {
        cb(local_port, ex);
        auto strong_self = weak_self.lock();
        if (!strong_self || ex) {
            return;
        }

        for (auto &track : tracks) {
            rtp_sender->addTrack(track);
        }
        rtp_sender->addTrackCompleted();

        auto reader = ring->attach(poller);
        reader->setReadCB([rtp_sender](const Frame::Ptr &frame) {
            rtp_sender->inputFrame(frame);
        });

        // 可能归属线程发生变更  [AUTO-TRANSLATED:2b379e30]
        // The owning thread may change
        strong_self->getOwnerPoller(MediaSource::NullMediaSource())->async([=]() {
            if (!ssrc_multi_send) {
                strong_self->_rtp_sender.erase(ssrc);
            }
            std::weak_ptr<RtpSender> sender = rtp_sender;
            strong_self->_rtp_sender.emplace(ssrc, make_tuple(reader, sender));
        });
    });
#else
    cb(0, SockException(Err_other, "该功能未启用，编译时请打开ENABLE_RTPPROXY宏"));
#endif//ENABLE_RTPPROXY
}

bool MultiMediaSourceMuxer::stopSendRtp(const string &ssrc) {
#if defined(ENABLE_RTPPROXY)
    if (ssrc.empty()) {
        // 关闭全部  [AUTO-TRANSLATED:ffaadfda]
        // Close all
        auto size = _rtp_sender.size();
        _rtp_sender.clear();
        return size;
    }
    // 关闭特定的  [AUTO-TRANSLATED:2286322a]
    // Close specific
    return _rtp_sender.erase(ssrc);
#else
    return false;
#endif//ENABLE_RTPPROXY
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
        // listener未重载getOwnerPoller  [AUTO-TRANSLATED:0ebf2e53]
        // Listener did not reload getOwnerPoller
        return _poller;
    }
}

bool MultiMediaSourceMuxer::close(MediaSource &sender) {
    MediaSourceEventInterceptor::close(sender);
    _rtmp = nullptr;
    _rtsp = nullptr;
    _fmp4 = nullptr;
    _ts = nullptr;
    _mp4 = nullptr;
    _hls = nullptr;
    _hls_fmp4 = nullptr;
#if defined(ENABLE_RTPPROXY)
    _rtp_sender.clear();
#endif // ENABLE_RTPPROXY
    return true;
}

std::shared_ptr<MultiMediaSourceMuxer> MultiMediaSourceMuxer::getMuxer(MediaSource &sender) const {
    return const_cast<MultiMediaSourceMuxer*>(this)->shared_from_this();
}

bool MultiMediaSourceMuxer::onTrackReady(const Track::Ptr &track) {
    auto &stamp = _stamps[track->getIndex()];
    if (_dur_sec > 0.01) {
        // 点播  [AUTO-TRANSLATED:f0b0f74a]
        // On-demand
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
    GET_CONFIG(size_t, gop_cache, RtpProxy::kGopCache);
    if (gop_cache > 0) {
        createGopCacheIfNeed(gop_cache);
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

void MultiMediaSourceMuxer::createGopCacheIfNeed(size_t gop_count) {
    if (_ring) {
        return;
    }
    weak_ptr<MultiMediaSourceMuxer> weak_self = shared_from_this();
    auto src = std::make_shared<MediaSourceForMuxer>(weak_self.lock());
    _ring = std::make_shared<RingType>(1024, [weak_self, src](int size) {
        if (auto strong_self = weak_self.lock()) {
            // 切换到归属线程  [AUTO-TRANSLATED:abcf859b]
            // Switch to the owning thread
            strong_self->getOwnerPoller(MediaSource::NullMediaSource())->async([=]() {
                strong_self->onReaderChanged(*src, strong_self->totalReaderCount());
            });
        }
    }, gop_count);
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
        // 时间戳不采用原始的绝对时间戳  [AUTO-TRANSLATED:8beb3bf7]
        // Timestamp does not use the original absolute timestamp
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
        // 此场景由于直接转发，可能存在切换线程引起的数据被缓存在管道，所以需要CacheAbleFrame  [AUTO-TRANSLATED:528afbb7]
        // In this scenario, due to direct forwarding, there may be data cached in the pipeline due to thread switching, so CacheAbleFrame is needed
        frame = Frame::getCacheAbleFrame(frame);
        if (frame->getTrackType() == TrackVideo) {
            // 视频时，遇到第一帧配置帧或关键帧则标记为gop开始处  [AUTO-TRANSLATED:66247aa8]
            // When it is a video, if the first frame configuration frame or key frame is encountered, it is marked as the beginning of the GOP
            auto video_key_pos = frame->keyFrame() || frame->configFrame();
            _ring->write(frame, video_key_pos && !_video_key_pos);
            if (!frame->dropAble()) {
                _video_key_pos = video_key_pos;
            }
        } else {
            // 没有视频时，设置is_key为true，目的是关闭gop缓存  [AUTO-TRANSLATED:f3223755]
            // When there is no video, set is_key to true to disable gop caching
            _ring->write(frame, !haveVideo());
        }
    }
    return ret;
}

bool MultiMediaSourceMuxer::isEnabled(){
    GET_CONFIG(uint32_t, stream_none_reader_delay_ms, General::kStreamNoneReaderDelayMS);
    if (!_is_enable || _last_check.elapsedTime() > stream_none_reader_delay_ms) {
        // 无人观看时，每次检查是否真的无人观看  [AUTO-TRANSLATED:48bc59c6]
        // When no one is watching, check each time if there is really no one watching
        // 有人观看时，则延迟一定时间检查一遍是否无人观看了(节省性能)  [AUTO-TRANSLATED:a7dfddc4]
        // When someone is watching, check again after a certain delay to see if no one is watching (save performance)
        _is_enable = (_rtmp ? _rtmp->isEnabled() : false) ||
                     (_rtsp ? _rtsp->isEnabled() : false) ||
                     (_ts ? _ts->isEnabled() : false) ||
                     (_fmp4 ? _fmp4->isEnabled() : false) ||
                     (_ring ? (bool)_ring->readerCount() : false)  ||
                     (_hls ? _hls->isEnabled() : false) ||
                     (_hls_fmp4 ? _hls_fmp4->isEnabled() : false) ||
                     _mp4;

        if (_is_enable) {
            // 无人观看时，不刷新计时器,因为无人观看时每次都会检查一遍，所以刷新计数器无意义且浪费cpu  [AUTO-TRANSLATED:03ab47cf]
            // When no one is watching, do not refresh the timer, because each time no one is watching, it will be checked, so refreshing the counter is meaningless and wastes cpu
            _last_check.resetTime();
        }
    }
    return _is_enable;
}

}//namespace mediakit
