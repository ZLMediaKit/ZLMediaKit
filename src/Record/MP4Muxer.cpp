/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifdef ENABLE_MP4

#include "MP4Muxer.h"
#include "Util/File.h"
#include "Extension/H264.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

MP4Muxer::MP4Muxer() {}

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
    return _mp4_file->createWriter(mp4FastStart ? MOV_FLAG_FASTSTART : 0, false);
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

void MP4MuxerInterface::resetTracks() {
    _started = false;
    _have_video = false;
    _mov_writter = nullptr;
    _frame_merger.clear();
    _codec_to_trackid.clear();
}

bool MP4MuxerInterface::inputFrame(const Frame::Ptr &frame) {
    auto it = _codec_to_trackid.find(frame->getCodecId());
    if (it == _codec_to_trackid.end()) {
        //该Track不存在或初始化失败
        return false;
    }

    if (!_started) {
        //该逻辑确保含有视频时，第一帧为关键帧
        if (_have_video && !frame->keyFrame()) {
            //含有视频，但是不是关键帧，那么前面的帧丢弃
            return false;
        }
        //开始写文件
        _started = true;
    }

    //mp4文件时间戳需要从0开始
    auto &track_info = it->second;
    int64_t dts_out, pts_out;
    switch (frame->getCodecId()) {
        case CodecH264:
        case CodecH265: {
            //这里的代码逻辑是让SPS、PPS、IDR这些时间戳相同的帧打包到一起当做一个帧处理，
            _frame_merger.inputFrame(frame, [&](uint64_t dts, uint64_t pts, const Buffer::Ptr &buffer, bool have_idr) {
                track_info.stamp.revise(dts, pts, dts_out, pts_out);
                mp4_writer_write(_mov_writter.get(),
                                 track_info.track_id,
                                 buffer->data(),
                                 buffer->size(),
                                 pts_out,
                                 dts_out,
                                 have_idr ? MOV_AV_FLAG_KEYFREAME : 0);
            });
            break;
        }

        default: {
            track_info.stamp.revise(frame->dts(), frame->pts(), dts_out, pts_out);
            mp4_writer_write(_mov_writter.get(),
                             track_info.track_id,
                             frame->data() + frame->prefixSize(),
                             frame->size() - frame->prefixSize(),
                             pts_out,
                             dts_out,
                             frame->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0);
            break;
        }
    }
    return true;
}

static uint8_t getObject(CodecId codecId) {
    switch (codecId){
        case CodecG711A : return MOV_OBJECT_G711a;
        case CodecG711U : return MOV_OBJECT_G711u;
        case CodecOpus : return MOV_OBJECT_OPUS;
        case CodecAAC : return MOV_OBJECT_AAC;
        case CodecH264 : return MOV_OBJECT_H264;
        case CodecH265 : return MOV_OBJECT_HEVC;
        default : return 0;
    }
}

void MP4MuxerInterface::stampSync() {
    if (_codec_to_trackid.size() < 2) {
        return;
    }

    Stamp *audio = nullptr, *video = nullptr;
    for(auto &pr : _codec_to_trackid){
        switch (getTrackType((CodecId) pr.first)){
            case TrackAudio : audio = &pr.second.stamp; break;
            case TrackVideo : video = &pr.second.stamp; break;
            default : break;
        }
    }

    if (audio && video) {
        //音频时间戳同步于视频，因为音频时间戳被修改后不影响播放
        audio->syncTo(*video);
    }
}

