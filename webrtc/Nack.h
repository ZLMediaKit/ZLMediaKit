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
    using onNack = function<void(const FCI_NACK &nack)>;

    NackContext() = default;
    ~NackContext() = default;

    void received(uint16_t seq);
    void setOnNack(onNack cb);

private:
    void eraseFrontSeq();
    void doNack(const FCI_NACK &nack);

private:
    onNack _cb;
    set<uint16_t> _seq;
    uint16_t _last_max_seq = 0;
};

#endif //ZLMEDIAKIT_NACK_H
