/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtpCodec.h"

namespace mediakit{

RtpPacket::Ptr RtpInfo::makeRtp(TrackType type, const void* data, size_t len, bool mark, uint32_t stamp) {
    uint16_t payload_len = (uint16_t)(len + 12);
    uint32_t ts = htonl((_ui32SampleRate / 1000) * stamp);
    uint16_t sq = htons(_ui16Sequence);
    uint32_t sc = htonl(_ui32Ssrc);

    auto rtp_ptr = ResourcePoolHelper<RtpPacket>::obtainObj();
    rtp_ptr->setCapacity(len + 16);
    rtp_ptr->setSize(len + 16);

    auto *rtp = (unsigned char *)rtp_ptr->data();
    rtp[0] = '$';
    rtp[1] = _ui8Interleaved;
    rtp[2] = payload_len >> 8;
    rtp[3] = payload_len & 0xFF;
    rtp[4] = 0x80;
    rtp[5] = (mark << 7) | _ui8PayloadType;
    memcpy(&rtp[6], &sq, 2);
    memcpy(&rtp[8], &ts, 4);
    //ssrc
    memcpy(&rtp[12], &sc, 4);

    if(data){
        //payload
        memcpy(&rtp[16], data, len);
    }

    rtp_ptr->PT = _ui8PayloadType;
    rtp_ptr->interleaved = _ui8Interleaved;
    rtp_ptr->mark = mark;
    rtp_ptr->sequence = _ui16Sequence;
    rtp_ptr->timeStamp = stamp;
    rtp_ptr->ssrc = _ui32Ssrc;
    rtp_ptr->type = type;
    rtp_ptr->offset = 16;
    _ui16Sequence++;
    _ui32TimeStamp = stamp;
    return rtp_ptr;
}

}//namespace mediakit


