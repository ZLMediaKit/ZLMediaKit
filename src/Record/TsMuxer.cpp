/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TsMuxer.h"
#if defined(ENABLE_HLS)
#include "mpeg-ts-proto.h"
#include "mpeg-ts.h"
#include "Extension/H264.h"

namespace mediakit {

TsMuxer::TsMuxer() {
    init();
}

TsMuxer::~TsMuxer() {
    uninit();
}

void TsMuxer::stampSync(){
    if(_codec_to_trackid.size() < 2){
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

    if(audio && video){
        //音频时间戳同步于视频，因为音频时间戳被修改后不影响播放
        audio->syncTo(*video);
    }
}

void TsMuxer::addTrack(const Track::Ptr &track) {
    switch (track->getCodecId()) {
        case CodecH264: {
            _have_video = true;
            _codec_to_trackid[track->getCodecId()].track_id = mpeg_ts_add_stream(_context, PSI_STREAM_H264, nullptr, 0);
            break;
        }

        case CodecH265: {
            _have_video = true;
            _codec_to_trackid[track->getCodecId()].track_id = mpeg_ts_add_stream(_context, PSI_STREAM_H265, nullptr, 0);
            break;
        }

        case CodecAAC: {
            _codec_to_trackid[track->getCodecId()].track_id = mpeg_ts_add_stream(_context, PSI_STREAM_AAC, nullptr, 0);
            break;
        }

        case CodecG711A: {
            _codec_to_trackid[track->getCodecId()].track_id = mpeg_ts_add_stream(_context, PSI_STREAM_AUDIO_G711A, nullptr, 0);
            break;
        }

        case CodecG711U: {
            _codec_to_trackid[track->getCodecId()].track_id = mpeg_ts_add_stream(_context, PSI_STREAM_AUDIO_G711U, nullptr, 0);
            break;
        }

        default: WarnL << "mpeg-ts 不支持该编码格式,已忽略:" << track->getCodecName(); break;
    }

    //尝试音视频同步
    stampSync();
}

void TsMuxer::inputFrame(const Frame::Ptr &frame) {
    auto it = _codec_to_trackid.find(frame->getCodecId());
    if(it == _codec_to_trackid.end()){
        return;
    }
    //mp4文件时间戳需要从0开始
    auto &track_info = it->second;
    int64_t dts_out, pts_out;
    _is_idr_fast_packet = !_have_video;
    switch (frame->getCodecId()){
        case CodecH264: {
            int type = H264_TYPE(*((uint8_t *)frame->data() + frame->prefixSize()));
            if(type == H264Frame::NAL_SEI){
                break;
            }
        }
        case CodecH265: {
            //这里的代码逻辑是让SPS、PPS、IDR这些时间戳相同的帧打包到一起当做一个帧处理，
            if (!_frameCached.empty() && _frameCached.back()->dts() != frame->dts()) {
                Frame::Ptr back = _frameCached.back();
                Buffer::Ptr merged_frame = back;
                if(_frameCached.size() != 1){
                    string merged;
                    _frameCached.for_each([&](const Frame::Ptr &frame){
                        if(frame->prefixSize()){
                            merged.append(frame->data(),frame->size());
                        } else{
                            merged.append("\x00\x00\x00\x01",4);
                            merged.append(frame->data(),frame->size());
                        }
                        if(frame->keyFrame()){
                            _is_idr_fast_packet = true;
                        }
                    });
                    merged_frame = std::make_shared<BufferString>(std::move(merged));
                }
                track_info.stamp.revise(back->dts(),back->pts(),dts_out,pts_out);
                _timestamp = dts_out;
                mpeg_ts_write(_context, track_info.track_id, back->keyFrame() ? 0x0001 : 0, pts_out * 90LL, dts_out * 90LL, merged_frame->data(),  merged_frame->size());
                _frameCached.clear();
            }
            _frameCached.emplace_back(Frame::getCacheAbleFrame(frame));
        }
            break;
        default: {
            track_info.stamp.revise(frame->dts(),frame->pts(),dts_out,pts_out);
            _timestamp = dts_out;
            mpeg_ts_write(_context, track_info.track_id, frame->keyFrame() ? 0x0001 : 0, pts_out * 90LL, dts_out * 90LL, frame->data(), frame->size());
        }
            break;
    }
}

void TsMuxer::resetTracks() {
    _have_video = false;
    //通知片段中断
    onTs(nullptr, 0, 0, 0);
    uninit();
    init();
}

void TsMuxer::init() {
    static mpeg_ts_func_t s_func= {
            [](void* param, size_t bytes){
                TsMuxer *muxer = (TsMuxer *)param;
                assert(sizeof(TsMuxer::_tsbuf) >= bytes);
                return (void *)muxer->_tsbuf;
            },
            [](void* param, void* packet){
                //do nothing
            },
            [](void* param, const void* packet, size_t bytes){
                TsMuxer *muxer = (TsMuxer *)param;
                muxer->onTs(packet, bytes,muxer->_timestamp,muxer->_is_idr_fast_packet);
                muxer->_is_idr_fast_packet = false;
            }
    };
    if(_context == nullptr){
        _context = mpeg_ts_create(&s_func,this);
    }
}

void TsMuxer::uninit() {
    if(_context){
        mpeg_ts_destroy(_context);
        _context = nullptr;
    }
    _codec_to_trackid.clear();
}

}//namespace mediakit

#endif// defined(ENABLE_HLS)