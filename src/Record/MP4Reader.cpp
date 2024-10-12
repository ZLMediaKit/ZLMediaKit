/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifdef ENABLE_MP4

#include "MP4Reader.h"
#include "Common/config.h"
#include "Thread/WorkThreadPool.h"
#include "Util/File.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

MP4Reader::MP4Reader(const MediaTuple &tuple, const string &file_path,
                     toolkit::EventPoller::Ptr poller) {
    ProtocolOption option;
    // 读取mp4文件并流化时，不重复生成mp4/hls文件  [AUTO-TRANSLATED:5d414546]
    // Read mp4 file and stream it, do not regenerate mp4/hls file repeatedly
    option.enable_mp4 = false;
    option.enable_hls = false;
    option.enable_hls_fmp4 = false;
    // mp4支持多track  [AUTO-TRANSLATED:b9688762]
    // mp4 supports multiple tracks
    option.max_track = 16;
    setup(tuple, file_path, option, std::move(poller));
}

MP4Reader::MP4Reader(const MediaTuple &tuple, const string &file_path, const ProtocolOption &option, toolkit::EventPoller::Ptr poller) {
    setup(tuple, file_path, option, std::move(poller));
}

void MP4Reader::setup(const MediaTuple &tuple, const std::string &file_path, const ProtocolOption &option, toolkit::EventPoller::Ptr poller) {
    // 读写文件建议放在后台线程  [AUTO-TRANSLATED:6f09ef53]
    // It is recommended to read and write files in the background thread
    _poller = poller ? std::move(poller) : WorkThreadPool::Instance().getPoller();
    _file_path = file_path;
    if (_file_path.empty()) {
        GET_CONFIG(string, recordPath, Protocol::kMP4SavePath);
        GET_CONFIG(bool, enableVhost, General::kEnableVhost);
        if (enableVhost) {
            _file_path = tuple.shortUrl();
        } else {
            _file_path = tuple.app + "/" + tuple.stream;
        }
        _file_path = File::absolutePath(_file_path, recordPath);
    }

    _demuxer = std::make_shared<MultiMP4Demuxer>();
    _demuxer->openMP4(_file_path);

    if (tuple.stream.empty()) {
        return;
    }

    _muxer = std::make_shared<MultiMediaSourceMuxer>(tuple, _demuxer->getDurationMS() / 1000.0f, option);
    auto tracks = _demuxer->getTracks(false);
    if (tracks.empty()) {
        throw std::runtime_error(StrPrinter << "该mp4文件没有有效的track:" << _file_path);
    }
    for (auto &track : tracks) {
        _muxer->addTrack(track);
        if (track->getTrackType() == TrackVideo) {
            _have_video = true;
        }
    }
    // 添加完毕所有track，防止单track情况下最大等待3秒  [AUTO-TRANSLATED:445e3403]
    // After all tracks are added, prevent the maximum waiting time of 3 seconds in the case of a single track
    _muxer->addTrackCompleted();
}

bool MP4Reader::readSample() {
    if (_paused) {
        // 确保暂停时，时间轴不走动  [AUTO-TRANSLATED:3d38dd31]
        // Ensure that the timeline does not move when paused
        _seek_ticker.resetTime();
        return true;
    }

    bool keyFrame = false;
    bool eof = false;
    while (!eof && _last_dts < getCurrentStamp()) {
        auto frame = _demuxer->readFrame(keyFrame, eof);
        if (!frame) {
            continue;
        }
        _last_dts = frame->dts();
        if (_muxer) {
            _muxer->inputFrame(frame);
        }
    }

    GET_CONFIG(bool, file_repeat, Record::kFileRepeat);
    if (eof && (file_repeat || _file_repeat)) {
        // 需要从头开始看  [AUTO-TRANSLATED:5b563a35]
        // Need to start from the beginning
        seekTo(0);
        return true;
    }

    return !eof;
}

bool MP4Reader::readNextSample() {
    bool keyFrame = false;
    bool eof = false;
    auto frame = _demuxer->readFrame(keyFrame, eof);
    if (!frame) {
        return false;
    }
    if (_muxer) {
        _muxer->inputFrame(frame);
    }
    setCurrentStamp(frame->dts());
    return true;
}

void MP4Reader::stopReadMP4() {
    _timer = nullptr;
}

void MP4Reader::startReadMP4(uint64_t sample_ms, bool ref_self, bool file_repeat) {
    GET_CONFIG(uint32_t, sampleMS, Record::kSampleMS);
    setCurrentStamp(0);
    auto strong_self = shared_from_this();
    if (_muxer) {
        // 一直读到所有track就绪为止  [AUTO-TRANSLATED:410f9ecc]
        // Keep reading until all tracks are ready
        while (!_muxer->isAllTrackReady() && readNextSample());
        // 注册后再切换OwnerPoller  [AUTO-TRANSLATED:4a483e23]
        // Register and then switch OwnerPoller
        _muxer->setMediaListener(strong_self);
    }

    auto timer_sec = (sample_ms ? sample_ms : sampleMS) / 1000.0f;

    // 启动定时器  [AUTO-TRANSLATED:0b93ed77]
    // Start the timer
    if (ref_self) {
        _timer = std::make_shared<Timer>(timer_sec, [strong_self]() {
            lock_guard<recursive_mutex> lck(strong_self->_mtx);
            return strong_self->readSample();
        }, _poller);
    } else {
        weak_ptr<MP4Reader> weak_self = strong_self;
        _timer = std::make_shared<Timer>(timer_sec, [weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return false;
            }
            lock_guard<recursive_mutex> lck(strong_self->_mtx);
            return strong_self->readSample();
        }, _poller);
    }

    _file_repeat = file_repeat;
}

