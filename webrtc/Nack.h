/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_NACK_H
#define ZLMEDIAKIT_NACK_H

#include "Rtsp/Rtsp.h"
#include "Rtcp/RtcpFCI.h"

using namespace mediakit;

class NackList {
public:
    NackList() = default;
    ~NackList() = default;

    void push_back(RtpPacket::Ptr rtp);
    void for_each_nack(const FCI_NACK &nack, const function<void(const RtpPacket::Ptr &rtp)> &cb);

private:
    void pop_front();
    uint32_t get_cache_ms();
    RtpPacket::Ptr *get_rtp(uint16_t seq);

private:
    deque<uint16_t> _nack_cache_seq;
    unordered_map<uint16_t, RtpPacket::Ptr> _nack_cache_pkt;
};

class NackContext {
public:
    using Ptr = std::shared_ptr<NackContext>;
    using onNack = function<void(const FCI_NACK &nack)>;
    //最大保留的rtp丢包状态个数
    static constexpr auto kNackMaxSize = 1024;
    //rtp丢包状态最长保留时间
    static constexpr auto kNackMaxMS = 3 * 1000;
    //nack最多请求重传10次
    static constexpr auto kNackMaxCount = 10;
    //nack重传频率，rtt的倍数
    static constexpr auto kNackIntervalRatio = 2.0f;

    NackContext() = default;
    ~NackContext() = default;

    void received(uint16_t seq, bool is_rtx = false);
    void setOnNack(onNack cb);
    uint64_t reSendNack();

private:
    void eraseFrontSeq();
    void doNack(const FCI_NACK &nack, bool record_nack);
    void recordNack(const FCI_NACK &nack);
    void onRtx(uint16_t seq);

private:
    int _rtt = 50;
    onNack _cb;
    set<uint16_t> _seq;
    uint16_t _last_max_seq = 0;

    struct NackStatus{
        uint64_t first_stamp;
        uint64_t update_stamp;
        int nack_count = 0;
    };
    map<uint16_t/*seq*/, NackStatus > _nack_send_status;
};

#endif //ZLMEDIAKIT_NACK_H
