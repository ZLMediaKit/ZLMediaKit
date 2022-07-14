/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtcpContext.h"
#include "Util/logger.h"
using namespace toolkit;

namespace mediakit {

void RtcpContext::onRtp(
    uint16_t /*seq*/, uint32_t stamp, uint64_t ntp_stamp_ms, uint32_t /*sample_rate*/, size_t bytes) {
    ++_packets;
    _bytes += bytes;
    _last_rtp_stamp = stamp;
    _last_ntp_stamp_ms = ntp_stamp_ms;
}

size_t RtcpContext::getExpectedPackets() const {
    throw std::runtime_error("没有实现, rtp发送者无法统计应收包数");
}

size_t RtcpContext::getExpectedPacketsInterval() {
    throw std::runtime_error("没有实现, rtp发送者无法统计应收包数");
}

size_t RtcpContext::getLost() {
    throw std::runtime_error("没有实现, rtp发送者无法统计丢包率");
}

size_t RtcpContext::geLostInterval() {
    throw std::runtime_error("没有实现, rtp发送者无法统计丢包率");
}

Buffer::Ptr RtcpContext::createRtcpSR(uint32_t rtcp_ssrc) {
    throw std::runtime_error("没有实现, rtp接收者尝试发送sr包");
}

Buffer::Ptr RtcpContext::createRtcpRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc) {
    throw std::runtime_error("没有实现, rtp发送者尝试发送rr包");
}

Buffer::Ptr RtcpContext::createRtcpXRDLRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc) {
    throw std::runtime_error("没有实现, rtp发送者尝试发送xr dlrr包");
}

////////////////////////////////////////////////////////////////////////////////////

void RtcpContextForSend::onRtcp(RtcpHeader *rtcp) {
    switch ((RtcpType)rtcp->pt) {
    case RtcpType::RTCP_RR: {
        auto rtcp_rr = (RtcpRR *)rtcp;
        for (auto item : rtcp_rr->getItemList()) {
            if (!item->last_sr_stamp) {
                continue;
            }
            auto it = _sender_report_ntp.find(item->last_sr_stamp);
            if (it == _sender_report_ntp.end()) {
                continue;
            }
            // 发送sr到收到rr之间的时间戳增量
            auto ms_inc = getCurrentMillisecond() - it->second;
            // rtp接收端收到sr包后，回复rr包的延时，已转换为毫秒
            auto delay_ms = (uint64_t)item->delay_since_last_sr * 1000 / 65536;
            auto rtt = (int)(ms_inc - delay_ms);
            if (rtt >= 0) {
                // rtt不可能小于0
                _rtt[item->ssrc] = rtt;
                // InfoL << "ssrc:" << item->ssrc << ",rtt:" << rtt;
            }
        }
        break;
    }
    case RtcpType::RTCP_XR: {
        auto rtcp_xr = (RtcpXRRRTR *)rtcp;
        if (rtcp_xr->bt == 4) {
            _xr_xrrtr_recv_last_rr[rtcp_xr->ssrc]
                = ((rtcp_xr->ntpmsw & 0xFFFF) << 16) | ((rtcp_xr->ntplsw >> 16) & 0xFFFF);
            _xr_rrtr_recv_sys_stamp[rtcp_xr->ssrc] = getCurrentMillisecond();
        } else if (rtcp_xr->bt == 5) {
            TraceL << "for sender not recive dlrr";
        } else {
            TraceL << "not support xr bt " << rtcp_xr->bt;
        }
        break;
    }
    default:
        break;
    }
}

uint32_t RtcpContextForSend::getRtt(uint32_t ssrc) const {
    auto it = _rtt.find(ssrc);
    if (it == _rtt.end()) {
        return 0;
    }
    return it->second;
}

Buffer::Ptr RtcpContextForSend::createRtcpSR(uint32_t rtcp_ssrc) {
    auto rtcp = RtcpSR::create(0);
    rtcp->setNtpStamp(_last_ntp_stamp_ms);
    rtcp->rtpts = htonl(_last_rtp_stamp);
    rtcp->ssrc = htonl(rtcp_ssrc);
    rtcp->packet_count = htonl((uint32_t)_packets);
    rtcp->octet_count = htonl((uint32_t)_bytes);

    // 记录上次发送的sender report信息，用于后续统计rtt
    auto last_sr_lsr = ((ntohl(rtcp->ntpmsw) & 0xFFFF) << 16) | ((ntohl(rtcp->ntplsw) >> 16) & 0xFFFF);
    _sender_report_ntp[last_sr_lsr] = getCurrentMillisecond();
    if (_sender_report_ntp.size() >= 5) {
        // 删除最早的sr rtcp
        _sender_report_ntp.erase(_sender_report_ntp.begin());
    }

    return RtcpHeader::toBuffer(std::move(rtcp));
}

