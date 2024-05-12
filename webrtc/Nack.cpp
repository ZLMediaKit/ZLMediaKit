/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Nack.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

// RTC配置项目
namespace Rtc {
#define RTC_FIELD "rtc."
//~ nack接收端
// Nack缓存包最早时间间隔
const string kMaxNackMS = RTC_FIELD "maxNackMS";
// Nack包检查间隔(包数量)
const string kRtpCacheCheckInterval = RTC_FIELD "rtpCacheCheckInterval";
//~ nack发送端
//最大保留的rtp丢包状态个数
const string kNackMaxSize = RTC_FIELD "nackMaxSize";
// rtp丢包状态最长保留时间
const string kNackMaxMS = RTC_FIELD "nackMaxMS";
// nack最多请求重传次数
const string kNackMaxCount = RTC_FIELD "nackMaxCount";
// nack重传频率，rtt的倍数
const string kNackIntervalRatio = RTC_FIELD "nackIntervalRatio";
// nack包中rtp个数，减小此值可以让nack包响应更灵敏
const string kNackRtpSize = RTC_FIELD "nackRtpSize";

static onceToken token([]() {
    mINI::Instance()[kMaxNackMS] = 5 * 1000;
    mINI::Instance()[kRtpCacheCheckInterval] = 100;
    mINI::Instance()[kNackMaxSize] = 2048;
    mINI::Instance()[kNackMaxMS] = 3 * 1000;
    mINI::Instance()[kNackMaxCount] = 15;
    mINI::Instance()[kNackIntervalRatio] = 1.0f;
    mINI::Instance()[kNackRtpSize] = 8;
});

} // namespace Rtc

void NackList::pushBack(RtpPacket::Ptr rtp) {
    auto seq = rtp->getSeq();
    _nack_cache_seq.emplace_back(seq);
    _nack_cache_pkt.emplace(seq, std::move(rtp));
    GET_CONFIG(uint32_t, rtpcache_checkinterval, Rtc::kRtpCacheCheckInterval);
    if (++_cache_ms_check < rtpcache_checkinterval) {
        return;
    }
    _cache_ms_check = 0;
    GET_CONFIG(uint32_t, maxnackms, Rtc::kMaxNackMS);
    while (getCacheMS() >= maxnackms) {
        // 需要清除部分nack缓存
        popFront();
    }
}

void NackList::forEach(const FCI_NACK &nack, const function<void(const RtpPacket::Ptr &rtp)> &func) {
    auto seq = nack.getPid();
    for (auto bit : nack.getBitArray()) {
        if (bit) {
            // 丢包
            RtpPacket::Ptr *ptr = getRtp(seq);
            if (ptr) {
                func(*ptr);
            }
        }
        ++seq;
    }
}

void NackList::popFront() {
    if (_nack_cache_seq.empty()) {
        return;
    }
    _nack_cache_pkt.erase(_nack_cache_seq.front());
    _nack_cache_seq.pop_front();
}

RtpPacket::Ptr *NackList::getRtp(uint16_t seq) {
    auto it = _nack_cache_pkt.find(seq);
    if (it == _nack_cache_pkt.end()) {
        return nullptr;
    }
    return &it->second;
}

uint32_t NackList::getCacheMS() {
    while (_nack_cache_seq.size() > 2) {
        auto back_stamp = getRtpStamp(_nack_cache_seq.back());
        if (back_stamp == -1) {
            _nack_cache_seq.pop_back();
            continue;
        }

        auto front_stamp = getRtpStamp(_nack_cache_seq.front());
        if (front_stamp == -1) {
            _nack_cache_seq.pop_front();
            continue;
        }

        if (back_stamp >= front_stamp) {
            return back_stamp - front_stamp;
        }
        // 很有可能回环了
        return back_stamp + (UINT32_MAX - front_stamp);
    }
    return 0;
}

int64_t NackList::getRtpStamp(uint16_t seq) {
    auto it = _nack_cache_pkt.find(seq);
    if (it == _nack_cache_pkt.end()) {
        return -1;
    }
    return it->second->getStampMS(false);
}

////////////////////////////////////////////////////////////////////////////////////////////////

NackContext::NackContext() {
    setOnNack(nullptr);
}

void NackContext::received(uint16_t seq, bool is_rtx) {
    if (!_started) {
        // 记录第一个seq
        _started = true;
        _nack_seq = seq - 1;
    }

    if (seq < _nack_seq && _nack_seq != UINT16_MAX && seq < 1024 && _nack_seq > UINT16_MAX - 1024) {
        // seq回环,清空回环前状态
        makeNack(UINT16_MAX, true);
        _seq.emplace(seq);
        return;
    }

    if (is_rtx || (seq < _nack_seq && _nack_seq != UINT16_MAX)) {
        // seq非回环回退包，猜测其为重传包，清空其nack状态
        clearNackStatus(seq);
        return;
    }

    auto pr = _seq.emplace(seq);
    if (!pr.second) {
        // seq重复, 忽略
        return;
    }

    auto max_seq = *_seq.rbegin();
    auto min_seq = *_seq.begin();
    auto diff = max_seq - min_seq;
    if (diff > (UINT16_MAX >> 1)) {
        // 回环后，收到回环前的大值seq, 忽略掉
        _seq.erase(max_seq);
        return;
    }
    if (min_seq == (uint16_t)(_nack_seq + 1) && _seq.size() == (size_t)diff + 1) {
        // 都是连续的seq，未丢包
        _seq.clear();
        _nack_seq = max_seq;
    } else {
        // seq不连续，有丢包
        makeNack(max_seq, false);
    }
}

