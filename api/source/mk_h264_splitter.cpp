/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_h264_splitter.h"
#include "Http/HttpRequestSplitter.h"
#include "Extension/Factory.h"

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
    bool _have_decode_frame = false;
    onH264 _cb;
    toolkit::BufferLikeString _buffer;
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
    auto frame = Factory::getFrameFromPtr(_h265 ? CodecH265 : CodecH264, (char *)data, len, 0, 0);
    if (_have_decode_frame && (frame->decodeAble() || frame->configFrame())) {
        // 缓存中存在可解码帧，且下一帧是可解码帧或者配置帧，那么flush缓存
        _cb(_buffer.data(), _buffer.size());
        _buffer.assign(data, len);
        _have_decode_frame = frame->decodeAble();
    } else {
        // 还需要缓存
        _buffer.append(data, len);
        _have_decode_frame = _have_decode_frame || frame->decodeAble();
    }
    return 0;
}

const char *H264Splitter::onSearchPacketTail(const char *data, size_t len) {
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
