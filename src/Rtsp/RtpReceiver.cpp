/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Common/config.h"
#include "RtpReceiver.h"

#define RTP_MAX_SIZE (10 * 1024)

namespace mediakit {

RtpTrack::RtpTrack() {
    setOnSort([this](uint16_t seq, RtpPacket::Ptr &packet) {
        onRtpSorted(std::move(packet));
    });
}

uint32_t RtpTrack::getSSRC() const {
    return _ssrc;
}

void RtpTrack::clear() {
    _ssrc = 0;
    _ssrc_alive.resetTime();
    PacketSortor<RtpPacket::Ptr>::clear();
}

bool RtpTrack::inputRtp(TrackType type, int sample_rate, uint8_t *ptr, size_t len) {
    if (len < RtpPacket::kRtpHeaderSize) {
        WarnL << "rtp包太小:" << len;
        return false;
    }
    if (len > RTP_MAX_SIZE) {
        WarnL << "超大的rtp包:" << len << " > " << RTP_MAX_SIZE;
        return false;
    }
    if (!sample_rate) {
        //无法把时间戳转换成毫秒
        return false;
    }
    RtpHeader *header = (RtpHeader *) ptr;
    if (header->version != RtpPacket::kRtpVersion) {
        throw BadRtpException("非法的rtp，version字段非法");
    }
    if (!header->getPayloadSize(len)) {
        //无有效负载的rtp包
        return false;
    }

    //比对缓存ssrc
    auto ssrc = ntohl(header->ssrc);

    if (!_ssrc) {
        //记录并锁定ssrc
        _ssrc = ssrc;
        _ssrc_alive.resetTime();
    } else if (_ssrc == ssrc) {
        //ssrc匹配正确,刷新计时器
        _ssrc_alive.resetTime();
    } else {
        //ssrc错误
        if (_ssrc_alive.elapsedTime() < 3 * 1000) {
            //接受正确ssrc的rtp在10秒内，那么我们认为存在多路rtp,忽略掉ssrc不匹配的rtp
            WarnL << "ssrc不匹配,rtp已丢弃:" << ssrc << " != " << _ssrc;
            return false;
        }
        InfoL << "rtp流ssrc切换:" << _ssrc << " -> " << ssrc;
        _ssrc = ssrc;
        _ssrc_alive.resetTime();
    }

    auto rtp = RtpPacket::create();
    //需要添加4个字节的rtp over tcp头
    rtp->setCapacity(RtpPacket::kRtpTcpHeaderSize + len);
    rtp->setSize(RtpPacket::kRtpTcpHeaderSize + len);
    rtp->sample_rate = sample_rate;
    rtp->type = type;

    //赋值4个字节的rtp over tcp头
    uint8_t *data = (uint8_t *) rtp->data();
    data[0] = '$';
    data[1] = 2 * type;
    data[2] = (len >> 8) & 0xFF;
    data[3] = len & 0xFF;
    //拷贝rtp
    memcpy(&data[4], ptr, len);

    onBeforeRtpSorted(rtp);
    auto seq = rtp->getSeq();
    sortPacket(seq, std::move(rtp));
    return true;
}

////////////////////////////////////////////////////////////////////////////////////

void RtpTrackImp::setOnSorted(OnSorted cb) {
    _on_sorted = std::move(cb);
}

void RtpTrackImp::setBeforeSorted(BeforeSorted cb) {
    _on_before_sorted = std::move(cb);
}

void RtpTrackImp::onRtpSorted(RtpPacket::Ptr rtp) {
    if (_on_sorted) {
        _on_sorted(std::move(rtp));
    }
}

void RtpTrackImp::onBeforeRtpSorted(const RtpPacket::Ptr &rtp) {
    if (_on_before_sorted) {
        _on_before_sorted(rtp);
    }
}

}//namespace mediakit