void NackContext::makeNack(uint16_t max_seq, bool flush) {
    // 尝试移除前面部分连续的seq
    eraseFrontSeq();
    // 最多生成5个nack包，防止seq大幅跳跃导致一直循环
    auto max_nack = 5u;
    GET_CONFIG(uint32_t, nack_rtpsize, Rtc::kNackRtpSize);
    // kNackRtpSize must between 0 and 16
    nack_rtpsize = std::min<uint32_t>(nack_rtpsize, FCI_NACK::kBitSize);
    while (_nack_seq != max_seq && max_nack--) {
        // 一次不能发送超过16+1个rtp的状态
        uint16_t nack_rtp_count = std::min<uint16_t>(FCI_NACK::kBitSize, max_seq - (uint16_t)(_nack_seq + 1));
        if (!flush && nack_rtp_count < nack_rtpsize) {
            // 非flush状态下，seq个数不足以发送一次nack
            break;
        }
        vector<bool> vec;
        vec.resize(nack_rtp_count, false);
        for (size_t i = 0; i < nack_rtp_count; ++i) {
            vec[i] = _seq.find((uint16_t)(_nack_seq + i + 2)) == _seq.end();
        }
        doNack(FCI_NACK(_nack_seq + 1, vec), true);
        _nack_seq += nack_rtp_count + 1;
        // 返回第一个比_last_max_seq大的元素
        auto it = _seq.upper_bound(_nack_seq);
        // 移除 <=_last_max_seq 的seq
        _seq.erase(_seq.begin(), it);
    }
}

void NackContext::setOnNack(onNack cb) {
    if (cb) {
        _cb = std::move(cb);
    } else {
        _cb = [](const FCI_NACK &nack) {};
    }
}

void NackContext::doNack(const FCI_NACK &nack, bool record_nack) {
    if (record_nack) {
        recordNack(nack);
    }
    _cb(nack);
}

void NackContext::eraseFrontSeq() {
    // 前面部分seq是连续的，未丢包，移除之
    for (auto it = _seq.begin(); it != _seq.end();) {
        if (*it != (uint16_t)(_nack_seq + 1)) {
            // seq不连续，丢包了
            break;
        }
        _nack_seq = *it;
        it = _seq.erase(it);
    }
}

void NackContext::clearNackStatus(uint16_t seq) {
    auto it = _nack_send_status.find(seq);
    if (it == _nack_send_status.end()) {
        return;
    }
    //收到重传包与第一个nack包间的时间约等于rtt时间
    auto rtt = getCurrentMillisecond() - it->second.first_stamp;
    _nack_send_status.erase(it);

    // 限定rtt在合理有效范围内
    GET_CONFIG(uint32_t, nack_maxms, Rtc::kNackMaxMS);
    GET_CONFIG(uint32_t, nack_maxcount, Rtc::kNackMaxCount);
    _rtt = max<int>(10, min<int>(rtt, nack_maxms / nack_maxcount));
}

void NackContext::recordNack(const FCI_NACK &nack) {
    auto now = getCurrentMillisecond();
    auto i = nack.getPid();
    for (auto flag : nack.getBitArray()) {
        if (flag) {
            auto &ref = _nack_send_status[i];
            ref.first_stamp = now;
            ref.update_stamp = now;
            ref.nack_count = 1;
        }
        ++i;
    }
    // 记录太多了，移除一部分早期的记录
    GET_CONFIG(uint32_t, nack_maxsize, Rtc::kNackMaxSize);
    while (_nack_send_status.size() > nack_maxsize) {
        _nack_send_status.erase(_nack_send_status.begin());
    }
}

uint64_t NackContext::reSendNack() {
    set<uint16_t> nack_rtp;
    auto now = getCurrentMillisecond();
    GET_CONFIG(uint32_t, nack_maxms, Rtc::kNackMaxMS);
    GET_CONFIG(uint32_t, nack_maxcount, Rtc::kNackMaxCount);
    GET_CONFIG(float, nack_intervalratio, Rtc::kNackIntervalRatio);
    for (auto it = _nack_send_status.begin(); it != _nack_send_status.end();) {
        if (now - it->second.first_stamp > nack_maxms) {
            // 该rtp丢失太久了，不再要求重传
            it = _nack_send_status.erase(it);
            continue;
        }
        if (now - it->second.update_stamp < nack_intervalratio * _rtt) {
            // 距离上次nack不足2倍的rtt，不用再发送nack
            ++it;
            continue;
        }
        // 此rtp需要请求重传
        nack_rtp.emplace(it->first);
        // 更新nack发送时间戳
        it->second.update_stamp = now;
        if (++(it->second.nack_count) == nack_maxcount) {
            // nack次数太多，移除之
            it = _nack_send_status.erase(it);
            continue;
        }
        ++it;
    }

    int pid = -1;
    vector<bool> vec;
    for (auto it = nack_rtp.begin(); it != nack_rtp.end();) {
        if (pid == -1) {
            pid = *it;
            vec.assign(FCI_NACK::kBitSize, false);
            ++it;
            continue;
        }
        auto inc = *it - pid;
        if (inc > (ssize_t)FCI_NACK::kBitSize) {
            // 新的nack包
            doNack(FCI_NACK(pid, vec), false);
            pid = -1;
            continue;
        }
        // 这个包丢了
        vec[inc - 1] = true;
        ++it;
    }
    if (pid != -1) {
        doNack(FCI_NACK(pid, vec), false);
    }

    // 没有任何包需要重传时返回0，否则返回下次重传间隔(不得低于5ms)
    return _nack_send_status.empty() ? 0 : _rtt;
}

} // namespace mediakit
