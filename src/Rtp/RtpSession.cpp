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
#include "RtpSession.h"
#include "RtpSelector.h"
namespace mediakit{

RtpSession::RtpSession(const Socket::Ptr &sock) : TcpSession(sock) {
    DebugP(this);
    socklen_t addr_len = sizeof(addr);
    getpeername(sock->rawFD(), &addr, &addr_len);
}
RtpSession::~RtpSession() {
    DebugP(this);
    if(_ssrc){
        RtpSelector::Instance().delProcess(_ssrc,_process.get());
    }
}

void RtpSession::onRecv(const Buffer::Ptr &data) {
    try {
        RtpSplitter::input(data->data(), data->size());
    } catch (SockException &ex) {
        shutdown(ex);
    } catch (std::exception &ex) {
        shutdown(SockException(Err_other, ex.what()));
    }
}

void RtpSession::onError(const SockException &err) {
    WarnL << _ssrc << " " << err.what();
}

void RtpSession::onManager() {
    if(_process && !_process->alive()){
        shutdown(SockException(Err_timeout, "receive rtp timeout"));
    }

    if(!_process && _ticker.createdTime() > 10 * 1000){
        shutdown(SockException(Err_timeout, "illegal connection"));
    }
}

void RtpSession::onRtpPacket(const char *data, uint64_t len) {
    if(!_ssrc){
        _ssrc = RtpSelector::getSSRC(data + 2,len - 2);
        _process = RtpSelector::Instance().getProcess(_ssrc, true);
    }
    _process->inputRtp(data + 2,len - 2,&addr);
    _ticker.resetTime();
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)