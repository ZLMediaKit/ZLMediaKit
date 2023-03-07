/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include <stddef.h>
#include "RtpSelector.h"
#include "RtpSplitter.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

INSTANCE_IMP(RtpSelector);

void RtpSelector::clear(){
    lock_guard<decltype(_mtx_map)> lck(_mtx_map);
    _map_rtp_process.clear();
}

bool RtpSelector::getSSRC(const char *data, size_t data_len, uint32_t &ssrc){
    if (data_len < 12) {
        return false;
    }
    uint32_t *ssrc_ptr = (uint32_t *) (data + 8);
    ssrc = ntohl(*ssrc_ptr);
    return true;
}

RtpProcess::Ptr RtpSelector::getProcess(const string &stream_id,bool makeNew) {
    lock_guard<decltype(_mtx_map)> lck(_mtx_map);
    auto it = _map_rtp_process.find(stream_id);
    if (it == _map_rtp_process.end() && !makeNew) {
        return nullptr;
    }
    if (it != _map_rtp_process.end() && makeNew) {
        //已经被其他线程持有了，不得再被持有，否则会存在线程安全的问题
        throw ProcessExisted(StrPrinter << "RtpProcess(" << stream_id << ") already existed");
    }
    RtpProcessHelper::Ptr &ref = _map_rtp_process[stream_id];
    if (!ref) {
        ref = std::make_shared<RtpProcessHelper>(stream_id, shared_from_this());
        ref->attachEvent();
        createTimer();
    }
    return ref->getProcess();
}

void RtpSelector::createTimer() {
    if (!_timer) {
        //创建超时管理定时器
        weak_ptr<RtpSelector> weakSelf = shared_from_this();
        _timer = std::make_shared<Timer>(3.0f, [weakSelf] {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            strongSelf->onManager();
            return true;
        }, EventPollerPool::Instance().getPoller());
    }
}

void RtpSelector::delProcess(const string &stream_id,const RtpProcess *ptr) {
    RtpProcess::Ptr process;
    {
        lock_guard<decltype(_mtx_map)> lck(_mtx_map);
        auto it = _map_rtp_process.find(stream_id);
        if (it == _map_rtp_process.end()) {
            return;
        }
        if (it->second->getProcess().get() != ptr) {
            return;
        }
        process = it->second->getProcess();
        _map_rtp_process.erase(it);
    }
    process->onDetach();
}

void RtpSelector::onManager() {
    List<RtpProcess::Ptr> clear_list;
    {
        lock_guard<decltype(_mtx_map)> lck(_mtx_map);
        for (auto it = _map_rtp_process.begin(); it != _map_rtp_process.end();) {
            if (it->second->getProcess()->alive()) {
                ++it;
                continue;
            }
            WarnL << "RtpProcess timeout:" << it->first;
            clear_list.emplace_back(it->second->getProcess());
            it = _map_rtp_process.erase(it);
        }
    }

    clear_list.for_each([](const RtpProcess::Ptr &process) {
        process->onDetach();
    });
}

RtpProcessHelper::RtpProcessHelper(const string &stream_id, const weak_ptr<RtpSelector> &parent) {
    _stream_id = stream_id;
    _parent = parent;
    _process = std::make_shared<RtpProcess>(stream_id);
}

RtpProcessHelper::~RtpProcessHelper() {
    auto process = std::move(_process);
    try {
        // flush时，确保线程安全
        process->getOwnerPoller(MediaSource::NullMediaSource())->async([process]() { process->flush(); });
    } catch (...) {
        // 忽略getOwnerPoller可能抛出的异常
    }
}

void RtpProcessHelper::attachEvent() {
    //主要目的是close回调触发时能把对象从RtpSelector中删除
    _process->setDelegate(shared_from_this());
}

bool RtpProcessHelper::close(MediaSource &sender) {
    //此回调在其他线程触发
    auto parent = _parent.lock();
    if (!parent) {
        return false;
    }
    parent->delProcess(_stream_id, _process.get());
    WarnL << "close media: " << sender.getUrl();
    return true;
}

RtpProcess::Ptr &RtpProcessHelper::getProcess() {
    return _process;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)