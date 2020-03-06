/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "TsMuxer.h"
#if defined(ENABLE_HLS)
#include "mpeg-ts-proto.h"
#include "mpeg-ts.h"

namespace mediakit {

TsMuxer::TsMuxer() {
    init();
}

TsMuxer::~TsMuxer() {
    uninit();
}

void TsMuxer::addTrack(const Track::Ptr &track) {
    switch (track->getCodecId()) {
        case CodecH264: {
            _have_video = true;
            _codec_to_trackid[track->getCodecId()].track_id = mpeg_ts_add_stream(_context, PSI_STREAM_H264, nullptr, 0);
        }
            break;
        case CodecH265: {
            _have_video = true;
            _codec_to_trackid[track->getCodecId()].track_id = mpeg_ts_add_stream(_context, PSI_STREAM_H265, nullptr, 0);
        }
            break;
        case CodecAAC: {
            _codec_to_trackid[track->getCodecId()].track_id = mpeg_ts_add_stream(_context, PSI_STREAM_AAC, nullptr, 0);
        }
            break;
        default:
            break;
    }
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
        case CodecH265:
        case CodecH264: {
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