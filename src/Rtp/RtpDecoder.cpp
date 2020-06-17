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
#include "Util/logger.h"
#include "RtpDecoder.h"
#include "rtp-payload.h"
using namespace toolkit;

namespace mediakit{

RtpDecoder::RtpDecoder(const char *codec) {
    _buffer = std::make_shared<BufferRaw>();
    _codec = codec;
}

RtpDecoder::~RtpDecoder() {
    if(_rtp_decoder){
        rtp_payload_decode_destroy(_rtp_decoder);
        _rtp_decoder = nullptr;
    }
}

void RtpDecoder::decodeRtp(const void *data, int bytes) {
    if(!_rtp_decoder){
        static rtp_payload_t s_func= {
                [](void* param, int bytes){
                    RtpDecoder *obj = (RtpDecoder *)param;
                    obj->_buffer->setCapacity(bytes);
                    return (void *)obj->_buffer->data();
                },
                [](void* param, void* packet){
                    //do nothing
                },
                [](void* param, const void *packet, int bytes, uint32_t timestamp, int flags){
                    RtpDecoder *obj = (RtpDecoder *)param;
                    obj->onRtpDecode((uint8_t *)packet, bytes, timestamp, flags);
                }
        };

        uint8_t rtp_type = 0x7F & ((uint8_t *) data)[1];
        InfoL << "rtp type:" << (int) rtp_type;
        _rtp_decoder = rtp_payload_decode_create(rtp_type, _codec.data(), &s_func, this);
        if (!_rtp_decoder) {
            WarnL << "unsupported rtp type:" << (int) rtp_type << ",size:" << bytes << ",hexdump" << hexdump(data, bytes > 16 ? 16 : bytes);
        }
    }

    if(_rtp_decoder){
        rtp_payload_decode_input(_rtp_decoder,data,bytes);
    }
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)