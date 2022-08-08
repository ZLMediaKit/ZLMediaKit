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

RtpPacket::Ptr RtpInfo::makeRtp(TrackType type, const void* data, size_t len, bool mark, uint64_t stamp) {
    uint16_t payload_len = (uint16_t) (len + RtpPacket::kRtpHeaderSize);
    auto rtp = RtpPacket::create();
    rtp->setCapacity(payload_len + RtpPacket::kRtpTcpHeaderSize);
    rtp->setSize(payload_len + RtpPacket::kRtpTcpHeaderSize);
    rtp->sample_rate = _sample_rate;
    rtp->type = type;

    //rtsp over tcp 头
    auto ptr = (uint8_t *) rtp->data();
    ptr[0] = '$';
    ptr[1] = _interleaved;
    ptr[2] = payload_len >> 8;
    ptr[3] = payload_len & 0xFF;

    //rtp头
    auto header = rtp->getHeader();
    header->version = RtpPacket::kRtpVersion;
    header->padding = 0;
    header->ext = 0;
    header->csrc = 0;
    header->mark = mark;
    header->pt = _pt;
    header->seq = htons(_seq);
    ++_seq;
    header->stamp = htonl(uint64_t(stamp) * _sample_rate / 1000);
    header->ssrc = htonl(_ssrc);

    //有效负载
    if (data) {
        memcpy(&ptr[RtpPacket::kRtpHeaderSize + RtpPacket::kRtpTcpHeaderSize], data, len);
    }
    return rtp;
}

}//namespace mediakit