bool MP4MuxerInterface::addTrack(const Track::Ptr &track) {
    if (!_mov_writter) {
        _mov_writter = createWriter();
    }
    auto mp4_object = getObject(track->getCodecId());
    if (!mp4_object) {
        WarnL << "MP4录制不支持该编码格式:" << track->getCodecName();
        return false;
    }

    if (!track->ready()) {
        WarnL << "Track[" << track->getCodecName() << "]未就绪";
        return false;
    }

    switch (track->getCodecId()) {
        case CodecG711A:
        case CodecG711U:
        case CodecOpus: {
            auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
            if (!audio_track) {
                WarnL << "不是音频Track:" << track->getCodecName();
                return false;
            }

            auto track_id = mp4_writer_add_audio(_mov_writter.get(),
                                                 mp4_object,
                                                 audio_track->getAudioChannel(),
                                                 audio_track->getAudioSampleBit() * audio_track->getAudioChannel(),
                                                 audio_track->getAudioSampleRate(),
                                                 nullptr, 0);
            if (track_id < 0) {
                WarnL << "添加Track[" << track->getCodecName() << "]失败:" << track_id;
                return false;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
            break;
        }

        case CodecAAC: {
            auto audio_track = dynamic_pointer_cast<AACTrack>(track);
            if (!audio_track) {
                WarnL << "不是AAC Track";
                return false;
            }

            auto track_id = mp4_writer_add_audio(_mov_writter.get(),
                                                 mp4_object,
                                                 audio_track->getAudioChannel(),
                                                 audio_track->getAudioSampleBit() * audio_track->getAudioChannel(),
                                                 audio_track->getAudioSampleRate(),
                                                 audio_track->getAacCfg().data(),
                                                 audio_track->getAacCfg().size());
            if (track_id < 0) {
                WarnL << "添加AAC Track失败:" << track_id;
                return false;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
            break;
        }

        case CodecH264: {
            auto h264_track = dynamic_pointer_cast<H264Track>(track);
            if (!h264_track) {
                WarnL << "不是H264 Track";
                return false;
            }

            struct mpeg4_avc_t avc;
            memset(&avc, 0, sizeof(avc));
            string sps_pps = string("\x00\x00\x00\x01", 4) + h264_track->getSps() +
                             string("\x00\x00\x00\x01", 4) + h264_track->getPps();
            h264_annexbtomp4(&avc, sps_pps.data(), (int) sps_pps.size(), NULL, 0, NULL, NULL);

            uint8_t extra_data[1024];
            int extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, extra_data, sizeof(extra_data));
            if (extra_data_size == -1) {
                WarnL << "生成H264 extra_data 失败";
                return false;
            }

            auto track_id = mp4_writer_add_video(_mov_writter.get(),
                                                 mp4_object,
                                                 h264_track->getVideoWidth(),
                                                 h264_track->getVideoHeight(),
                                                 extra_data,
                                                 extra_data_size);

            if (track_id < 0) {
                WarnL << "添加H264 Track失败:" << track_id;
                return false;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
            _have_video = true;
            break;
        }

        case CodecH265: {
            auto h265_track = dynamic_pointer_cast<H265Track>(track);
            if (!h265_track) {
                WarnL << "不是H265 Track";
                return false;
            }

            struct mpeg4_hevc_t hevc;
            memset(&hevc, 0, sizeof(hevc));
            string vps_sps_pps = string("\x00\x00\x00\x01", 4) + h265_track->getVps() +
                                 string("\x00\x00\x00\x01", 4) + h265_track->getSps() +
                                 string("\x00\x00\x00\x01", 4) + h265_track->getPps();
            h265_annexbtomp4(&hevc, vps_sps_pps.data(), (int) vps_sps_pps.size(), NULL, 0, NULL, NULL);

            uint8_t extra_data[1024];
            int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, extra_data, sizeof(extra_data));
            if (extra_data_size == -1) {
                WarnL << "生成H265 extra_data 失败";
                return false;
            }

            auto track_id = mp4_writer_add_video(_mov_writter.get(),
                                                 mp4_object,
                                                 h265_track->getVideoWidth(),
                                                 h265_track->getVideoHeight(),
                                                 extra_data,
                                                 extra_data_size);
            if (track_id < 0) {
                WarnL << "添加H265 Track失败:" << track_id;
                return false;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
            _have_video = true;
            break;
        }

        default: WarnL << "MP4录制不支持该编码格式:" << track->getCodecName(); return false;
    }

    //尝试音视频同步
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
        //尚未生成init segment
        return false;
    }

    bool key_frame = frame->keyFrame();

    //flush切片
    saveSegment();

    auto data = _memory_file->getAndClearMemory();
    if (!data.empty()) {
        //输出切片数据
        onSegmentData(std::move(data), frame->dts(), _key_frame);
        _key_frame = false;
    }

    if (key_frame) {
        _key_frame = true;
    }

    return MP4MuxerInterface::inputFrame(frame);
}

}//namespace mediakit
#endif//#ifdef ENABLE_MP4
