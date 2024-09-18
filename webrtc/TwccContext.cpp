/*
 * Copyright (c) 2021 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TwccContext.h"
#include "Rtcp/RtcpFCI.h"

namespace mediakit {

enum class ExtSeqStatus : int {
    normal = 0,
    looped,
    jumped,
};

void TwccContext::onRtp(uint32_t ssrc, uint16_t twcc_ext_seq, uint64_t stamp_ms) {
    switch ((ExtSeqStatus) checkSeqStatus(twcc_ext_seq)) {
        case ExtSeqStatus::jumped: /*seq异常,过滤掉*/ return;
        case ExtSeqStatus::looped: /*回环，触发发送twcc rtcp*/ onSendTwcc(ssrc); break;
        case ExtSeqStatus::normal: break;
        default: /*不可达*/assert(0); break;
    }

    auto result = _rtp_recv_status.emplace(twcc_ext_seq, stamp_ms);
    if (!result.second) {
        WarnL << "recv same twcc ext seq:" << twcc_ext_seq;
        return;
    }

    _max_stamp = result.first->second;
    if (!_min_stamp) {
        _min_stamp = _max_stamp;
    }

    if (needSendTwcc()) {
        // 其他匹配条件立即发送twcc  [AUTO-TRANSLATED:959d22b6]
        // Send twcc immediately if other matching conditions are met
        onSendTwcc(ssrc);
    }
}

bool TwccContext::needSendTwcc() const {
    if (_rtp_recv_status.empty()) {
        return false;
    }
    return (_rtp_recv_status.size() >= kMaxSeqSize) || (_max_stamp - _min_stamp >= kMaxTimeDelta);
}

int TwccContext::checkSeqStatus(uint16_t twcc_ext_seq) const {
    if (_rtp_recv_status.empty()) {
        return (int) ExtSeqStatus::normal;
    }
    auto max = _rtp_recv_status.rbegin()->first;
    auto delta = (int32_t) twcc_ext_seq - (int32_t) max;
    if (delta > 0 && delta < 0xFFFF / 2) {
        // 正常增长  [AUTO-TRANSLATED:7699c37d]
        // Normal growth
        return (int) ExtSeqStatus::normal;
    }
    if (delta < -0xFF00) {
        // 回环  [AUTO-TRANSLATED:82956e03]
        // Loop
        TraceL << "rtp twcc ext seq looped:" << max << " -> " << twcc_ext_seq;
        return (int) ExtSeqStatus::looped;
    }
    if (delta > 0xFF00) {
        // 回环后收到前面大的乱序的包，无法处理，丢弃  [AUTO-TRANSLATED:512cc269]
        // Discard packets that are out of order and large after looping back, as they cannot be processed
        TraceL << "rtp twcc ext seq jumped after looped:" << max << " -> " << twcc_ext_seq;
        return (int) ExtSeqStatus::jumped;
    }
    auto min = _rtp_recv_status.begin()->first;
    if (min <= twcc_ext_seq || twcc_ext_seq <= max) {
        // 正常回退  [AUTO-TRANSLATED:c8c6803f]
        // Normal rollback
        return (int) ExtSeqStatus::normal;
    }
    // seq莫名的大幅增加或减少，无法处理，丢弃  [AUTO-TRANSLATED:44dbbd28]
    // Discard packets with a large increase or decrease in seq, as they cannot be processed
    TraceL << "rtp twcc ext seq jumped:" << max << " -> " << twcc_ext_seq;
    return (int) ExtSeqStatus::jumped;
}

void TwccContext::onSendTwcc(uint32_t ssrc) {
    auto max = _rtp_recv_status.rbegin()->first;
    auto begin = _rtp_recv_status.begin();
    auto min = begin->first;
    // 参考时间戳的最小单位是64ms  [AUTO-TRANSLATED:2e701a8c]
    // The minimum unit of the reference timestamp is 64ms
    auto ref_time = begin->second >> 6;
    // 还原基准时间戳  [AUTO-TRANSLATED:bab53195]
    // Restore the baseline timestamp
    auto last_time = ref_time << 6;
    FCI_TWCC::TwccPacketStatus status;
    for (auto seq = min; seq <= max; ++seq) {
        int16_t delta = 0;
        SymbolStatus symbol = SymbolStatus::not_received;
        auto it = _rtp_recv_status.find(seq);
        if (it != _rtp_recv_status.end()) {
            // recv delta,单位为250us,1ms等于4x250us  [AUTO-TRANSLATED:46a0e186]
            // recv delta, unit is 250us, 1ms equals 4x250us
            delta = (int16_t) (4 * ((int64_t) it->second - (int64_t) last_time));
            if (delta < 0 || delta > 0xFF) {
                symbol = SymbolStatus::large_delta;
            } else {
                symbol = SymbolStatus::small_delta;
            }
            last_time = it->second;
        }
        status.emplace(seq, std::make_pair(symbol, delta));
    }
    auto fci = FCI_TWCC::create(ref_time, _twcc_pkt_count++, status);
    if (_cb) {
        _cb(ssrc, std::move(fci));
    }
    clearStatus();
}

void TwccContext::clearStatus() {
    _rtp_recv_status.clear();
    _min_stamp = 0;
}

void TwccContext::setOnSendTwccCB(TwccContext::onSendTwccCB cb) {
    _cb = std::move(cb);
}

}// namespace mediakit