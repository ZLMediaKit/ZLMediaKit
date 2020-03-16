/*
 * MIT License
 *
 * Copyright (c) 2019 Gemfield <gemfield@civilnet.cn>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(ENABLE_RTPPROXY)
#include "Util/logger.h"
#include "RtpDecoder.h"
#include "rtp-payload.h"
using namespace toolkit;

namespace mediakit{

RtpDecoder::RtpDecoder() {
    _buffer = std::make_shared<BufferRaw>();
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
        _rtp_decoder = rtp_payload_decode_create(rtp_type, "MP2P", &s_func, this);
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