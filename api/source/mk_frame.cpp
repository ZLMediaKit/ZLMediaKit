/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_frame.h"
#include "Extension/Frame.h"
#include "Extension/H264.h"
#include "Extension/H265.h"
#include "Extension/AAC.h"

using namespace mediakit;

extern "C" {
#define XX(name, type, value, str, mpeg_id) API_EXPORT const int MK##name = value;
    CODEC_MAP(XX)
#undef XX
}

class FrameFromPtrForC : public FrameFromPtr {
public:
    using Ptr = std::shared_ptr<FrameFromPtrForC>;

    template<typename ...ARGS>
    FrameFromPtrForC(bool cache_able, uint32_t flags, on_mk_frame_data_release cb, void *user_data, ARGS &&...args) : FrameFromPtr(
            std::forward<ARGS>(args)...) {
        _flags = flags;
        _cb = cb;
        _user_data = user_data;
        _cache_able = cache_able;
    }

    ~FrameFromPtrForC() override {
        if (_cb) {
            _cb(_user_data, _ptr);
        }
    }

    bool cacheAble() const override {
        return _cache_able;
    }

    bool keyFrame() const override {
        return _flags & MK_FRAME_FLAG_IS_KEY;
    }

    bool configFrame() const override {
        return _flags & MK_FRAME_FLAG_IS_CONFIG;
    }

    //默认返回false
    bool dropAble() const override {
        return _flags & MK_FRAME_FLAG_DROP_ABLE;
    }

    //默认返回true
    bool decodeAble() const override {
        return !(_flags & MK_FRAME_FLAG_NOT_DECODE_ABLE);
    }

private:
    uint32_t _flags;
    on_mk_frame_data_release _cb;
    void *_user_data;
    bool _cache_able;
};

static mk_frame mk_frame_create_complex(int codec_id, uint64_t dts, uint64_t pts, uint32_t frame_flags, size_t prefix_size,
                                       char *data, size_t size, on_mk_frame_data_release cb, void *user_data) {
    switch (codec_id) {
        case CodecH264:
            return new Frame::Ptr(new H264FrameHelper<FrameFromPtrForC>(cb, frame_flags, cb, user_data, (CodecId) codec_id,
                                                                        data, size, dts, pts, prefix_size));
        case CodecH265:
            return new Frame::Ptr(new H265FrameHelper<FrameFromPtrForC>(cb, frame_flags, cb, user_data, (CodecId) codec_id,
                                                                        data, size, dts, pts, prefix_size));
        default:
            return new Frame::Ptr(new FrameFromPtrForC(cb, frame_flags, cb, user_data, (CodecId) codec_id, data,
                                                       size, dts, pts, prefix_size));
    }
}

API_EXPORT mk_frame API_CALL mk_frame_create(int codec_id, uint64_t dts, uint64_t pts, const char *data, size_t size,
                                            on_mk_frame_data_release cb, void *user_data) {

    switch (codec_id) {
        case CodecH264:
        case CodecH265:
            return mk_frame_create_complex(codec_id, dts, pts, 0, prefixSize(data, size), (char *)data, size, cb, user_data);

        case CodecAAC: {
            int prefix = 0;
            if ((((uint8_t *) data)[0] == 0xFF && (((uint8_t *) data)[1] & 0xF0) == 0xF0) && size > ADTS_HEADER_LEN) {
                prefix = ADTS_HEADER_LEN;
            }
            return mk_frame_create_complex(codec_id, dts, pts, 0, prefix, (char *)data, size, cb, user_data);
        }

        default:
            return mk_frame_create_complex(codec_id, dts, pts, 0, 0, (char *)data, size, cb, user_data);
    }
}

API_EXPORT void API_CALL mk_frame_unref(mk_frame frame) {
    assert(frame);
    delete (Frame::Ptr *) frame;
}

API_EXPORT mk_frame API_CALL mk_frame_ref(mk_frame frame) {
    assert(frame);
    return new Frame::Ptr(Frame::getCacheAbleFrame(*((Frame::Ptr *) frame)));
}

API_EXPORT int API_CALL mk_frame_codec_id(mk_frame frame) {
    assert(frame);
    return (*((Frame::Ptr *) frame))->getCodecId();
}

API_EXPORT const char *API_CALL mk_frame_codec_name(mk_frame frame) {
    assert(frame);
    return (*((Frame::Ptr *) frame))->getCodecName();
}

API_EXPORT int API_CALL mk_frame_is_video(mk_frame frame) {
    assert(frame);
    return (*((Frame::Ptr *) frame))->getTrackType() == TrackVideo;
}

API_EXPORT const char *API_CALL mk_frame_get_data(mk_frame frame) {
    assert(frame);
    return (*((Frame::Ptr *) frame))->data();
}

API_EXPORT size_t API_CALL mk_frame_get_data_size(mk_frame frame) {
    assert(frame);
    return (*((Frame::Ptr *) frame))->size();
}

API_EXPORT size_t API_CALL mk_frame_get_data_prefix_size(mk_frame frame) {
    assert(frame);
    return (*((Frame::Ptr *) frame))->prefixSize();
}

API_EXPORT uint64_t API_CALL mk_frame_get_dts(mk_frame frame) {
    assert(frame);
    return (*((Frame::Ptr *) frame))->dts();
}

API_EXPORT uint64_t API_CALL mk_frame_get_pts(mk_frame frame) {
    assert(frame);
    return (*((Frame::Ptr *) frame))->pts();
}

API_EXPORT uint32_t API_CALL mk_frame_get_flags(mk_frame frame) {
    assert(frame);
    auto &ref = *((Frame::Ptr *) frame);
    uint32_t ret = 0;
    if (ref->keyFrame()) {
        ret &= MK_FRAME_FLAG_IS_KEY;
    }
    if (ref->configFrame()) {
        ret &= MK_FRAME_FLAG_IS_CONFIG;
    }
    if (ref->dropAble()) {
        ret &= MK_FRAME_FLAG_DROP_ABLE;
    }
    if (!ref->decodeAble()) {
        ret &= MK_FRAME_FLAG_NOT_DECODE_ABLE;
    }
    return ret;
}