toolkit::Buffer::Ptr RtcpContextForSend::createRtcpXRDLRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc) {
    auto rtcp = RtcpXRDLRR::create(1);
    rtcp->bt = 5;
    rtcp->reserved = 0;
    rtcp->block_length = htons(3);
    rtcp->ssrc = htonl(rtcp_ssrc);
    rtcp->items.ssrc = htonl(rtp_ssrc);

    if (_xr_xrrtr_recv_last_rr.find(rtp_ssrc) == _xr_xrrtr_recv_last_rr.end()) {
        rtcp->items.lrr = 0;
        WarnL;
    } else {
        rtcp->items.lrr = htonl(_xr_xrrtr_recv_last_rr[rtp_ssrc]);
    }

    if (_xr_rrtr_recv_sys_stamp.find(rtp_ssrc) == _xr_rrtr_recv_sys_stamp.end()) {
        rtcp->items.dlrr = 0;
        WarnL;
    } else {
        // now - Last SR time,单位毫秒
        auto delay = getCurrentMillisecond() - _xr_rrtr_recv_sys_stamp[rtp_ssrc];
        // in units of 1/65536 seconds
        auto dlsr = (uint32_t)(delay / 1000.0f * 65536);
        rtcp->items.dlrr = htonl(dlsr);
    }
    return RtcpHeader::toBuffer(std::move(rtcp));
}

////////////////////////////////////////////////////////////////////////////////////

void RtcpContextForRecv::onRtp(
    uint16_t seq, uint32_t stamp, uint64_t ntp_stamp_ms, uint32_t sample_rate, size_t bytes) {
    {
        // 接收者才做复杂的统计运算
        auto sys_stamp = getCurrentMillisecond();
        if (_last_rtp_sys_stamp) {
            // 计算时间戳抖动值
            double diff = double(
                (int64_t(sys_stamp) - int64_t(_last_rtp_sys_stamp)) * (sample_rate / double(1000.0))
                - (int64_t(stamp) - int64_t(_last_rtp_stamp)));
            if (diff < 0) {
                diff = -diff;
            }
            // 抖动单位为采样次数
            _jitter += (diff - _jitter) / 16.0;
        } else {
            _jitter = 0;
        }

        if (_last_rtp_seq > 0xFF00 && seq < 0xFF && (!_seq_cycles || _packets - _last_cycle_packets > 0x1FFF)) {
            // 上次seq大于0xFF00且本次seq小于0xFF，
            // 且未发生回环或者距离上次回环间隔超过0x1FFF个包，则认为回环
            ++_seq_cycles;
            _last_cycle_packets = _packets;
            _seq_max = seq;
        } else if (seq > _seq_max) {
            // 本次回环前最大seq
            _seq_max = seq;
        }

        if (!_seq_base) {
            // 记录第一个rtp的seq
            _seq_base = seq;
        } else if (!_seq_cycles && seq < _seq_base) {
            // 未发生回环，那么取最新的seq为基准seq
            _seq_base = seq;
        }

        _last_rtp_seq = seq;
        _last_rtp_sys_stamp = sys_stamp;
    }
    RtcpContext::onRtp(seq, stamp, ntp_stamp_ms, sample_rate, bytes);
}

void RtcpContextForRecv::onRtcp(RtcpHeader *rtcp) {
    switch ((RtcpType)rtcp->pt) {
    case RtcpType::RTCP_SR: {
        auto rtcp_sr = (RtcpSR *)rtcp;
        /**
         last SR timestamp (LSR): 32 bits
          The middle 32 bits out of 64 in the NTP timestamp (as explained in
          Section 4) received as part of the most recent RTCP sender report
          (SR) packet from source SSRC_n.  If no SR has been received yet,
          the field is set to zero.
         */
        _last_sr_lsr = ((rtcp_sr->ntpmsw & 0xFFFF) << 16) | ((rtcp_sr->ntplsw >> 16) & 0xFFFF);
        _last_sr_ntp_sys = getCurrentMillisecond();
        break;
    }
    default:
        break;
    }
}

size_t RtcpContextForRecv::getExpectedPackets() const {
    return (_seq_cycles << 16) + _seq_max - _seq_base + 1;
}

size_t RtcpContextForRecv::getExpectedPacketsInterval() {
    auto expected = getExpectedPackets();
    auto ret = expected - _last_expected;
    _last_expected = expected;
    return ret;
}

size_t RtcpContextForRecv::getLost() {
    return getExpectedPackets() - _packets;
}

size_t RtcpContextForRecv::geLostInterval() {
    auto lost = getLost();
    auto ret = lost - _last_lost;
    _last_lost = lost;
    return ret;
}

Buffer::Ptr RtcpContextForRecv::createRtcpRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc) {
    auto rtcp = RtcpRR::create(1);
    rtcp->ssrc = htonl(rtcp_ssrc);

    ReportItem *item = (ReportItem *)&rtcp->items;
    item->ssrc = htonl(rtp_ssrc);

    uint8_t fraction = 0;
    auto expected_interval = getExpectedPacketsInterval();
    if (expected_interval) {
        fraction = uint8_t(geLostInterval() << 8 / expected_interval);
    }

    item->fraction = fraction;
    item->cumulative = htonl(uint32_t(getLost())) >> 8;
    item->seq_cycles = htons(_seq_cycles);
    item->seq_max = htons(_seq_max);
    item->jitter = htonl(uint32_t(_jitter));
    item->last_sr_stamp = htonl(_last_sr_lsr);

    // now - Last SR time,单位毫秒
    auto delay = getCurrentMillisecond() - _last_sr_ntp_sys;
    // in units of 1/65536 seconds
    auto dlsr = (uint32_t)(delay / 1000.0f * 65536);
    item->delay_since_last_sr = htonl(_last_sr_lsr ? dlsr : 0);
    return RtcpHeader::toBuffer(rtcp);
}

} // namespace mediakit