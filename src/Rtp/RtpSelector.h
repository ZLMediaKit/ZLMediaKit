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

#ifndef ZLMEDIAKIT_RTPSELECTOR_H
#define ZLMEDIAKIT_RTPSELECTOR_H

#if defined(ENABLE_RTPPROXY)
#include <stdint.h>
#include <mutex>
#include <unordered_map>
#include "RtpProcess.h"

namespace mediakit{

class RtpSelector : public std::enable_shared_from_this<RtpSelector>{
public:
    RtpSelector();
    ~RtpSelector();

    static RtpSelector &Instance();
    bool inputRtp(const char *data,int data_len,const struct sockaddr *addr ,uint32_t *dts_out = nullptr );
    static uint32_t getSSRC(const char *data,int data_len);
    RtpProcess::Ptr getProcess(uint32_t ssrc,bool makeNew);
    void delProcess(uint32_t ssrc,const RtpProcess *ptr);
private:
    void onManager();
private:
    unordered_map<uint32_t,RtpProcess::Ptr> _map_rtp_process;
    recursive_mutex _mtx_map;
    Ticker _last_rtp_time;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSELECTOR_H
