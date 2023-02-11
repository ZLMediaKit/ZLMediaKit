/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_h264_splitter.h"
#include "Http/HttpRequestSplitter.h"
#include "Extension/H264.h"
#include "Extension/H265.h"

using namespace mediakit;

class H264Splitter : public HttpRequestSplitter {
public:
    using onH264 = std::function<void(const char *data, size_t len)>;
    H264Splitter(bool h265 = false) { _h265 = h265; }
    ~H264Splitter() override;
    void setOnSplitted(onH264 cb);

protected:
    ssize_t onRecvHeader(const char *data, size_t len) override;
    const char *onSearchPacketTail(const char *data, size_t len) override;

private:
    bool _h265 = false;
    onH264 _cb;
    size_t _search_pos = 0;
};

void H264Splitter::setOnSplitted(H264Splitter::onH264 cb) {
    _cb = std::move(cb);
}

H264Splitter::~H264Splitter() {
    if (remainDataSize()) {
        _cb(remainData(), remainDataSize());
    }
}

ssize_t H264Splitter::onRecvHeader(const char *data, size_t len) {
    _cb(data, len);
    return 0;
}

static const char *onSearchPacketTail_l(const char *data, size_t len) {
    for (size_t i = 2; len > 2 && i < len - 2; ++i) {
        //判断0x00 00 01
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            if (i > 0 && data[i - 1] == 0) {
                //找到0x00 00 00 01
                return data + i - 1;
            }
            return data + i;
        }
    }
    return nullptr;
}

const char *H264Splitter::onSearchPacketTail(const char *data, size_t len) {
    auto last_frame = data + _search_pos;
    auto next_frame = onSearchPacketTail_l(last_frame, len - _search_pos);
    if (!next_frame) {
        return nullptr;
    }

    auto last_frame_len = next_frame - last_frame;
    Frame::Ptr frame;
    if (_h265) {
        frame = std::make_shared<H265FrameNoCacheAble>((char *) last_frame, last_frame_len, 0, 0, prefixSize(last_frame, last_frame_len));
    } else {
        frame = std::make_shared<H264FrameNoCacheAble>((char *) last_frame, last_frame_len, 0, 0, prefixSize(last_frame, last_frame_len));
    }
    if (frame->decodeAble()) {
        _search_pos = 0;
        return next_frame;
    }
    _search_pos += last_frame_len;
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

API_EXPORT mk_h264_splitter API_CALL mk_h264_splitter_create(on_mk_h264_splitter_frame cb, void *user_data, int is_h265) {
    return mk_h264_splitter_create2(cb, user_data, nullptr, is_h265);
}

API_EXPORT mk_h264_splitter API_CALL mk_h264_splitter_create2(on_mk_h264_splitter_frame cb, void *user_data, on_user_data_free user_data_free, int is_h265) {
    assert(cb);
    auto ret = new H264Splitter(is_h265);
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    ret->setOnSplitted([cb, ptr, ret](const char *data, size_t len) {
        cb(ptr.get(), reinterpret_cast<mk_h264_splitter>(ret), data, len);
    });
    return reinterpret_cast<mk_h264_splitter>(ret);
}

API_EXPORT void API_CALL mk_h264_splitter_release(mk_h264_splitter ctx){
    assert(ctx);
    auto ptr = reinterpret_cast<H264Splitter *>(ctx);
    delete ptr;
}

API_EXPORT void API_CALL mk_h264_splitter_input_data(mk_h264_splitter ctx, const char *data, int size) {
    assert(ctx && data && size);
    auto ptr = reinterpret_cast<H264Splitter *>(ctx);
    ptr->input(data, size);
}
