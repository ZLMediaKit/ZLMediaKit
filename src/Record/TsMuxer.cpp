/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_HLS)

#include "TsMuxer.h"
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

bool TsMuxer::addTrack(const Track::Ptr &track) {
    switch (track->getCodecId()) {
        case CodecH264: {
            _have_video = true;
            _codec_to_trackid[track->getCodecId()] = mpeg_ts_add_stream(_context, PSI_STREAM_H264, nullptr, 0);
            break;
        }

        case CodecH265: {
            _have_video = true;
            _codec_to_trackid[track->getCodecId()] = mpeg_ts_add_stream(_context, PSI_STREAM_H265, nullptr, 0);
            break;
        }

        case CodecAAC: {
            _codec_to_trackid[track->getCodecId()] = mpeg_ts_add_stream(_context, PSI_STREAM_AAC, nullptr, 0);
            break;
        }

        case CodecG711A: {
            _codec_to_trackid[track->getCodecId()] = mpeg_ts_add_stream(_context, PSI_STREAM_AUDIO_G711A,nullptr, 0);
            break;
        }

        case CodecG711U: {
            _codec_to_trackid[track->getCodecId()] = mpeg_ts_add_stream(_context, PSI_STREAM_AUDIO_G711U,nullptr, 0);
            break;
        }

        case CodecOpus: {
            _codec_to_trackid[track->getCodecId()] = mpeg_ts_add_stream(_context, PSI_STREAM_AUDIO_OPUS,nullptr, 0);
            break;
        }

        default: WarnL << "mpeg-ts 不支持该编码格式,已忽略:" << track->getCodecName(); return false;
    }

    return true;
}

bool TsMuxer::inputFrame(const Frame::Ptr &frame) {
    auto it = _codec_to_trackid.find(frame->getCodecId());
    if (it == _codec_to_trackid.end()) {
        return false;
    }
    auto track_id = it->second;
    _is_idr_fast_packet = !_have_video;
    switch (frame->getCodecId()) {
        case CodecH264:
        case CodecH265: {
            //这里的代码逻辑是让SPS、PPS、IDR这些时间戳相同的帧打包到一起当做一个帧处理，
            return _frame_merger.inputFrame(frame,[&](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer, bool have_idr) {
                //取视频时间戳为TS的时间戳
                _timestamp = (uint32_t) dts;
                _is_idr_fast_packet = have_idr;
                mpeg_ts_write(_context, track_id, have_idr ? 0x0001 : 0, pts * 90LL,dts * 90LL, buffer->data(), buffer->size());
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
                _timestamp = (uint32_t) frame->dts();
            }
            mpeg_ts_write(_context, track_id, frame->keyFrame() ? 0x0001 : 0, frame->pts() * 90LL, frame->dts() * 90LL, frame->data(), frame->size());
            flushCache();
            return true;
        }
    }
}

void TsMuxer::resetTracks() {
    _have_video = false;
    //通知片段中断
    onTs(nullptr, _timestamp, 0);
    uninit();
    init();
}

void TsMuxer::init() {
    static mpeg_ts_func_t s_func = {
            [](void *param, size_t bytes) {
                TsMuxer *muxer = (TsMuxer *) param;
                assert(sizeof(muxer->_tsbuf) >= bytes);
                return (void *) muxer->_tsbuf;
            },
            [](void *param, void *packet) {
                //do nothing
            },
            [](void *param, const void *packet, size_t bytes) {
                TsMuxer *muxer = (TsMuxer *) param;
                muxer->onTs_l(packet, bytes);
                return 0;
            }
    };
    if (_context == nullptr) {
        _context = mpeg_ts_create(&s_func, this);
    }
}

void TsMuxer::onTs_l(const void *packet, size_t bytes) {
    if (!_cache) {
        _cache = std::make_shared<BufferLikeString>();
    }
    _cache->append((char *) packet, bytes);
}

void TsMuxer::flushCache() {
    onTs(std::move(_cache), _timestamp, _is_idr_fast_packet);
    _is_idr_fast_packet = false;
}

void TsMuxer::uninit() {
    if (_context) {
        mpeg_ts_destroy(_context);
        _context = nullptr;
    }
    _codec_to_trackid.clear();
    _frame_merger.clear();
}

}//namespace mediakit

#endif// defined(ENABLE_HLS)