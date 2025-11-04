/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
     * Called when outputting or inputting rtp
     * @param seq rtp's seq
     * @param stamp rtp's timestamp, unit is sample number (not millisecond)
     * @param ntp_stamp_ms ntp timestamp
     * @param rtp rtp timestamp sampling rate, video is generally 90000, audio is generally sampling rate
     * @param bytes rtp data length
     
     * [AUTO-TRANSLATED:745772b5]
     */
    virtual void onRtp(uint16_t seq, uint32_t stamp, uint64_t ntp_stamp_ms, uint32_t sample_rate, size_t bytes);

    /**
     * 输入sr rtcp包
     * @param rtcp 输入一个rtcp
     * Input sr rtcp packet
     * @param rtcp input an rtcp
     
     * [AUTO-TRANSLATED:46f309ec]
     */
    virtual void onRtcp(RtcpHeader *rtcp) = 0;

    /**
     * 计算总丢包数
     * Calculate the total number of lost packets
     
     * [AUTO-TRANSLATED:084f3832]
     */
    virtual size_t getLost();

    /**
     * 返回理应收到的rtp数
     * Return the number of rtp that should be received
     
     * [AUTO-TRANSLATED:ede367a0]
     */
    virtual size_t getExpectedPackets() const;

    /**
     * 创建SR rtcp包
     * @param rtcp_ssrc rtcp的ssrc
     * @return rtcp包
     * Create SR rtcp packet
     * @param rtcp_ssrc rtcp's ssrc
     * @return rtcp packet
     
     * [AUTO-TRANSLATED:a9ec36d0]
     */
    virtual toolkit::Buffer::Ptr createRtcpSR(uint32_t rtcp_ssrc);

    /**
     * @brief 创建xr的dlrr包，用于接收者估算rtt
     *
     * @return toolkit::Buffer::Ptr
     * @brief Create xr's dlrr packet, used by receiver to estimate rtt
     *
     * @return toolkit::Buffer::Ptr
     
     * [AUTO-TRANSLATED:a5094e1d]
     */
    virtual toolkit::Buffer::Ptr createRtcpXRDLRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc);

    /**
     * 创建RR rtcp包
     * @param rtcp_ssrc rtcp的ssrc
     * @param rtp_ssrc rtp的ssrc
     * @return rtcp包
     * Create RR rtcp packet
     * @param rtcp_ssrc rtcp's ssrc
     * @param rtp_ssrc rtp's ssrc
     * @return rtcp packet
     
     * [AUTO-TRANSLATED:81ebbf81]
     */
    virtual toolkit::Buffer::Ptr createRtcpRR(uint32_t rtcp_ssrc, uint32_t rtp_ssrc);

    /**
     * 上次结果与本次结果间应收包数
     * Number of packets that should be received between the last result and the current result
     
     * [AUTO-TRANSLATED:3b2846ab]
     */
    virtual size_t getExpectedPacketsInterval();

    /**
     * 上次结果与本次结果间丢包个数
     * Number of lost packets between the last result and the current result
     
     * [AUTO-TRANSLATED:fe5ac890]
     */
    virtual size_t getLostInterval();

protected:
    // 收到或发送的rtp的字节数  [AUTO-TRANSLATED:a38d88a9]
    // Number of bytes of rtp received or sent
    size_t _bytes = 0;
    // 收到或发送的rtp的个数  [AUTO-TRANSLATED:b28c3c90]
    // Number of rtp received or sent
    size_t _packets = 0;
    // 上次的rtp时间戳,毫秒  [AUTO-TRANSLATED:99eecec6]
    // Last rtp timestamp, milliseconds
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
     * Get rtt
     * @param ssrc rtp ssrc
     * @return rtt, unit is millisecond
     
     * [AUTO-TRANSLATED:f0885551]
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
    size_t getLostInterval() override;
    void onRtcp(RtcpHeader *rtcp) override;

private:
    // 时间戳抖动值  [AUTO-TRANSLATED:8100680c]
    // Timestamp jitter value
    double _jitter = 0;
    // 第一个seq的值  [AUTO-TRANSLATED:d893719d]
    // The value of the first seq
    uint16_t _seq_base = 0;
    // rtp最大seq  [AUTO-TRANSLATED:5cc9f775]
    // Maximum rtp seq
    uint16_t _seq_max = 0;
    // rtp回环次数  [AUTO-TRANSLATED:9fe9c340]
    // Rtp loopback times
    uint16_t _seq_cycles = 0;
    // 上次回环发生时，记录的rtp包数  [AUTO-TRANSLATED:c32cb555]
    // Number of rtp packets recorded when the last loopback occurred
    size_t _last_cycle_packets = 0;
    // 上次的seq  [AUTO-TRANSLATED:07364b7d]
    // Last seq
    uint16_t _last_rtp_seq = 0;
    // 上次的rtp的系统时间戳(毫秒)用于统计抖动  [AUTO-TRANSLATED:b1e8c89b]
    // Last rtp system timestamp (milliseconds) used for jitter statistics
    uint64_t _last_rtp_sys_stamp = 0;
    // 上次统计的丢包总数  [AUTO-TRANSLATED:242e75ed]
    // Last total number of lost packets counted
    size_t _last_lost = 0;
    // 上次统计应收rtp包总数  [AUTO-TRANSLATED:eb2d5f4d]
    // Last total number of rtp packets that should be received counted
    size_t _last_expected = 0;
    // 上次收到sr包时计算出的Last SR timestamp  [AUTO-TRANSLATED:fdec069e]
    // Last SR timestamp calculated when the last SR packet was received
    uint32_t _last_sr_lsr = 0;
    // 上次收到sr时的系统时间戳,单位毫秒  [AUTO-TRANSLATED:044fa0d5]
    // System timestamp when the last SR was received, unit is millisecond
    uint64_t _last_sr_ntp_sys = 0;
};

} // namespace mediakit
#endif // ZLMEDIAKIT_RTCPCONTEXT_H
