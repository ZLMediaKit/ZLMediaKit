/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_PSDECODER_H
#define ZLMEDIAKIT_PSDECODER_H

#if defined(ENABLE_RTPPROXY)
#include <stdint.h>
#include "Decoder.h"
#include "Http/HttpRequestSplitter.h"

namespace mediakit{

//ps解析器
class PSDecoder : public Decoder, private HttpRequestSplitter {
public:
    PSDecoder();
    ~PSDecoder();

    ssize_t input(const uint8_t* data, size_t bytes) override;

    // HttpRequestSplitter interface
private:
    using HttpRequestSplitter::input;
    const char *onSearchPacketTail(const char *data, size_t len) override;
    ssize_t onRecvHeader(const char *, size_t) override { return 0; };

private:
    void *_ps_demuxer = nullptr;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_PSDECODER_H
