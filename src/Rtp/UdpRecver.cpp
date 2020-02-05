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
#include "UdpRecver.h"
#include "RtpSelector.h"
namespace mediakit{

UdpRecver::UdpRecver() {
}

UdpRecver::~UdpRecver() {
}

bool UdpRecver::initSock(uint16_t local_port,const char *local_ip) {
    _sock.reset(new Socket(nullptr, false));
    onceToken token(nullptr,[&](){
        SockUtil::setRecvBuf(_sock->rawFD(),4 * 1024 * 1024);
    });

    auto &ref = RtpSelector::Instance();
    _sock->setOnRead([&ref](const Buffer::Ptr &buf, struct sockaddr *addr, int ){
        ref.inputRtp(buf->data(),buf->size(),addr);
    });
    return _sock->bindUdpSock(local_port,local_ip);
}

EventPoller::Ptr UdpRecver::getPoller() {
    return _sock->getPoller();
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)