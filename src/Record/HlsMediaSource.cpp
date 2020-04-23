/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HlsMediaSource.h"

namespace mediakit{

HlsCookieData::HlsCookieData(const MediaInfo &info, const std::shared_ptr<SockInfo> &sock_info) {
    _info = info;
    _sock_info = sock_info;
    _added = std::make_shared<bool>(false);
    addReaderCount();
}

void HlsCookieData::addReaderCount(){
    if(!*_added){
        auto src = dynamic_pointer_cast<HlsMediaSource>(MediaSource::find(HLS_SCHEMA,_info._vhost,_info._app,_info._streamid));
        if(src){
            src->modifyReaderCount(true);
            *_added = true;
            _src = src;
            _ring_reader = src->getRing()->attach(EventPollerPool::Instance().getPoller());
            auto added = _added;
            _ring_reader->setDetachCB([added](){
                //HlsMediaSource已经销毁
                *added = false;
            });
        }
    }
}

HlsCookieData::~HlsCookieData() {
    if (*_added) {
        auto src = _src.lock();
        if (src) {
            src->modifyReaderCount(false);
        }
        uint64_t duration = (_ticker.createdTime() - _ticker.elapsedTime()) / 1000;
        WarnL << _sock_info->getIdentifier() << "(" << _sock_info->get_peer_ip() << ":" << _sock_info->get_peer_port() << ") "
              << "HLS播放器(" << _info._vhost << "/" << _info._app << "/" << _info._streamid
              << ")断开,耗时(s):" << duration;

        GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);
        if (_bytes > iFlowThreshold * 1024) {
            NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _info, _bytes, duration, true, static_cast<SockInfo&>(*_sock_info));
        }
    }
}

void HlsCookieData::addByteUsage(uint64_t bytes) {
    addReaderCount();
    _bytes += bytes;
    _ticker.resetTime();
}


}//namespace mediakit

