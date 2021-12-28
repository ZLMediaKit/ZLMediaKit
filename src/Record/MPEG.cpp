/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include <assert.h>
#include "MPEG.h"

#if defined(ENABLE_HLS) || defined(ENABLE_RTPPROXY)

#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"

namespace mediakit{

struct mpeg_muxer_t {
    int is_ps;
    union {
        struct {
            void *ctx;
            void *param;
            mpeg_muxer_func_t func;
        } ts;
        ps_muxer_t *ps;
    } u;
};

static void *on_mpeg_ts_alloc(void *param, size_t bytes) {
    mpeg_muxer_t *mpeg = (mpeg_muxer_t *) param;
    return mpeg->u.ts.func.alloc(mpeg->u.ts.param, bytes);
}

static void on_mpeg_ts_free(void *param, void *packet) {
    mpeg_muxer_t *mpeg = (mpeg_muxer_t *) param;
    mpeg->u.ts.func.free(mpeg->u.ts.param, packet);
}

static int on_mpeg_ts_write(void *param, const void *packet, size_t bytes) {
    mpeg_muxer_t *mpeg = (mpeg_muxer_t *) param;
    return mpeg->u.ts.func.write(mpeg->u.ts.param, 0, (void *) packet, bytes);
}

mpeg_muxer_t *mpeg_muxer_create(int is_ps, const mpeg_muxer_func_t *func, void *param) {
    mpeg_muxer_t *mpeg = (mpeg_muxer_t *) malloc(sizeof(mpeg_muxer_t));
    assert(mpeg);
    mpeg->is_ps = is_ps;
    if (is_ps) {
        mpeg->u.ps = ps_muxer_create(func, param);
    } else {
        struct mpeg_ts_func_t ts_func{on_mpeg_ts_alloc, on_mpeg_ts_free, on_mpeg_ts_write};
        mpeg->u.ts.func = *func;
        mpeg->u.ts.param = param;
        mpeg->u.ts.ctx = mpeg_ts_create(&ts_func, mpeg);
    }
    return mpeg;
}

int mpeg_muxer_destroy(mpeg_muxer_t *muxer) {
    assert(muxer);
    int ret = -1;
    if (muxer->is_ps) {
        ret = ps_muxer_destroy(muxer->u.ps);
    } else {
        ret = mpeg_ts_destroy(muxer->u.ts.ctx);
    }
    free(muxer);
    return ret;
}

int mpeg_muxer_add_stream(mpeg_muxer_t *muxer, int codecid, const void *extradata, size_t extradata_size) {
    assert(muxer);
    if (muxer->is_ps) {
        return ps_muxer_add_stream(muxer->u.ps, codecid, extradata, extradata_size);
    }
    return mpeg_ts_add_stream(muxer->u.ts.ctx, codecid, extradata, extradata_size);
}

int mpeg_muxer_input(mpeg_muxer_t *muxer, int stream, int flags, int64_t pts, int64_t dts, const void *data, size_t bytes) {
    assert(muxer);
    if (muxer->is_ps) {
        return ps_muxer_input(muxer->u.ps, stream, flags, pts, dts, data, bytes);
    }
    return mpeg_ts_write(muxer->u.ts.ctx, stream, flags, pts, dts, data, bytes);
}

int mpeg_muxer_reset(mpeg_muxer_t *muxer) {
    assert(muxer);
    if (muxer->is_ps) {
        return -1;
    }
    return mpeg_ts_reset(muxer->u.ts.ctx);
}

int mpeg_muxer_add_program(mpeg_muxer_t *muxer, uint16_t pn, const void *info, int bytes) {
    assert(muxer);
    if (muxer->is_ps) {
        return -1;
    }
    return mpeg_ts_add_program(muxer->u.ts.ctx, pn, info, bytes);
}

int mpeg_muxer_remove_program(mpeg_muxer_t *muxer, uint16_t pn) {
    assert(muxer);
    if (muxer->is_ps) {
        return -1;
    }
    return mpeg_ts_remove_program(muxer->u.ts.ctx, pn);
}

int peg_muxer_add_program_stream(mpeg_muxer_t *muxer, uint16_t pn, int codecid, const void *extra_data, size_t extra_data_size) {
    assert(muxer);
    if (muxer->is_ps) {
        return -1;
    }
    return mpeg_ts_add_program_stream(muxer->u.ts.ctx, pn, codecid, extra_data, extra_data_size);
}

//////////////////////////////////////////////////////////////////////////////////////////

MpegMuxer::MpegMuxer(bool is_ps) {
    _is_ps = is_ps;
    _buffer = BufferRaw::create();
    createContext();
}

MpegMuxer::~MpegMuxer() {
    releaseContext();
}

#define XX(name, type, value, str, mpeg_id)                                                            \
    case name : {                                                                                      \
        if (mpeg_id == PSI_STREAM_RESERVED) {                                                          \
            break;                                                                                     \
        }                                                                                              \
        _codec_to_trackid[track->getCodecId()] = mpeg_muxer_add_stream(_context, mpeg_id, nullptr, 0); \
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
            return _frame_merger.inputFrame(frame,[&](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer, bool have_idr) {
                _key_pos = have_idr;
                //取视频时间戳为TS的时间戳
                _timestamp = (uint32_t) dts;
                mpeg_muxer_input(_context, track_id, have_idr ? 0x0001 : 0, pts * 90LL,dts * 90LL, buffer->data(), buffer->size());
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
            mpeg_muxer_input(_context, track_id, frame->keyFrame() ? 0x0001 : 0, frame->pts() * 90LL, frame->dts() * 90LL, frame->data(), frame->size());
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
                MpegMuxer *thiz = (MpegMuxer *) param;
                thiz->_buffer->setCapacity(bytes + 1);
                return (void *) thiz->_buffer->data();
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
        _context = mpeg_muxer_create(_is_ps, &func, this);
    }
}

void MpegMuxer::onWrite_l(const void *packet, size_t bytes) {
    if (!_cache) {
        _cache = std::make_shared<BufferLikeString>();
    }
    _cache->append((char *) packet, bytes);
}

void MpegMuxer::flushCache() {
    if (!_cache || _cache->empty()) {
        return;
    }
    onWrite(std::move(_cache), _timestamp, _key_pos);
    _key_pos = false;
}

void MpegMuxer::releaseContext() {
    if (_context) {
        mpeg_muxer_destroy(_context);
        _context = nullptr;
    }
    _codec_to_trackid.clear();
    _frame_merger.clear();
}

}//mediakit

#endif