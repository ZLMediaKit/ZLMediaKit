﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "MediaSink.h"
#include "Common/config.h"
#include "Extension/Factory.h"

#define MUTE_AUDIO_INDEX 0xFFFF

using namespace std;

namespace mediakit{

bool MediaSink::addTrack(const Track::Ptr &track_in) {
    if (_only_audio && track_in->getTrackType() != TrackAudio) {
        InfoL << "Only audio enabled, track ignored: " << track_in->getCodecName();
        return false;
    }
    if (!_enable_audio) {
        // 关闭音频时，加快单视频流注册速度
        if (track_in->getTrackType() == TrackAudio) {
            // 音频被全局忽略
            InfoL << "Audio disabled, audio track ignored";
            return false;
        }
    }
    if (_all_track_ready) {
        WarnL << "All track is ready, add track too late: " << track_in->getCodecName();
        return false;
    }
    // 克隆Track，只拷贝其数据，不拷贝其数据转发关系
    auto track = track_in->clone();
    CHECK(track, "Clone track failed: ", track_in->getCodecName());
    auto index = track->getIndex();
    if (!_track_map.emplace(index, std::make_pair(track, false)).second) {
        WarnL << "Already add a same track: " << track->getIndex() << ", codec: " << track->getCodecName();
        return false;
    }
    _ticker.resetTime();
    _audio_add = track->getTrackType() == TrackAudio ? true : _audio_add;
    _track_ready_callback[index] = [this, track]() { onTrackReady(track); };

    track->addDelegate([this](const Frame::Ptr &frame) {
        if (_all_track_ready) {
            return onTrackFrame(frame);
        }
        auto &frame_unread = _frame_unread[frame->getIndex()];

        GET_CONFIG(uint32_t, kMaxUnreadyFrame, General::kUnreadyFrameCache);
        if (frame_unread.size() > kMaxUnreadyFrame) {
            // 未就绪的的track，不能缓存太多的帧，否则可能内存溢出
            frame_unread.clear();
            WarnL << "Cached frame of unready track(" << frame->getCodecName() << ") is too much, now cleared";
        }
        // 还有Track未就绪，先缓存之
        frame_unread.emplace_back(Frame::getCacheAbleFrame(frame));
        return true;
    });
    return true;
}

void MediaSink::resetTracks() {
    _audio_add = false;
    _have_video = false;
    _all_track_ready = false;
    _mute_audio_maker = nullptr;
    _ticker.resetTime();
    _track_map.clear();
    _frame_unread.clear();
    _track_ready_callback.clear();
}

bool MediaSink::inputFrame(const Frame::Ptr &frame) {
    auto it = _track_map.find(frame->getIndex());
    if (it == _track_map.end()) {
        return false;
    }
    // got frame
    it->second.second = true;
    auto ret = it->second.first->inputFrame(frame);
    if (_mute_audio_maker && frame->getTrackType() == TrackVideo) {
        // 视频驱动产生静音音频
        _mute_audio_maker->inputFrame(frame);
    }
    checkTrackIfReady();
    return ret;
}

void MediaSink::checkTrackIfReady() {
    if (!_all_track_ready && !_track_ready_callback.empty()) {
        for (auto &pr : _track_map) {
            if (pr.second.second && pr.second.first->ready()) {
                // Track由未就绪状态转换成就绪状态，我们就触发onTrackReady回调
                auto it = _track_ready_callback.find(pr.first);
                if (it != _track_ready_callback.end()) {
                    it->second();
                    _track_ready_callback.erase(it);
                }
            }
        }
    }

    if (!_all_track_ready) {
        GET_CONFIG(uint32_t, kMaxWaitReadyMS, General::kWaitTrackReadyMS);
        if (_ticker.elapsedTime() > kMaxWaitReadyMS) {
            // 如果超过规定时间，那么不再等待并忽略未准备好的Track
            emitAllTrackReady();
            return;
        }

        if (!_track_ready_callback.empty()) {
            // 在超时时间内，如果存在未准备好的Track，那么继续等待
            return;
        }

        if (_only_audio && _audio_add) {
            // 只开启音频
            emitAllTrackReady();
            return;
        }

        if (_track_map.size() == _max_track_size) {
            // 如果已经添加了音视频Track，并且不存在未准备好的Track，那么说明所有Track都准备好了
            emitAllTrackReady();
            return;
        }

        GET_CONFIG(uint32_t, kMaxAddTrackMS, General::kWaitAddTrackMS);
        if (_track_map.size() == 1 && (_ticker.elapsedTime() > kMaxAddTrackMS || !_enable_audio)) {
            // 如果只有一个Track，那么在该Track添加后，我们最多还等待若干时间(可能后面还会添加Track)
            emitAllTrackReady();
            return;
        }
    }
}

void MediaSink::addTrackCompleted() {
    setMaxTrackCount(_track_map.size());
}

void MediaSink::setMaxTrackCount(size_t i) {
    if (_all_track_ready) {
        WarnL << "All track is ready, set max track count ignored";
        return;
    }
    _max_track_size = MAX(i, 1);
    checkTrackIfReady();
}

void MediaSink::emitAllTrackReady() {
    if (_all_track_ready) {
        return;
    }

    DebugL << "All track ready use " << _ticker.elapsedTime() << "ms";
    if (!_track_ready_callback.empty()) {
        // 这是超时强制忽略未准备好的Track
        _track_ready_callback.clear();
        // 移除未准备好的Track
        for (auto it = _track_map.begin(); it != _track_map.end();) {
            if (!it->second.second || !it->second.first->ready()) {
                WarnL << "Track not ready for a long time, ignored: " << it->second.first->getCodecName();
                it = _track_map.erase(it);
                continue;
            }
            ++it;
        }
    }

    if (!_track_map.empty()) {
        // 最少有一个有效的Track
        onAllTrackReady_l();

        // 全部Track就绪，我们一次性把之前的帧输出
        for (auto &pr : _frame_unread) {
            if (_track_map.find(pr.first) == _track_map.end()) {
                // 该Track已经被移除
                continue;
            }
            pr.second.for_each([&](const Frame::Ptr &frame) { MediaSink::inputFrame(frame); });
        }
        _frame_unread.clear();
    } else {
        throw toolkit::SockException(toolkit::Err_shutdown, "no vaild track data");
    }
}

void MediaSink::onAllTrackReady_l() {
    // 是否添加静音音频
    if (_add_mute_audio) {
        addMuteAudioTrack();
    }
    onAllTrackReady();
    _all_track_ready = true;
    _have_video = (bool)getTrack(TrackVideo);
}

vector<Track::Ptr> MediaSink::getTracks(bool ready) const {
    vector<Track::Ptr> ret;
    for (auto &pr : _track_map) {
        if (ready && !pr.second.first->ready()) {
            continue;
        }
        ret.emplace_back(pr.second.first);
    }
    return ret;
}

static uint8_t s_mute_adts[] = {0xff, 0xf1, 0x6c, 0x40, 0x2d, 0x3f, 0xfc, 0x00, 0xe0, 0x34, 0x20, 0xad, 0xf2, 0x3f, 0xb5, 0xdd,
                                0x73, 0xac, 0xbd, 0xca, 0xd7, 0x7d, 0x4a, 0x13, 0x2d, 0x2e, 0xa2, 0x62, 0x02, 0x70, 0x3c, 0x1c,
                                0xc5, 0x63, 0x55, 0x69, 0x94, 0xb5, 0x8d, 0x70, 0xd7, 0x24, 0x6a, 0x9e, 0x2e, 0x86, 0x24, 0xea,
                                0x4f, 0xd4, 0xf8, 0x10, 0x53, 0xa5, 0x4a, 0xb2, 0x9a, 0xf0, 0xa1, 0x4f, 0x2f, 0x66, 0xf9, 0xd3,
                                0x8c, 0xa6, 0x97, 0xd5, 0x84, 0xac, 0x09, 0x25, 0x98, 0x0b, 0x1d, 0x77, 0x04, 0xb8, 0x55, 0x49,
                                0x85, 0x27, 0x06, 0x23, 0x58, 0xcb, 0x22, 0xc3, 0x20, 0x3a, 0x12, 0x09, 0x48, 0x24, 0x86, 0x76,
                                0x95, 0xe3, 0x45, 0x61, 0x43, 0x06, 0x6b, 0x4a, 0x61, 0x14, 0x24, 0xa9, 0x16, 0xe0, 0x97, 0x34,
                                0xb6, 0x58, 0xa4, 0x38, 0x34, 0x90, 0x19, 0x5d, 0x00, 0x19, 0x4a, 0xc2, 0x80, 0x4b, 0xdc, 0xb7,
                                0x00, 0x18, 0x12, 0x3d, 0xd9, 0x93, 0xee, 0x74, 0x13, 0x95, 0xad, 0x0b, 0x59, 0x51, 0x0e, 0x99,
                                0xdf, 0x49, 0x98, 0xde, 0xa9, 0x48, 0x4b, 0xa5, 0xfb, 0xe8, 0x79, 0xc9, 0xe2, 0xd9, 0x60, 0xa5,
                                0xbe, 0x74, 0xa6, 0x6b, 0x72, 0x0e, 0xe3, 0x7b, 0x28, 0xb3, 0x0e, 0x52, 0xcc, 0xf6, 0x3d, 0x39,
                                0xb7, 0x7e, 0xbb, 0xf0, 0xc8, 0xce, 0x5c, 0x72, 0xb2, 0x89, 0x60, 0x33, 0x7b, 0xc5, 0xda, 0x49,
                                0x1a, 0xda, 0x33, 0xba, 0x97, 0x9e, 0xa8, 0x1b, 0x6d, 0x5a, 0x77, 0xb6, 0xf1, 0x69, 0x5a, 0xd1,
                                0xbd, 0x84, 0xd5, 0x4e, 0x58, 0xa8, 0x5e, 0x8a, 0xa0, 0xc2, 0xc9, 0x22, 0xd9, 0xa5, 0x53, 0x11,
                                0x18, 0xc8, 0x3a, 0x39, 0xcf, 0x3f, 0x57, 0xb6, 0x45, 0x19, 0x1e, 0x8a, 0x71, 0xa4, 0x46, 0x27,
                                0x9e, 0xe9, 0xa4, 0x86, 0xdd, 0x14, 0xd9, 0x4d, 0xe3, 0x71, 0xe3, 0x26, 0xda, 0xaa, 0x17, 0xb4,
                                0xac, 0xe1, 0x09, 0xc1, 0x0d, 0x75, 0xba, 0x53, 0x0a, 0x37, 0x8b, 0xac, 0x37, 0x39, 0x41, 0x27,
                                0x6a, 0xf0, 0xe9, 0xb4, 0xc2, 0xac, 0xb0, 0x39, 0x73, 0x17, 0x64, 0x95, 0xf4, 0xdc, 0x33, 0xbb,
                                0x84, 0x94, 0x3e, 0xf8, 0x65, 0x71, 0x60, 0x7b, 0xd4, 0x5f, 0x27, 0x79, 0x95, 0x6a, 0xba, 0x76,
                                0xa6, 0xa5, 0x9a, 0xec, 0xae, 0x55, 0x3a, 0x27, 0x48, 0x23, 0xcf, 0x5c, 0x4d, 0xbc, 0x0b, 0x35,
                                0x5c, 0xa7, 0x17, 0xcf, 0x34, 0x57, 0xc9, 0x58, 0xc5, 0x20, 0x09, 0xee, 0xa5, 0xf2, 0x9c, 0x6c,
                                0x39, 0x1a, 0x77, 0x92, 0x9b, 0xff, 0xc6, 0xae, 0xf8, 0x36, 0xba, 0xa8, 0xaa, 0x6b, 0x1e, 0x8c,
                                0xc5, 0x97, 0x39, 0x6a, 0xb8, 0xa2, 0x55, 0xa8, 0xf8};

#define MUTE_ADTS_DATA s_mute_adts
#define MUTE_ADTS_DATA_MS 128
static uint8_t ADTS_CONFIG[2] = { 0x15, 0x88 };

bool MuteAudioMaker::inputFrame(const Frame::Ptr &frame) {
    if (_track_index == -1) {
        // 锁定track
        _track_index = frame->getIndex();
    }
    if (frame->getIndex() != _track_index) {
        // 不是锁定的track
        return false;
    }
    auto audio_idx = frame->dts() / MUTE_ADTS_DATA_MS;
    if (_audio_idx != audio_idx) {
        _audio_idx = audio_idx;
        auto aacFrame = std::make_shared<FrameToCache<FrameFromPtr>>(CodecAAC, (char *)MUTE_ADTS_DATA, sizeof(s_mute_adts), _audio_idx * MUTE_ADTS_DATA_MS, 0, 7);
        aacFrame->setIndex(MUTE_AUDIO_INDEX);
        return FrameDispatcher::inputFrame(aacFrame);
    }
    return false;
}

bool MediaSink::addMuteAudioTrack() {
    if (!_enable_audio) {
        return false;
    }
    for (auto &pr : _track_map) {
        if (pr.second.first->getTrackType() == TrackAudio) {
            return false;
        }
    }
    auto audio = Factory::getTrackByCodecId(CodecAAC);
    audio->setIndex(MUTE_AUDIO_INDEX);
    audio->setExtraData(ADTS_CONFIG, 2);
    _track_map[MUTE_AUDIO_INDEX] = std::make_pair(audio, true);
    audio->addDelegate([this](const Frame::Ptr &frame) { return onTrackFrame(frame); });
    _mute_audio_maker = std::make_shared<MuteAudioMaker>();
    _mute_audio_maker->addDelegate([audio](const Frame::Ptr &frame) { return audio->inputFrame(frame); });
    onTrackReady(audio);
    TraceL << "Mute aac track added";
    return true;
}

bool MediaSink::isAllTrackReady() const {
    return _all_track_ready;
}

void MediaSink::enableAudio(bool flag) {
    _enable_audio = flag;
}

void MediaSink::setOnlyAudio() {
    _only_audio = true;
    _enable_audio = true;
    _add_mute_audio = false;
}

void MediaSink::enableMuteAudio(bool flag) {
    _add_mute_audio = flag;
}

bool MediaSink::haveVideo() const {
    return _have_video;
}

///////////////////////////DemuxerSink//////////////////////////////

void MediaSinkDelegate::setTrackListener(TrackListener *listener) {
    _listener = listener;
}

bool MediaSinkDelegate::onTrackReady(const Track::Ptr &track) {
    if (_listener) {
        _listener->addTrack(track);
    }
    return true;
}

void MediaSinkDelegate::onAllTrackReady() {
    if (_listener) {
        _listener->addTrackCompleted();
    }
}

void MediaSinkDelegate::resetTracks() {
    MediaSink::resetTracks();
    if (_listener) {
        _listener->resetTracks();
    }
}

///////////////////////////Demuxer//////////////////////////////

void Demuxer::setTrackListener(TrackListener *listener, bool wait_track_ready) {
    if (wait_track_ready) {
        auto sink = std::make_shared<MediaSinkDelegate>();
        sink->setTrackListener(listener);
        _sink = std::move(sink);
    }
    _listener = listener;
}

bool Demuxer::addTrack(const Track::Ptr &track) {
    if (!_sink) {
        _origin_track.emplace_back(track);
        return _listener ? _listener->addTrack(track) : false;
    }

    if (_sink->addTrack(track)) {
        track->addDelegate([this](const Frame::Ptr &frame) { return _sink->inputFrame(frame); });
        return true;
    }
    return false;
}

void Demuxer::addTrackCompleted() {
    if (_sink) {
        _sink->addTrackCompleted();
    } else if (_listener) {
        _listener->addTrackCompleted();
    }
}

void Demuxer::resetTracks() {
    if (_sink) {
        _sink->resetTracks();
    } else if (_listener) {
        _listener->resetTracks();
    }
}

vector<Track::Ptr> Demuxer::getTracks(bool ready) const {
    if (_sink) {
        return _sink->getTracks(ready);
    }

    vector<Track::Ptr> ret;
    for (auto &track : _origin_track) {
        if (ready && !track->ready()) {
            continue;
        }
        ret.emplace_back(track);
    }
    return ret;
}
} // namespace mediakit
