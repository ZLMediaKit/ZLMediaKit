/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_MP4)

#include "MP4Muxer.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

MP4Muxer::~MP4Muxer() {
    closeMP4();
}

void MP4Muxer::openMP4(const string &file) {
    closeMP4();
    _file_name = file;
    _mp4_file = std::make_shared<MP4FileDisk>();
    _mp4_file->openFile(_file_name.data(), "wb+");
}

MP4FileIO::Writer MP4Muxer::createWriter() {
    GET_CONFIG(bool, mp4FastStart, Record::kFastStart);
    GET_CONFIG(bool, recordEnableFmp4, Record::kEnableFmp4);
    return _mp4_file->createWriter(mp4FastStart ? MOV_FLAG_FASTSTART : 0, recordEnableFmp4);
}

void MP4Muxer::closeMP4() {
    MP4MuxerInterface::resetTracks();
    _mp4_file = nullptr;
}

void MP4Muxer::resetTracks() {
    MP4MuxerInterface::resetTracks();
    openMP4(_file_name);
}

/////////////////////////////////////////// MP4MuxerInterface /////////////////////////////////////////////

void MP4MuxerInterface::saveSegment() {
    mp4_writer_save_segment(_mov_writter.get());
}

void MP4MuxerInterface::initSegment() {
    mp4_writer_init_segment(_mov_writter.get());
}

bool MP4MuxerInterface::haveVideo() const {
    return _have_video;
}

uint64_t MP4MuxerInterface::getDuration() const {
    uint64_t ret = 0;
    for (auto &pr : _tracks) {
        if (pr.second.stamp.getRelativeStamp() > (int64_t)ret) {
            ret = pr.second.stamp.getRelativeStamp();
        }
    }
    return ret;
}

void MP4MuxerInterface::resetTracks() {
    _started = false;
    _have_video = false;
    _mov_writter = nullptr;
    _tracks.clear();
}

void MP4MuxerInterface::flush() {
    for (auto &pr : _tracks) {
        pr.second.merger.flush();
    }
}

bool MP4MuxerInterface::inputFrame(const Frame::Ptr &frame) {
    auto it = _tracks.find(frame->getIndex());
    if (it == _tracks.end()) {
        // 该Track不存在或初始化失败
        return false;
    }

    if (!_started) {
        // 该逻辑确保含有视频时，第一帧为关键帧
        if (_have_video && !frame->keyFrame()) {
            // 含有视频，但是不是关键帧，那么前面的帧丢弃
            return false;
        }
        // 开始写文件
        _started = true;
    }

    // mp4文件时间戳需要从0开始
    auto &track = it->second;
    switch (frame->getCodecId()) {
        case CodecH264:
        case CodecH265: {
            // 这里的代码逻辑是让SPS、PPS、IDR这些时间戳相同的帧打包到一起当做一个帧处理，
            track.merger.inputFrame(frame, [this, &track](uint64_t dts, uint64_t pts, const Buffer::Ptr &buffer, bool have_idr) {
                int64_t dts_out, pts_out;
                track.stamp.revise(dts, pts, dts_out, pts_out);
                mp4_writer_write(_mov_writter.get(), track.track_id, buffer->data(), buffer->size(), pts_out, dts_out, have_idr ? MOV_AV_FLAG_KEYFREAME : 0);
            });
            break;
        }

        default: {
            int64_t dts_out, pts_out;
            track.stamp.revise(frame->dts(), frame->pts(), dts_out, pts_out);
            mp4_writer_write(_mov_writter.get(), track.track_id, frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize(), pts_out, dts_out, frame->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0);
            break;
        }
    }
    return true;
}

void MP4MuxerInterface::stampSync() {
    Stamp *first = nullptr;
    for (auto &pr : _tracks) {
        if (!first) {
            first = &pr.second.stamp;
        } else {
            pr.second.stamp.syncTo(*first);
        }
    }
}

bool MP4MuxerInterface::addTrack(const Track::Ptr &track) {
    if (!_mov_writter) {
        _mov_writter = createWriter();
    }
    auto mp4_object = getMovIdByCodec(track->getCodecId());
    if (mp4_object == MOV_OBJECT_NONE) {
        WarnL << "Unsupported codec: " << track->getCodecName();
        return false;
    }

    if (!track->ready()) {
        WarnL << "Track " << track->getCodecName() << " unready";
        return false;
    }

    track->update();

    auto extra = track->getExtraData();
    auto extra_data = extra ? extra->data() : nullptr;
    auto extra_size = extra ? extra->size() : 0;
    if (track->getTrackType() == TrackVideo) {
        auto video_track = dynamic_pointer_cast<VideoTrack>(track);
        CHECK(video_track);
        auto track_id = mp4_writer_add_video(_mov_writter.get(), mp4_object, video_track->getVideoWidth(), video_track->getVideoHeight(), extra_data, extra_size);
        if (track_id < 0) {
            WarnL << "mp4_writer_add_video failed: " << video_track->getCodecName();
            return false;
        }
        _tracks[track->getIndex()].track_id = track_id;
        _have_video = true;
    } else if (track->getTrackType() == TrackAudio) {
        auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
        CHECK(audio_track);
        auto track_id = mp4_writer_add_audio(_mov_writter.get(), mp4_object, audio_track->getAudioChannel(), audio_track->getAudioSampleBit() * audio_track->getAudioChannel(), audio_track->getAudioSampleRate(), extra_data, extra_size);
        if (track_id < 0) {
            WarnL << "mp4_writer_add_audio failed: " << audio_track->getCodecName();
            return false;
        }
        _tracks[track->getIndex()].track_id = track_id;
    }

    // 尝试音视频同步
    stampSync();
    return true;
}

/////////////////////////////////////////// MP4MuxerMemory /////////////////////////////////////////////

MP4MuxerMemory::MP4MuxerMemory() {
    _memory_file = std::make_shared<MP4FileMemory>();
}

MP4FileIO::Writer MP4MuxerMemory::createWriter() {
    return _memory_file->createWriter(MOV_FLAG_SEGMENT, true);
}

const string &MP4MuxerMemory::getInitSegment() {
    if (_init_segment.empty()) {
        initSegment();
        saveSegment();
        _init_segment = _memory_file->getAndClearMemory();
    }
    return _init_segment;
}

void MP4MuxerMemory::resetTracks() {
    MP4MuxerInterface::resetTracks();
    _memory_file = std::make_shared<MP4FileMemory>();
    _init_segment.clear();
}

bool MP4MuxerMemory::inputFrame(const Frame::Ptr &frame) {
    if (_init_segment.empty()) {
        // 尚未生成init segment
        return false;
    }

    // flush切片
    saveSegment();

    auto data = _memory_file->getAndClearMemory();
    if (!data.empty()) {
        // 输出切片数据
        onSegmentData(std::move(data), _last_dst, _key_frame);
        _key_frame = false;
    }

    if (frame->keyFrame()) {
        _key_frame = true;
    }
    if (frame->getTrackType() == TrackVideo || !haveVideo()) {
        _last_dst = frame->dts();
    }
    return MP4MuxerInterface::inputFrame(frame);
}

} // namespace mediakit
#endif // defined(ENABLE_MP4)
