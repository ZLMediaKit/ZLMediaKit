/*
 * MIT License
 *
 * Copyright (c) 2019 Gemfield <gemfield@civilnet.cn>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(ENABLE_RTPPROXY)
#include "RtpSelector.h"

namespace mediakit{

INSTANCE_IMP(RtpSelector);

bool RtpSelector::inputRtp(const char *data, int data_len,const struct sockaddr *addr,uint32_t *dts_out) {
    if(_last_rtp_time.elapsedTime() > 3000){
        _last_rtp_time.resetTime();
        onManager();
    }
    auto ssrc = getSSRC(data,data_len);
    if(!ssrc){
        WarnL << "get ssrc from rtp failed:" << data_len;
        return false;
    }
    auto process = getProcess(ssrc, true);
    if(process){
        return process->inputRtp(data,data_len, addr,dts_out);
    }
    return false;
}

uint32_t RtpSelector::getSSRC(const char *data, int data_len) {
    if(data_len < 12){
        return 0;
    }
    uint32_t *ssrc_ptr = (uint32_t *)(data + 8);
    return ntohl(*ssrc_ptr);
}

RtpProcess::Ptr RtpSelector::getProcess(uint32_t ssrc,bool makeNew) {
    lock_guard<decltype(_mtx_map)> lck(_mtx_map);
    auto it = _map_rtp_process.find(ssrc);
    if(it == _map_rtp_process.end() && !makeNew){
        return nullptr;
    }
    RtpProcess::Ptr &ref = _map_rtp_process[ssrc];
    if(!ref){
        ref = std::make_shared<RtpProcess>(ssrc);
    }
    return ref;
}

void RtpSelector::delProcess(uint32_t ssrc,const RtpProcess *ptr) {
    lock_guard<decltype(_mtx_map)> lck(_mtx_map);
    auto it = _map_rtp_process.find(ssrc);
    if(it == _map_rtp_process.end()){
        return;
    }

    if(it->second.get() != ptr){
        return;
    }

    _map_rtp_process.erase(it);
}

void RtpSelector::onManager() {
    lock_guard<decltype(_mtx_map)> lck(_mtx_map);
    for (auto it = _map_rtp_process.begin(); it != _map_rtp_process.end();) {
        if (it->second->alive()) {
            ++it;
            continue;
        }
        WarnL << "RtpProcess timeout:" << printSSRC(it->first);
        it = _map_rtp_process.erase(it);
    }
}

RtpSelector::RtpSelector() {
}

RtpSelector::~RtpSelector() {
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)