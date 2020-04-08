/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_DECODER_H
#define ZLMEDIAKIT_DECODER_H

#if defined(ENABLE_RTPPROXY)
#include <stdint.h>
#include <memory>
#include <functional>
#include "Decoder.h"
using namespace std;
namespace mediakit {

class Decoder {
public:
    typedef std::shared_ptr<Decoder> Ptr;
    typedef enum {
        decoder_ts = 0,
        decoder_ps
    }Type;

    typedef std::function<void(int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,int bytes)> onDecode;
    virtual int input(const uint8_t *data, int bytes) = 0;
    virtual void setOnDecode(const onDecode &decode) = 0;
    static Ptr createDecoder(Type type);
protected:
    Decoder() = default;
    virtual ~Decoder() = default;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_DECODER_H
