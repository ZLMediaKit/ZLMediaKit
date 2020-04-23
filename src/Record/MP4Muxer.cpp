/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifdef ENABLE_MP4
#include "MP4Muxer.h"
#include "Util/File.h"
namespace mediakit{

MP4Muxer::MP4Muxer(const char *file) {
    _file_name = file;
    openMP4();
}

MP4Muxer::~MP4Muxer() {
    closeMP4();
}

void MP4Muxer::openMP4(){
    closeMP4();
    openFile(_file_name.data(), "wb+");
    _mov_writter = createWriter();
}
void MP4Muxer::closeMP4(){
    _mov_writter = nullptr;
    closeFile();
}

void MP4Muxer::resetTracks() {
    _codec_to_trackid.clear();
    _started = false;
    _have_video = false;
    openMP4();
}

void MP4Muxer::inputFrame(const Frame::Ptr &frame) {
    auto it = _codec_to_trackid.find(frame->getCodecId());
    if(it == _codec_to_trackid.end()){
        //该Track不存在或初始化失败
        return;
    }

    if (!_started) {
        //还没开始
        if (!_have_video) {
            _started = true;
        } else {
            if (frame->getTrackType() != TrackVideo || !frame->keyFrame()) {
                //如果首帧是音频或者是视频但是不是i帧，那么不能开始写文件
                return;
            }
            //开始写文件
            _started = true;
        }
    }

    //mp4文件时间戳需要从0开始
    auto &track_info = it->second;
    int64_t dts_out, pts_out;

    switch (frame->getCodecId()) {
        case CodecH264:
        case CodecH265: {
            //这里的代码逻辑是让SPS、PPS、IDR这些时间戳相同的帧打包到一起当做一个帧处理，
            if (!_frameCached.empty() && _frameCached.back()->dts() != frame->dts()) {
                Frame::Ptr back = _frameCached.back();
                //求相对时间戳
                track_info.stamp.revise(back->dts(), back->pts(), dts_out, pts_out);

                if (_frameCached.size() != 1) {
                    //缓存中有多帧，需要按照mp4格式合并一起
                    string merged;
                    _frameCached.for_each([&](const Frame::Ptr &frame) {
                        uint32_t nalu_size = frame->size() - frame->prefixSize();
                        nalu_size = htonl(nalu_size);
                        merged.append((char *) &nalu_size, 4);
                        merged.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
                    });
                    mov_writer_write_l(_mov_writter.get(),
                                       track_info.track_id,
                                       merged.data(),
                                       merged.size(),
                                       pts_out,
                                       dts_out,
                                       back->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0,
                                       1/*我们合并时已经生成了4个字节的MP4格式start code*/);
                } else {
                    //缓存中只有一帧视频
                    mov_writer_write_l(_mov_writter.get(),
                                       track_info.track_id,
                                       back->data() + back->prefixSize(),
                                       back->size() - back->prefixSize(),
                                       pts_out,
                                       dts_out,
                                       back->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0,
                                       0/*需要生成头4个字节的MP4格式start code*/);
                }
                _frameCached.clear();
            }
            //缓存帧，时间戳相同的帧合并一起写入mp4
            _frameCached.emplace_back(Frame::getCacheAbleFrame(frame));
        }
            break;
        default: {
            track_info.stamp.revise(frame->dts(), frame->pts(), dts_out, pts_out);
            mov_writer_write_l(_mov_writter.get(),
                               track_info.track_id,
                               frame->data() + frame->prefixSize(),
                               frame->size() - frame->prefixSize(),
                               pts_out,
                               dts_out,
                               frame->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0,
                               1/*aac或其他类型frame不用添加4个nalu_size的字节*/);
        }
            break;
    }
}

void MP4Muxer::addTrack(const Track::Ptr &track) {
    switch (track->getCodecId()) {
        case CodecG711A:
        case CodecG711U: {
            auto audio_track = dynamic_pointer_cast<G711Track>(track);
            if (!audio_track) {
                WarnL << "不是G711 Track";
                return;
            }
            if (!audio_track->ready()) {
                WarnL << "G711 Track未就绪";
                return;
            }
            auto track_id = mov_writer_add_audio(_mov_writter.get(),
                                                 track->getCodecId() == CodecG711A ? MOV_OBJECT_G711a : MOV_OBJECT_G711u,
                                                 audio_track->getAudioChannel(),
                                                 audio_track->getAudioSampleBit() * audio_track->getAudioChannel(),
                                                 audio_track->getAudioSampleRate(),
                                                 nullptr, 0);
            if (track_id < 0) {
                WarnL << "添加G711 Track失败:" << track_id;
                return;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
        }
            break;

        case CodecAAC: {
            auto audio_track = dynamic_pointer_cast<AACTrack>(track);
            if (!audio_track) {
                WarnL << "不是AAC Track";
                return;
            }
            if(!audio_track->ready()){
                WarnL << "AAC Track未就绪";
                return;
            }
            auto track_id = mov_writer_add_audio(_mov_writter.get(),
                                                 MOV_OBJECT_AAC,
                                                 audio_track->getAudioChannel(),
                                                 audio_track->getAudioSampleBit() * audio_track->getAudioChannel(),
                                                 audio_track->getAudioSampleRate(),
                                                 audio_track->getAacCfg().data(), 2);
            if(track_id < 0){
                WarnL << "添加AAC Track失败:" << track_id;
                return;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
        }
            break;
        case CodecH264: {
            auto h264_track = dynamic_pointer_cast<H264Track>(track);
            if (!h264_track) {
                WarnL << "不是H264 Track";
                return;
            }
            if(!h264_track->ready()){
                WarnL << "H264 Track未就绪";
                return;
            }

            struct mpeg4_avc_t avc = {0};
            string sps_pps = string("\x00\x00\x00\x01", 4) + h264_track->getSps() +
                             string("\x00\x00\x00\x01", 4) + h264_track->getPps();
            h264_annexbtomp4(&avc, sps_pps.data(), sps_pps.size(), NULL, 0, NULL, NULL);

            uint8_t extra_data[1024];
            int extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, extra_data, sizeof(extra_data));
            if (extra_data_size == -1) {
                WarnL << "生成H264 extra_data 失败";
                return;
            }

            auto track_id = mov_writer_add_video(_mov_writter.get(),
                                                 MOV_OBJECT_H264,
                                                 h264_track->getVideoWidth(),
                                                 h264_track->getVideoHeight(),
                                                 extra_data,
                                                 extra_data_size);

            if(track_id < 0){
                WarnL << "添加H264 Track失败:" << track_id;
                return;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
            _have_video = true;
        }
            break;
        case CodecH265: {
            auto h265_track = dynamic_pointer_cast<H265Track>(track);
            if (!h265_track) {
                WarnL << "不是H265 Track";
                return;
            }
            if(!h265_track->ready()){
                WarnL << "H265 Track未就绪";
                return;
            }

            struct mpeg4_hevc_t hevc = {0};
            string vps_sps_pps = string("\x00\x00\x00\x01", 4) + h265_track->getVps() +
                                 string("\x00\x00\x00\x01", 4) + h265_track->getSps() +
                                 string("\x00\x00\x00\x01", 4) + h265_track->getPps();
            h265_annexbtomp4(&hevc, vps_sps_pps.data(), vps_sps_pps.size(), NULL, 0, NULL, NULL);

            uint8_t extra_data[1024];
            int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, extra_data, sizeof(extra_data));
            if (extra_data_size == -1) {
                WarnL << "生成H265 extra_data 失败";
                return;
            }

            auto track_id = mov_writer_add_video(_mov_writter.get(),
                                                 MOV_OBJECT_HEVC,
                                                 h265_track->getVideoWidth(),
                                                 h265_track->getVideoHeight(),
                                                 extra_data,
                                                 extra_data_size);
            if(track_id < 0){
                WarnL << "添加H265 Track失败:" << track_id;
                return;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
            _have_video = true;
        }
            break;
        default:
            WarnL << "MP4录制不支持该编码格式:" << track->getCodecName();
            break;
    }
}

}//namespace mediakit
#endif//#ifdef ENABLE_MP4
