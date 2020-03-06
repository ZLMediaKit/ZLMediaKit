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
#include "RtpSplitter.h"
namespace mediakit{

RtpSplitter::RtpSplitter() {
}

RtpSplitter::~RtpSplitter() {
}

const char *RtpSplitter::onSearchPacketTail(const char *data, int len) {
    //这是rtp包
    if(len < 2){
        //数据不够
        return nullptr;
    }
    uint16_t length = (((uint8_t *)data)[0] << 8) | ((uint8_t *)data)[1];
    if(len < length + 2){
        //数据不够
        return nullptr;
    }
    //返回rtp包末尾
    return data + 2 + length;
}

int64_t RtpSplitter::onRecvHeader(const char *data, uint64_t len) {
    onRtpPacket(data,len);
    return 0;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)