/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtpCache.h"

#if defined(ENABLE_RTPPROXY)

using namespace toolkit;

namespace mediakit{

RtpCache::RtpCache(onFlushed cb) {
    _cb = std::move(cb);
}
bool RtpCache::firstKeyReady(bool in) {
    if(_first_key){
        return _first_key;
    }
    _first_key = in;
    return _first_key;
}
void RtpCache::onFlush(std::shared_ptr<List<Buffer::Ptr> > rtp_list, bool) {
    _cb(std::move(rtp_list));
}

void RtpCache::input(uint64_t stamp, Buffer::Ptr buffer,bool is_key ) {
    inputPacket(stamp, true, std::move(buffer), is_key);
}

void RtpCachePS::onRTP(Buffer::Ptr buffer,bool is_key) {
    if(!firstKeyReady(is_key)){
        return;
    }
    auto rtp = std::static_pointer_cast<RtpPacket>(buffer);
    auto stamp = rtp->getStampMS();
    input(stamp, std::move(buffer),is_key);
}

void RtpCacheRaw::onRTP(Buffer::Ptr buffer,bool is_key) {
    if(!firstKeyReady(is_key)){
        return;
    }
    auto rtp = std::static_pointer_cast<RtpPacket>(buffer);
    auto stamp = rtp->getStampMS();
    input(stamp, std::move(buffer),is_key);
}

}//namespace mediakit

#endif//#if defined(ENABLE_RTPPROXY)