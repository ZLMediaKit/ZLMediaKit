/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_NACK_H
#define ZLMEDIAKIT_NACK_H

#include <set>
#include <map>
#include <deque>
#include <unordered_map>
#include "Rtsp/Rtsp.h"
#include "Rtcp/RtcpFCI.h"

namespace mediakit {

// RTC配置项目  [AUTO-TRANSLATED:19940011]
// RTC configuration project
namespace Rtc {
// ~ nack发送端，rtp接收端  [AUTO-TRANSLATED:bb169205]
// ~ nack sender, rtp receiver
// 最大保留的rtp丢包状态个数  [AUTO-TRANSLATED:70eee442]
// Maximum number of retained rtp packet loss states
extern const std::string kNackMaxSize;
// rtp丢包状态最长保留时间  [AUTO-TRANSLATED:f9306375]
// Maximum retention time for rtp packet loss states
extern const std::string kNackMaxMS;
} // namespace Rtc

class NackList {
public:
    void pushBack(RtpPacket::Ptr rtp);
    void forEach(const FCI_NACK &nack, const std::function<void(const RtpPacket::Ptr &rtp)> &cb);

private:
    void popFront();
    uint32_t getCacheMS();
    int64_t getNtpStamp(uint16_t seq);
    RtpPacket::Ptr *getRtp(uint16_t seq);

private:
    uint32_t _cache_ms_check = 0;
    std::deque<uint16_t> _nack_cache_seq;
    std::unordered_map<uint16_t, RtpPacket::Ptr> _nack_cache_pkt;
};

class NackContext {
public:
    using Ptr = std::shared_ptr<NackContext>;
    using onNack = std::function<void(const FCI_NACK &nack)>;

    NackContext();

    void received(uint16_t seq, bool is_rtx = false);
    void setOnNack(onNack cb);
    uint64_t reSendNack();

private:
    void eraseFrontSeq();
    void doNack(const FCI_NACK &nack, bool record_nack);
    void recordNack(const FCI_NACK &nack);
    void clearNackStatus(uint16_t seq);
    void makeNack(uint16_t max, bool flush = false);

private:
    bool _started = false;
    int _rtt = 50;
    onNack _cb;
    std::set<uint16_t> _seq;
    // 最新nack包中的rtp seq值  [AUTO-TRANSLATED:6984d95a]
    // RTP seq value in the latest nack packet
    uint16_t _nack_seq = 0;

    struct NackStatus {
        uint64_t first_stamp;
        uint64_t update_stamp;
        uint32_t nack_count = 0;
    };
    std::map<uint16_t /*seq*/, NackStatus> _nack_send_status;
};

} // namespace mediakit

#endif //ZLMEDIAKIT_NACK_H
