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
    switch (track->getCodecId()){
        case CodecH264:
            _codecid_to_stream_id[CodecH264] = mpeg_ts_add_stream(_context,PSI_STREAM_H264, nullptr,0);
            break;
        case CodecH265:
            _codecid_to_stream_id[CodecH265] = mpeg_ts_add_stream(_context,PSI_STREAM_H265, nullptr,0);
            break;
        case CodecAAC:
            _codecid_to_stream_id[CodecAAC] = mpeg_ts_add_stream(_context,PSI_STREAM_AAC, nullptr,0);
            break;
        default:
            break;
    }
}

void TsMuxer::inputFrame(const Frame::Ptr &frame) {
    auto it = _codecid_to_stream_id.find(frame->getCodecId());
    if(it == _codecid_to_stream_id.end()){
        return;
    }
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
                        merged.append(frame->data(),frame->size());
                    });
                    merged_frame = std::make_shared<BufferString>(std::move(merged));
                }
                _timestamp = back->dts();
                mpeg_ts_write(_context, it->second, back->keyFrame() ? 0x0001 : 0, back->pts() * 90LL, back->dts() * 90LL, merged_frame->data(),  merged_frame->size());
                _frameCached.clear();
            }
            _frameCached.emplace_back(Frame::getCacheAbleFrame(frame));
        }
            break;
        default: {
            _timestamp = frame->dts();
            mpeg_ts_write(_context, it->second, frame->keyFrame() ? 0x0001 : 0, frame->pts() * 90LL, frame->dts() * 90LL, frame->data(), frame->size());
        }
            break;
    }
}

void TsMuxer::resetTracks() {
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
                muxer->onTs(packet, bytes,muxer->_timestamp,0);
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
    _codecid_to_stream_id.clear();
}

}//namespace mediakit

#endif// defined(ENABLE_HLS)