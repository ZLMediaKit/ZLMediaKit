/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#include "RtpCodec.h"

namespace mediakit{

RtpPacket::Ptr RtpInfo::makeRtp(TrackType type, const void* data, unsigned int len, bool mark, uint32_t uiStamp) {
    uint16_t ui16RtpLen = len + 12;
    uint32_t ts = htonl((_ui32SampleRate / 1000) * uiStamp);
    uint16_t sq = htons(_ui16Sequence);
    uint32_t sc = htonl(_ui32Ssrc);

    auto rtppkt = ResourcePoolHelper<RtpPacket>::obtainObj();
    rtppkt->setCapacity(len + 16);
    rtppkt->setSize(len + 16);

    unsigned char *pucRtp = (unsigned char *)rtppkt->data();
    pucRtp[0] = '$';
    pucRtp[1] = _ui8Interleaved;
    pucRtp[2] = ui16RtpLen >> 8;
    pucRtp[3] = ui16RtpLen & 0x00FF;
    pucRtp[4] = 0x80;
    pucRtp[5] = (mark << 7) | _ui8PlayloadType;
    memcpy(&pucRtp[6], &sq, 2);
    memcpy(&pucRtp[8], &ts, 4);
    //ssrc
    memcpy(&pucRtp[12], &sc, 4);

    if(data){
        //playload
        memcpy(&pucRtp[16], data, len);
    }

    rtppkt->PT = _ui8PlayloadType;
    rtppkt->interleaved = _ui8Interleaved;
    rtppkt->mark = mark;
    rtppkt->sequence = _ui16Sequence;
    rtppkt->timeStamp = uiStamp;
    rtppkt->ssrc = _ui32Ssrc;
    rtppkt->type = type;
    rtppkt->offset = 16;
    _ui16Sequence++;
    _ui32TimeStamp = uiStamp;
    return rtppkt;
}

}//namespace mediakit


