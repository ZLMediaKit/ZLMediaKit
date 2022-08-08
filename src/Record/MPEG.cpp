/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include "MPEG.h"

#if defined(ENABLE_HLS) || defined(ENABLE_RTPPROXY)

#include "mpeg-ts-proto.h"
#include "mpeg-muxer.h"

using namespace toolkit;

namespace mediakit{

MpegMuxer::MpegMuxer(bool is_ps) {
    _is_ps = is_ps;
    createContext();
    _buffer_pool.setSize(64);
}

MpegMuxer::~MpegMuxer() {
    releaseContext();
}

#define XX(name, type, value, str, mpeg_id)                                                            \
    case name : {                                                                                      \
        if (mpeg_id == PSI_STREAM_RESERVED) {                                                          \
            break;                                                                                     \
        }                                                                                              \
        _codec_to_trackid[track->getCodecId()] = mpeg_muxer_add_stream((::mpeg_muxer_t *)_context, mpeg_id, nullptr, 0); \
        return true;                                                                                   \
    }

bool MpegMuxer::addTrack(const Track::Ptr &track) {
    if (track->getTrackType() == TrackVideo) {
        _have_video = true;
    }
    switch (track->getCodecId()) {
        CODEC_MAP(XX)
        default: break;
    }
    WarnL << "不支持该编码格式,已忽略:" << track->getCodecName();
    return false;
}
#undef XX

bool MpegMuxer::inputFrame(const Frame::Ptr &frame) {
    auto it = _codec_to_trackid.find(frame->getCodecId());
    if (it == _codec_to_trackid.end()) {
        return false;
    }
    auto track_id = it->second;
    _key_pos = !_have_video;
    switch (frame->getCodecId()) {
        case CodecH264:
        case CodecH265: {
            //这里的代码逻辑是让SPS、PPS、IDR这些时间戳相同的帧打包到一起当做一个帧处理，
            return _frame_merger.inputFrame(frame,[&](uint64_t dts, uint64_t pts, const Buffer::Ptr &buffer, bool have_idr) {
                _key_pos = have_idr;
                //取视频时间戳为TS的时间戳
                _timestamp = dts;
                _max_cache_size = 512 + 1.2 * buffer->size();
                mpeg_muxer_input((::mpeg_muxer_t *)_context, track_id, have_idr ? 0x0001 : 0, pts * 90LL,dts * 90LL, buffer->data(), buffer->size());
                flushCache();
            });
        }

        case CodecAAC: {
            if (frame->prefixSize() == 0) {
                WarnL << "必须提供adts头才能mpeg-ts打包";
                return false;
            }
        }

        default: {
            if (!_have_video) {
                //没有视频时，才以音频时间戳为TS的时间戳
                _timestamp = frame->dts();
            }
            _max_cache_size = 512 + 1.2 * frame->size();
            mpeg_muxer_input((::mpeg_muxer_t *)_context, track_id, frame->keyFrame() ? 0x0001 : 0, frame->pts() * 90LL, frame->dts() * 90LL, frame->data(), frame->size());
            flushCache();
            return true;
        }
    }
}

void MpegMuxer::resetTracks() {
    _have_video = false;
    //通知片段中断
    onWrite(nullptr, _timestamp, false);
    releaseContext();
    createContext();
}

void MpegMuxer::createContext() {
    static mpeg_muxer_func_t func = {
            /*alloc*/
            [](void *param, size_t bytes) {
                MpegMuxer *thiz = (MpegMuxer *)param;
                if (!thiz->_current_buffer
                    || thiz->_current_buffer->size() + bytes > thiz->_current_buffer->getCapacity()) {
                    if (thiz->_current_buffer) {
                        //WarnL << "need realloc mpeg buffer" << thiz->_current_buffer->size() + bytes  << " > " << thiz->_current_buffer->getCapacity();
                        thiz->flushCache();
                    }
                    thiz->_current_buffer = thiz->_buffer_pool.obtain2();
                    thiz->_current_buffer->setSize(0);
                    thiz->_current_buffer->setCapacity(MAX(thiz->_max_cache_size, bytes));
                }
                return (void *)(thiz->_current_buffer->data() + thiz->_current_buffer->size());
            },
            /*free*/
            [](void *param, void *packet) {
                //什么也不做
            },
            /*wtite*/
            [](void *param, int stream, void *packet, size_t bytes) {
                MpegMuxer *thiz = (MpegMuxer *) param;
                thiz->onWrite_l(packet, bytes);
                return 0;
            }
    };
    if (_context == nullptr) {
        _context = (struct mpeg_muxer_t *)mpeg_muxer_create(_is_ps, &func, this);
    }
}

void MpegMuxer::onWrite_l(const void *packet, size_t bytes) {
    assert(_current_buffer && _current_buffer->data() + _current_buffer->size() == packet);
    _current_buffer->setSize(_current_buffer->size() + bytes);
}

void MpegMuxer::flushCache() {
    onWrite(std::move(_current_buffer), _timestamp, _key_pos);
    _key_pos = false;
}

void MpegMuxer::releaseContext() {
    if (_context) {
        mpeg_muxer_destroy((::mpeg_muxer_t *)_context);
        _context = nullptr;
    }
    _codec_to_trackid.clear();
    _frame_merger.clear();
}

}//mediakit

#endif