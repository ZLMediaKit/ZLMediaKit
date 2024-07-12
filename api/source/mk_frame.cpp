/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_frame.h"
#include "Record/MPEG.h"
#include "Extension/Factory.h"

using namespace mediakit;

extern "C" {
#define XX(name, type, value, str, mpeg_id, mp4_id) API_EXPORT const int MK##name = value;
    CODEC_MAP(XX)
#undef XX
}

namespace {
class BufferFromPtr : public toolkit::Buffer {
public:
    BufferFromPtr(char *ptr, size_t size, on_mk_frame_data_release cb, std::shared_ptr<void> user_data) {
        _ptr = ptr;
        _size = size;
        _cb = cb;
        _user_data = std::move(user_data);
    }

    ~BufferFromPtr() override {
        _cb(_user_data.get(), _ptr);
    }

    char *data() const override { return _ptr; }
    size_t size() const override { return _size; }

private:
    char *_ptr;
    size_t _size;
    on_mk_frame_data_release _cb;
    std::shared_ptr<void> _user_data;
};
}; // namespace

static mk_frame mk_frame_create_complex(int codec_id, uint64_t dts, uint64_t pts, const char *data, size_t size,
                                        on_mk_frame_data_release cb, std::shared_ptr<void> user_data) {
    if (!cb) {
        // no cacheable
        return (mk_frame) new Frame::Ptr(Factory::getFrameFromPtr((CodecId)codec_id, data, size, dts, pts));
    }
    // cacheable
    auto buffer = std::make_shared<BufferFromPtr>((char *)data, size, cb, std::move(user_data));
    return (mk_frame) new Frame::Ptr(Factory::getFrameFromBuffer((CodecId)codec_id, std::move(buffer), dts, pts));
}

API_EXPORT mk_frame API_CALL mk_frame_create(int codec_id, uint64_t dts, uint64_t pts, const char *data, size_t size,
                                            on_mk_frame_data_release cb, void *user_data) {
    return mk_frame_create2(codec_id, dts, pts, data, size, cb, user_data, nullptr);
}

API_EXPORT mk_frame API_CALL mk_frame_create2(int codec_id, uint64_t dts, uint64_t pts, const char *data, size_t size,
                                              on_mk_frame_data_release cb, void *user_data, on_user_data_free user_data_free) {
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    return mk_frame_create_complex(codec_id, dts, pts, data, size, cb, std::move(ptr));
}

API_EXPORT void API_CALL mk_frame_unref(mk_frame frame) {
    assert(frame);
    delete (Frame::Ptr *) frame;
}

API_EXPORT mk_frame API_CALL mk_frame_ref(mk_frame frame) {
    assert(frame);
    return (mk_frame)new Frame::Ptr(Frame::getCacheAbleFrame(*((Frame::Ptr *) frame)));
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
        ret |= MK_FRAME_FLAG_IS_KEY;
    }
    if (ref->configFrame()) {
        ret |= MK_FRAME_FLAG_IS_CONFIG;
    }
    if (ref->dropAble()) {
        ret |= MK_FRAME_FLAG_DROP_ABLE;
    }
    if (!ref->decodeAble()) {
        ret |= MK_FRAME_FLAG_NOT_DECODE_ABLE;
    }
    return ret;
}

API_EXPORT mk_frame_merger API_CALL mk_frame_merger_create(int type) {
    return reinterpret_cast<mk_frame_merger>(new FrameMerger(type));
}

API_EXPORT void API_CALL mk_frame_merger_release(mk_frame_merger ctx) {
    assert(ctx);
    delete reinterpret_cast<FrameMerger *>(ctx);
}

API_EXPORT void API_CALL mk_frame_merger_clear(mk_frame_merger ctx) {
    assert(ctx);
    reinterpret_cast<FrameMerger *>(ctx)->clear();
}

API_EXPORT void API_CALL mk_frame_merger_flush(mk_frame_merger ctx) {
    assert(ctx);
    reinterpret_cast<FrameMerger *>(ctx)->flush();
}

API_EXPORT void API_CALL mk_frame_merger_input(mk_frame_merger ctx, mk_frame frame, on_mk_frame_merger cb, void *user_data) {
    assert(ctx && frame && cb);
    reinterpret_cast<FrameMerger *>(ctx)->inputFrame(*((Frame::Ptr *) frame), [cb, user_data](uint64_t dts, uint64_t pts, const toolkit::Buffer::Ptr &buffer, bool have_key_frame) {
        cb(user_data, dts, pts, (mk_buffer)(&buffer), have_key_frame);
    });
}

//////////////////////////////////////////////////////////////////////

class MpegMuxerForC : public MpegMuxer {
public:
    using onMuxer = std::function<void(const char *frame, size_t size, uint64_t timestamp, int key_pos)>;
    MpegMuxerForC(bool is_ps) : MpegMuxer(is_ps) {
        _cb = nullptr;
    }
    ~MpegMuxerForC() { MpegMuxer::flush(); };

    void setOnMuxer(onMuxer cb) {
        _cb = std::move(cb);
    }

private:
    void onWrite(std::shared_ptr<toolkit::Buffer> buffer, uint64_t timestamp, bool key_pos) override {
        if (_cb) {
            if (!buffer) {
                _cb(nullptr, 0, timestamp, key_pos);
            } else {
                _cb(buffer->data(), buffer->size(), timestamp, key_pos);
            }
        }
    }

private:
    onMuxer _cb;
};

API_EXPORT mk_mpeg_muxer API_CALL mk_mpeg_muxer_create(on_mk_mpeg_muxer_frame cb, void *user_data, int is_ps){
    assert(cb);
    auto ret = new MpegMuxerForC(is_ps);
    std::shared_ptr<void> ptr(user_data, [](void *) {});
    ret->setOnMuxer([cb, ptr, ret](const char *frame, size_t size, uint64_t timestamp, int key_pos) {
        cb(ptr.get(), reinterpret_cast<mk_mpeg_muxer>(ret), frame, size, timestamp, key_pos);
    });
    return reinterpret_cast<mk_mpeg_muxer>(ret);
}

API_EXPORT void API_CALL mk_mpeg_muxer_release(mk_mpeg_muxer ctx){
    assert(ctx);
    auto ptr = reinterpret_cast<MpegMuxerForC *>(ctx);
    delete ptr;
}

API_EXPORT void API_CALL mk_mpeg_muxer_init_track(mk_mpeg_muxer ctx, void* track) {
    assert(ctx && track);
    auto ptr = reinterpret_cast<MpegMuxerForC *>(ctx);
    ptr->addTrack(*((Track::Ptr *) track));
}

API_EXPORT void API_CALL mk_mpeg_muxer_init_complete(mk_mpeg_muxer ctx) {
    assert(ctx);
    auto ptr = reinterpret_cast<MpegMuxerForC *>(ctx);
    ptr->addTrackCompleted();
}

API_EXPORT int API_CALL mk_mpeg_muxer_input_frame(mk_mpeg_muxer ctx, mk_frame frame){
    assert(ctx && frame);
    auto ptr = reinterpret_cast<MpegMuxerForC *>(ctx);
    return ptr->inputFrame(*((Frame::Ptr *) frame));
}