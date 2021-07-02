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

static constexpr uint32_t kMaxNackMS = 10 * 1000;

void NackList::push_back(RtpPacket::Ptr rtp) {
    auto seq = rtp->getSeq();
    _nack_cache_seq.emplace_back(seq);
    _nack_cache_pkt.emplace(seq, std::move(rtp));
    while (get_cache_ms() > kMaxNackMS) {
        //需要清除部分nack缓存
        pop_front();
    }
}

void NackList::for_each_nack(const FCI_NACK &nack, const function<void(const RtpPacket::Ptr &rtp)> &func) {
    auto seq = nack.getPid();
    for (auto bit : nack.getBitArray()) {
        if (bit) {
            //丢包
            RtpPacket::Ptr *ptr = get_rtp(seq);
            if (ptr) {
                func(*ptr);
            }
        }
        ++seq;
    }
}

void NackList::pop_front() {
    if (_nack_cache_seq.empty()) {
        return;
    }
    _nack_cache_pkt.erase(_nack_cache_seq.front());
    _nack_cache_seq.pop_front();
}

RtpPacket::Ptr *NackList::get_rtp(uint16_t seq) {
    auto it = _nack_cache_pkt.find(seq);
    if (it == _nack_cache_pkt.end()) {
        return nullptr;
    }
    return &it->second;
}

uint32_t NackList::get_cache_ms() {
    if (_nack_cache_seq.size() < 2) {
        return 0;
    }
    uint32_t back = _nack_cache_pkt[_nack_cache_seq.back()]->getStampMS();
    uint32_t front = _nack_cache_pkt[_nack_cache_seq.front()]->getStampMS();
    if (back > front) {
        return back - front;
    }
    //很有可能回环了
    return back + (UINT32_MAX - front);
}

////////////////////////////////////////////////////////////////////////////////////////////////

void NackContext::received(uint16_t seq) {
    if (!_last_max_seq && _seq.empty()) {
        _last_max_seq = seq - 1;
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
        return;
    }

    if (_seq.size() == diff + 1 && _last_max_seq + 1 == min_seq) {
        //都是连续的seq，未丢包
        _seq.clear();
        _last_max_seq = max_seq;
    } else {
        //seq不连续，有丢包
        if (min_seq == _last_max_seq + 1) {
            //前面部分seq是连续的，未丢包，移除之
            eraseFrontSeq();
        }

        //有丢包，丢包从_last_max_seq开始
        if (max_seq - _last_max_seq > FCI_NACK::kBitSize) {
            vector<bool> vec;
            vec.resize(FCI_NACK::kBitSize);
            for (auto i = 0; i < FCI_NACK::kBitSize; ++i) {
                vec[i] = _seq.find(_last_max_seq + i + 2) == _seq.end();
            }
            doNack(FCI_NACK(_last_max_seq + 1, vec));
            _last_max_seq += FCI_NACK::kBitSize + 1;
            if (_last_max_seq >= max_seq) {
                _seq.clear();
            } else {
                auto it = _seq.emplace_hint(_seq.begin(), _last_max_seq);
                _seq.erase(_seq.begin(), it);
            }
        }
    }
}

void NackContext::setOnNack(onNack cb) {
    _cb = std::move(cb);
}

void NackContext::doNack(const FCI_NACK &nack) {
    if (_cb) {
        _cb(nack);
    }
}

void NackContext::eraseFrontSeq() {
    //前面部分seq是连续的，未丢包，移除之
    for (auto it = _seq.begin(); it != _seq.end();) {
        if (*it != _last_max_seq + 1) {
            //seq不连续，丢包了
            break;
        }
        _last_max_seq = *it;
        it = _seq.erase(it);
    }
}