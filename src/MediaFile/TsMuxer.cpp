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
        case CodecH264: {
            track_info info;
            info.track_id = mpeg_ts_add_stream(_context, PSI_STREAM_H264, nullptr, 0);
            _codec_to_trackid[track->getCodecId()] = info;
        } break;
        case CodecH265: {
            track_info info;
            info.track_id = mpeg_ts_add_stream(_context, PSI_STREAM_H265, nullptr, 0);
            _codec_to_trackid[track->getCodecId()] = info;
        }break;
        case CodecAAC: {
            track_info info;
            info.track_id = mpeg_ts_add_stream(_context, PSI_STREAM_AAC, nullptr, 0);
            _codec_to_trackid[track->getCodecId()] = info;
        }break;
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

    switch (frame->getCodecId()){
        case CodecH265:
        case CodecH264: {

            Buffer::Ptr merged_frame ;
            if(frame->configFrame()){
                //配置帧,缓存后直接返回，以便下次输入关键帧时使用
                _config_frame_cache.append("\x00\x00\x00\x01",4);
                _config_frame_cache.append(frame->data() + frame->prefixSize(),frame->size() - frame->prefixSize());
                break;
            }

            if(frame->keyFrame()){
                //关键帧
                if(!_config_frame_cache.empty()){
                    //有配置帧,那么配置帧合并关键帧后输入ts打包
                    _config_frame_cache.append("\x00\x00\x00\x01",4);
                    _config_frame_cache.append(frame->data() + frame->prefixSize(),frame->size() - frame->prefixSize());
                    merged_frame = std::make_shared<BufferString>(std::move(_config_frame_cache));
                    _config_frame_cache.clear();
                }else{
                    //这是非第一个的关键帧(h265有多种关键帧)
                    merged_frame = frame;
                }
            }else{
                //这里是普通帧，例如B/P，
                merged_frame = frame;
                //sps、pps这些配置帧清空掉
                _config_frame_cache.clear();
            }

            //输入到ts文件
            track_info.stamp.revise(frame->dts(),frame->pts(),dts_out,pts_out);
            _timestamp = dts_out;
            mpeg_ts_write(_context, track_info.track_id, frame->keyFrame() ? 0x0001 : 0, pts_out * 90LL, dts_out * 90LL, merged_frame->data(),  merged_frame->size());
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
    _codec_to_trackid.clear();
}

}//namespace mediakit

#endif// defined(ENABLE_HLS)