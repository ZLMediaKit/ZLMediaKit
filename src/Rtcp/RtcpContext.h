/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTCPCONTEXT_H
#define ZLMEDIAKIT_RTCPCONTEXT_H

#include "Rtcp.h"
#include <stddef.h>
#include <stdint.h>

namespace mediakit {

class RtcpContext {
public:
    using Ptr = std::shared_ptr<RtcpContext>;
    virtual ~RtcpContext() = default;

    /**
     * 输出或输入rtp时调用
     * @param seq rtp的seq
     * @param stamp rtp的时间戳，单位采样数(非毫秒)
     * @param ntp_stamp_ms ntp时间戳
     * @param rtp rtp时间戳采样率，视频一般为90000，音频一般为采样率
     * @param bytes rtp数据长度
     */
    virtual void onRtp(uint16_t seq, uint32_t stamp, uint64_t ntp_stamp_ms, uint32_t sample_rate, size_t bytes);

    /**
     * 输入sr rtcp包
     * @param rtcp 输入一个rtcp
     */
    virtual void onRtcp(RtcpHeader *rtcp) = 0;

    /**
     * 计算总丢包数
     */
    virtual size_t getLost();

    /**
     * 返回理应收到的rtp数
     */
    virtual size_t getExpectedPackets() const;

    /**
     * 创建SR rtcp包
     * @param rtcp_ssrc rtcp的ssrc
     * @return rtcp包
     */
    virtual toolkit::Buffer::Ptr createRtcpSR(uint32_t rtcp_ssrc);

    /**
     * @brief 创建xr的dlrr包，用于接收者估算rtt
     *
     * @return toolkit::Buffer::Ptr
     */
    virtual toolkit::Buffer::Ptr createRtcpXRDLRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc);

    /**
     * 创建RR rtcp包
     * @param rtcp_ssrc rtcp的ssrc
     * @param rtp_ssrc rtp的ssrc
     * @return rtcp包
     */
    virtual toolkit::Buffer::Ptr createRtcpRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc);

    /**
     * 上次结果与本次结果间应收包数
     */
    virtual size_t getExpectedPacketsInterval();

    /**
     * 上次结果与本次结果间丢包个数
     */
    virtual size_t geLostInterval();

protected:
    // 收到或发送的rtp的字节数
    size_t _bytes = 0;
    // 收到或发送的rtp的个数
    size_t _packets = 0;
    // 上次的rtp时间戳,毫秒
    uint32_t _last_rtp_stamp = 0;
    uint64_t _last_ntp_stamp_ms = 0;
};

class RtcpContextForSend : public RtcpContext {
public:
    toolkit::Buffer::Ptr createRtcpSR(uint32_t rtcp_ssrc) override;

    void onRtcp(RtcpHeader *rtcp) override;

    toolkit::Buffer::Ptr createRtcpXRDLRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc) override;

    /**
     * 获取rtt
     * @param ssrc rtp ssrc
     * @return rtt,单位毫秒
     */
    uint32_t getRtt(uint32_t ssrc) const;

private:
    std::map<uint32_t /*ssrc*/, uint32_t /*rtt*/> _rtt;
    std::map<uint32_t /*last_sr_lsr*/, uint64_t /*ntp stamp*/> _sender_report_ntp;

    std::map<uint32_t /*ssrc*/, uint64_t /*xr rrtr sys stamp*/> _xr_rrtr_recv_sys_stamp;
    std::map<uint32_t /*ssrc*/, uint32_t /*last rr */> _xr_xrrtr_recv_last_rr;
};

class RtcpContextForRecv : public RtcpContext {
public:
    void onRtp(uint16_t seq, uint32_t stamp, uint64_t ntp_stamp_ms, uint32_t sample_rate, size_t bytes) override;
    toolkit::Buffer::Ptr createRtcpRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc) override;
    size_t getExpectedPackets() const override;
    size_t getExpectedPacketsInterval() override;
    size_t getLost() override;
    size_t geLostInterval() override;
    void onRtcp(RtcpHeader *rtcp) override;

private:
    // 时间戳抖动值
    double _jitter = 0;
    // 第一个seq的值
    uint16_t _seq_base = 0;
    // rtp最大seq
    uint16_t _seq_max = 0;
    // rtp回环次数
    uint16_t _seq_cycles = 0;
    // 上次回环发生时，记录的rtp包数
    size_t _last_cycle_packets = 0;
    // 上次的seq
    uint16_t _last_rtp_seq = 0;
    // 上次的rtp的系统时间戳(毫秒)用于统计抖动
    uint64_t _last_rtp_sys_stamp = 0;
    // 上次统计的丢包总数
    size_t _last_lost = 0;
    // 上次统计应收rtp包总数
    size_t _last_expected = 0;
    // 上次收到sr包时计算出的Last SR timestamp
    uint32_t _last_sr_lsr = 0;
    // 上次收到sr时的系统时间戳,单位毫秒
    uint64_t _last_sr_ntp_sys = 0;
};

} // namespace mediakit
#endif // ZLMEDIAKIT_RTCPCONTEXT_H
