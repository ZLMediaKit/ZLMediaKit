/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
    pucRtp[5] = (mark << 7) | _ui8PayloadType;
    memcpy(&pucRtp[6], &sq, 2);
    memcpy(&pucRtp[8], &ts, 4);
    //ssrc
    memcpy(&pucRtp[12], &sc, 4);

    if(data){
        //payload
        memcpy(&pucRtp[16], data, len);
    }

    rtppkt->PT = _ui8PayloadType;
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


