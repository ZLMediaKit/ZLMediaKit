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

using namespace mediakit;

class H264Splitter : public HttpRequestSplitter {
public:
    using onH264 = std::function<void(const char *data, size_t len)>;
    H264Splitter() = default;
    ~H264Splitter() override;
    void setOnSplitted(onH264 cb);

protected:
    ssize_t onRecvHeader(const char *data, size_t len) override;
    const char *onSearchPacketTail(const char *data, size_t len) override;

private:
    onH264 _cb;
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

API_EXPORT mk_h264_splitter API_CALL mk_h264_splitter_create(on_mk_h264_splitter_frame cb, void *user_data) {
    assert(cb);
    auto ptr = new H264Splitter();
    ptr->setOnSplitted([cb, ptr, user_data](const char *data, size_t len) {
        cb(user_data, reinterpret_cast<mk_h264_splitter>(ptr), data, len);
    });
    return reinterpret_cast<mk_h264_splitter>(ptr);
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
