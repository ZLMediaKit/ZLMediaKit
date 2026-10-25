/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtpCodec.h"
#include <limits>
#include <stdexcept>

namespace mediakit{

RtpPacket::Ptr RtpInfo::makeRtp(TrackType type, const void* data, size_t len, bool mark, uint64_t stamp) {
    // Validate before addition, allocation or header writes, including data == nullptr.
    if (len > (std::numeric_limits<uint16_t>::max)() - RtpPacket::kRtpHeaderSize) {
        throw std::length_error("RTP payload exceeds the RTSP interleaved length limit");
    }
    uint16_t payload_len = (uint16_t) (len + RtpPacket::kRtpHeaderSize);
    auto rtp = RtpPacket::create();
    rtp->setCapacity(payload_len + RtpPacket::kRtpTcpHeaderSize);
    rtp->setSize(payload_len + RtpPacket::kRtpTcpHeaderSize);
    rtp->sample_rate = _sample_rate;
    rtp->type = type;
    rtp->track_index = _track_index;

    // rtsp over tcp 头  [AUTO-TRANSLATED:4225b9ec]
    // rtsp over tcp header
    auto ptr = (uint8_t *) rtp->data();
    ptr[0] = '$';
    ptr[1] = _interleaved;
    ptr[2] = payload_len >> 8;
    ptr[3] = payload_len & 0xFF;

    // rtp头  [AUTO-TRANSLATED:64aef747]
    // rtp header
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
    rtp->ntp_stamp = stamp;
    // 有效负载  [AUTO-TRANSLATED:8530a274]
    // payload
    if (data) {
        memcpy(&ptr[RtpPacket::kRtpHeaderSize + RtpPacket::kRtpTcpHeaderSize], data, len);
    }
    return rtp;
}

}//namespace mediakit


