/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "PSDecoder.h"
#include "mpeg-ps.h"
namespace mediakit{

PSDecoder::PSDecoder() {
    _ps_demuxer = ps_demuxer_create([](void* param,
                                       int stream,
                                       int codecid,
                                       int flags,
                                       int64_t pts,
                                       int64_t dts,
                                       const void* data,
                                       size_t bytes){
        PSDecoder *thiz = (PSDecoder *)param;
        if(thiz->_on_decode){
            thiz->_on_decode(stream, codecid, flags, pts, dts, data, bytes);
        }
    },this);
}

PSDecoder::~PSDecoder() {
    ps_demuxer_destroy((struct ps_demuxer_t*)_ps_demuxer);
}

int PSDecoder::input(const uint8_t *data, int bytes) {
    return ps_demuxer_input((struct ps_demuxer_t*)_ps_demuxer,data,bytes);
}

void PSDecoder::setOnDecode(const Decoder::onDecode &decode) {
    _on_decode = decode;
}

}//namespace mediakit
#endif//#if defined(ENABLE_RTPPROXY)