const MultiMP4Demuxer::Ptr &MP4Reader::getDemuxer() const {
    return _demuxer;
}

uint32_t MP4Reader::getCurrentStamp() {
    return (uint32_t) (_seek_to + !_paused * _speed * _seek_ticker.elapsedTime());
}

void MP4Reader::setCurrentStamp(uint32_t new_stamp) {
    auto old_stamp = getCurrentStamp();
    _seek_to = new_stamp;
    _last_dts = new_stamp;
    _seek_ticker.resetTime();
    if (old_stamp != new_stamp && _muxer) {
        // 时间轴未拖动时不操作  [AUTO-TRANSLATED:c5b53103]
        // Do not operate when the timeline is not dragged
        _muxer->setTimeStamp(new_stamp);
    }
}

bool MP4Reader::seekTo(MediaSource &sender, uint32_t stamp) {
    // 拖动进度条后应该恢复播放  [AUTO-TRANSLATED:8a6d11f7]
    // Playback should resume after dragging the progress bar
    pause(sender, false);
    TraceL << getOriginUrl(sender) << ",stamp:" << stamp;
    return seekTo(stamp);
}

bool MP4Reader::pause(MediaSource &sender, bool pause) {
    if (_paused == pause) {
        return true;
    }
    // _seek_ticker重新计时，不管是暂停还是seek都不影响总的播放进度  [AUTO-TRANSLATED:96051076]
    // _seek_ticker restarts the timer, whether it is paused or seek does not affect the total playback progress
    setCurrentStamp(getCurrentStamp());
    _paused = pause;
    TraceL << getOriginUrl(sender) << ",pause:" << pause;
    return true;
}

bool MP4Reader::speed(MediaSource &sender, float speed) {
    if (speed < 0.1 || speed > 20) {
        WarnL << "播放速度取值范围非法:" << speed;
        return false;
    }
    // _seek_ticker重置，赋值_seek_to  [AUTO-TRANSLATED:b30a3f06]
    // _seek_ticker reset, assign _seek_to
    setCurrentStamp(getCurrentStamp());
    // 设置播放速度后应该恢复播放  [AUTO-TRANSLATED:851fcde9]
    // Playback should resume after setting the playback speed
    _paused = false;
    if (_speed == speed) {
        return true;
    }
    _speed = speed;
    TraceL << getOriginUrl(sender) << ",speed:" << speed;
    return true;
}

bool MP4Reader::seekTo(uint32_t stamp_seek) {
    lock_guard<recursive_mutex> lck(_mtx);
    if (stamp_seek > _demuxer->getDurationMS()) {
        // 超过文件长度  [AUTO-TRANSLATED:b4361054]
        // Exceeds the file length
        return false;
    }
    auto stamp = _demuxer->seekTo(stamp_seek);
    if (stamp == -1) {
        // seek失败  [AUTO-TRANSLATED:88cc8444]
        // Seek failed
        return false;
    }

    if (!_have_video) {
        // 没有视频，不需要搜索关键帧；设置当前时间戳  [AUTO-TRANSLATED:82f87f21]
        // There is no video, no need to search for keyframes; set the current timestamp
        setCurrentStamp((uint32_t) stamp);
        return true;
    }
    // 搜索到下一帧关键帧  [AUTO-TRANSLATED:aa2ec689]
    // Search for the next keyframe
    bool keyFrame = false;
    bool eof = false;
    while (!eof) {
        auto frame = _demuxer->readFrame(keyFrame, eof);
        if (!frame) {
            // 文件读完了都未找到下一帧关键帧  [AUTO-TRANSLATED:49a8d3a7]
            // The file has been read but the next keyframe has not been found
            continue;
        }
        if (keyFrame || frame->keyFrame() || frame->configFrame()) {
            // 定位到key帧  [AUTO-TRANSLATED:0300901d]
            // Locate to the keyframe
            if (_muxer) {
                _muxer->inputFrame(frame);
            }
            // 设置当前时间戳  [AUTO-TRANSLATED:88949974]
            // Set the current timestamp
            setCurrentStamp(frame->dts());
            return true;
        }
    }
    return false;
}

bool MP4Reader::close(MediaSource &sender) {
    _timer = nullptr;
    WarnL << "close media: " << sender.getUrl();
    return true;
}

MediaOriginType MP4Reader::getOriginType(MediaSource &sender) const {
    return MediaOriginType::mp4_vod;
}

string MP4Reader::getOriginUrl(MediaSource &sender) const {
    return _file_path;
}

toolkit::EventPoller::Ptr MP4Reader::getOwnerPoller(MediaSource &sender) {
    return _poller;
}

} /* namespace mediakit */
#endif //ENABLE_MP4