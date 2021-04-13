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

RtpReceiver::RtpReceiver() {
    int index = 0;
    for (auto &sortor : _rtp_sortor) {
        sortor.setOnSort([this, index](uint16_t seq, RtpPacket::Ptr &packet) {
            onRtpSorted(std::move(packet), index);
        });
        ++index;
    }
}

RtpReceiver::~RtpReceiver() {}

bool RtpReceiver::handleOneRtp(int index, TrackType type, int sample_rate, uint8_t *ptr, size_t len) {
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
        throw std::invalid_argument("非法的rtp，version字段非法");
    }
    if (!header->getPayloadSize(len)) {
        //无有效负载的rtp包
        return false;
    }

    //比对缓存ssrc
    auto ssrc = ntohl(header->ssrc);

    if (!_ssrc[index]) {
        //记录并锁定ssrc
        _ssrc[index] = ssrc;
        _ssrc_alive[index].resetTime();
    } else if (_ssrc[index] == ssrc) {
        //ssrc匹配正确,刷新计时器
        _ssrc_alive[index].resetTime();
    } else {
        //ssrc错误
        if (_ssrc_alive[index].elapsedTime() < 10 * 1000) {
            //接受正确ssrc的rtp在10秒内，那么我们认为存在多路rtp,忽略掉ssrc不匹配的rtp
            WarnL << "ssrc不匹配,rtp已丢弃:" << ssrc << " != " << _ssrc[index];
            return false;
        }
        InfoL << "rtp流ssrc切换:" << _ssrc[index] << " -> " << ssrc;
        _ssrc[index] = ssrc;
        _ssrc_alive[index].resetTime();
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

    onBeforeRtpSorted(rtp, index);
    auto seq = rtp->getSeq();
    _rtp_sortor[index].sortPacket(seq, std::move(rtp));
    return true;
}

void RtpReceiver::clear() {
    CLEAR_ARR(_ssrc);
    for (auto &sortor : _rtp_sortor) {
        sortor.clear();
    }
}

size_t RtpReceiver::getJitterSize(int index) const{
    return _rtp_sortor[index].getJitterSize();
}

size_t RtpReceiver::getCycleCount(int index) const{
    return _rtp_sortor[index].getCycleCount();
}

uint32_t RtpReceiver::getSSRC(int index) const{
    return _ssrc[index];
}

}//namespace mediakit
