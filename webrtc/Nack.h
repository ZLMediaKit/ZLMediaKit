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

// RTC配置项目
namespace Rtc {

//~ nack接收端(rtp发送端)
// Nack缓存包最早时间间隔
extern const std::string kMaxNackMS;
// Nack包检查间隔(包数量)
extern const std::string kRtpCacheCheckInterval;

//~ nack发送端(rtp接收端)
// 最大保留的rtp丢包状态个数
extern const std::string kNackMaxSize;
// rtp丢包状态最长保留时间
extern const std::string kNackMaxMS;
// nack最多请求重传次数
extern const std::string kNackMaxCount;
// nack重传频率，rtt的倍数
extern const std::string kNackIntervalRatio;
// nack包中rtp个数，减小此值可以让nack包响应更灵敏
extern const std::string kNackRtpSize;
} // namespace Rtc

class NackList {
public:
    void pushBack(RtpPacket::Ptr rtp);
    void forEach(const FCI_NACK &nack, const std::function<void(const RtpPacket::Ptr &rtp)> &cb);

private:
    void popFront();
    uint32_t getCacheMS();
    int64_t getRtpStamp(uint16_t seq);
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
    // 最新nack包中的rtp seq值
    uint16_t _nack_seq = 0;

    struct NackStatus {
        uint64_t first_stamp;
        uint64_t update_stamp;
        int nack_count = 0;
    };
    std::map<uint16_t /*seq*/, NackStatus> _nack_send_status;
};

} // namespace mediakit

#endif //ZLMEDIAKIT_NACK_H
