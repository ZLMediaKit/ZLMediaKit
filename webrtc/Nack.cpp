/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Nack.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

static constexpr uint32_t kMaxNackMS = 5 * 1000;
static constexpr uint32_t kRtpCacheCheckInterval = 100;

void NackList::pushBack(RtpPacket::Ptr rtp) {
    auto seq = rtp->getSeq();
    _nack_cache_seq.emplace_back(seq);
    _nack_cache_pkt.emplace(seq, std::move(rtp));
    if (++_cache_ms_check < kRtpCacheCheckInterval) {
        return;
    }
    _cache_ms_check = 0;
    while (getCacheMS() >= kMaxNackMS) {
        //需要清除部分nack缓存
        popFront();
    }
}

void NackList::forEach(const FCI_NACK &nack, const function<void(const RtpPacket::Ptr &rtp)> &func) {
    auto seq = nack.getPid();
    for (auto bit : nack.getBitArray()) {
        if (bit) {
            //丢包
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
        //很有可能回环了
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

void NackContext::received(uint16_t seq, bool is_rtx) {
    if (!_last_max_seq && _seq.empty()) {
        _last_max_seq = seq - 1;
    }
    if (is_rtx || (seq < _last_max_seq && !(seq < 1024 && _last_max_seq > UINT16_MAX - 1024))) {
        //重传包或
        // seq回退，且非回环，那么这个应该是重传包
        onRtx(seq);
        return;
    }

    _seq.emplace(seq);
    auto max_seq = *_seq.rbegin();
    auto min_seq = *_seq.begin();
    auto diff = max_seq - min_seq;
    if (!diff) {
        return;
    }

    if (diff > UINT16_MAX / 2) {
        //回环
        _seq.clear();
        _last_max_seq = min_seq;
        _nack_send_status.clear();
        return;
    }

    if (_seq.size() == (size_t)diff + 1 && _last_max_seq + 1 == min_seq) {
        //都是连续的seq，未丢包
        _seq.clear();
        _last_max_seq = max_seq;
    } else {
        // seq不连续，有丢包
        if (min_seq == _last_max_seq + 1) {
            //前面部分seq是连续的，未丢包，移除之
            eraseFrontSeq();
        }

        //有丢包，丢包从_last_max_seq开始
        auto nack_rtp_count = FCI_NACK::kBitSize;
        if (max_seq > nack_rtp_count + _last_max_seq) {
            vector<bool> vec;
            vec.resize(FCI_NACK::kBitSize, false);
            for (size_t i = 0; i < nack_rtp_count; ++i) {
                vec[i] = _seq.find(_last_max_seq + i + 2) == _seq.end();
            }
            doNack(FCI_NACK(_last_max_seq + 1, vec), true);
            _last_max_seq += nack_rtp_count + 1;
            if (_last_max_seq >= max_seq) {
                _seq.clear();
            } else {
                auto it = _seq.emplace_hint(_seq.begin(), _last_max_seq + 1);
                _seq.erase(_seq.begin(), it);
            }
        }
    }
}

void NackContext::setOnNack(onNack cb) {
    _cb = std::move(cb);
}

void NackContext::doNack(const FCI_NACK &nack, bool record_nack) {
    if (record_nack) {
        recordNack(nack);
    }
    if (_cb) {
        _cb(nack);
    }
}

void NackContext::eraseFrontSeq() {
    //前面部分seq是连续的，未丢包，移除之
    for (auto it = _seq.begin(); it != _seq.end();) {
        if (*it != _last_max_seq + 1) {
            // seq不连续，丢包了
            break;
        }
        _last_max_seq = *it;
        it = _seq.erase(it);
    }
}

void NackContext::onRtx(uint16_t seq) {
    auto it = _nack_send_status.find(seq);
    if (it == _nack_send_status.end()) {
        return;
    }
    auto rtt = getCurrentMillisecond() - it->second.update_stamp;
    _nack_send_status.erase(it);

    if (rtt >= 0) {
        // rtt不肯小于0
        _rtt = rtt;
        // InfoL << "rtt:" << rtt;
    }
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
    //记录太多了，移除一部分早期的记录
    while (_nack_send_status.size() > kNackMaxSize) {
        _nack_send_status.erase(_nack_send_status.begin());
    }
}

uint64_t NackContext::reSendNack() {
    set<uint16_t> nack_rtp;
    auto now = getCurrentMillisecond();
    for (auto it = _nack_send_status.begin(); it != _nack_send_status.end();) {
        if (now - it->second.first_stamp > kNackMaxMS) {
            //该rtp丢失太久了，不再要求重传
            it = _nack_send_status.erase(it);
            continue;
        }
        if (now - it->second.update_stamp < kNackIntervalRatio * _rtt) {
            //距离上次nack不足2倍的rtt，不用再发送nack
            ++it;
            continue;
        }
        //此rtp需要请求重传
        nack_rtp.emplace(it->first);
        //更新nack发送时间戳
        it->second.update_stamp = now;
        if (++(it->second.nack_count) == kNackMaxCount) {
            // nack次数太多，移除之
            it = _nack_send_status.erase(it);
            continue;
        }
        ++it;
    }

    if (_nack_send_status.empty()) {
        //不需要再发送nack
        return 0;
    }

    int pid = -1;
    vector<bool> vec;
    for (auto it = nack_rtp.begin(); it != nack_rtp.end();) {
        if (pid == -1) {
            pid = *it;
            vec.resize(FCI_NACK::kBitSize, false);
            ++it;
            continue;
        }
        auto inc = *it - pid;
        if (inc > (ssize_t)FCI_NACK::kBitSize) {
            //新的nack包
            doNack(FCI_NACK(pid, vec), false);
            pid = -1;
            continue;
        }
        //这个包丢了
        vec[inc - 1] = true;
        ++it;
    }
    if (pid != -1) {
        doNack(FCI_NACK(pid, vec), false);
    }

    //重传间隔不得低于5ms
    return max(_rtt, 5);
}

} // namespace mediakit
