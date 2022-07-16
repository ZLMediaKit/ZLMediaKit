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

RtpPacket::Ptr RtpTrack::inputRtp(TrackType type, int sample_rate, uint8_t *ptr, size_t len) {
    if (len < RtpPacket::kRtpHeaderSize) {
        throw BadRtpException("rtp size less than 12");
    }
    GET_CONFIG(uint32_t, rtpMaxSize, Rtp::kRtpMaxSize);
    if (len > 1024 * rtpMaxSize) {
        WarnL << "超大的rtp包:" << len << " > " << 1024 * rtpMaxSize;
        return nullptr;
    }
    if (!sample_rate) {
        //无法把时间戳转换成毫秒
        return nullptr;
    }
    RtpHeader *header = (RtpHeader *) ptr;
    if (header->version != RtpPacket::kRtpVersion) {
        throw BadRtpException("invalid rtp version");
    }
    if (header->getPayloadSize(len) < 0) {
        //rtp有效负载小于0，非法
        throw BadRtpException("invalid rtp payload size");
    }

    //比对缓存ssrc
    auto ssrc = ntohl(header->ssrc);

    if (_pt == 0xFF) {
        _pt = header->pt;
    } else if (header->pt != _pt) {
        //TraceL << "rtp pt mismatch:" << (int) header->pt << " !=" << (int) _pt;
        return nullptr;
    }

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
            WarnL << "ssrc mismatch, rtp dropped:" << ssrc << " != " << _ssrc;
            return nullptr;
        }
        InfoL << "rtp ssrc changed:" << _ssrc << " -> " << ssrc;
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
    if (_disable_ntp) {
        //不支持ntp时间戳，例如国标推流，那么直接使用rtp时间戳
        rtp->ntp_stamp = rtp->getStamp() * uint64_t(1000) / sample_rate;
    } else {
        //设置ntp时间戳
        rtp->ntp_stamp = _ntp_stamp.getNtpStamp(rtp->getStamp(), sample_rate);
    }
    onBeforeRtpSorted(rtp);
    sortPacket(rtp->getSeq(), rtp);
    return rtp;
}

void RtpTrack::setNtpStamp(uint32_t rtp_stamp, uint64_t ntp_stamp_ms) {
    _disable_ntp = rtp_stamp == 0 && ntp_stamp_ms == 0;
    if (!_disable_ntp) {
        _ntp_stamp.setNtpStamp(rtp_stamp, ntp_stamp_ms);
    }
}

void RtpTrack::setPT(uint8_t pt){
    _pt = pt;
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
