/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "UdpRecver.h"
#include "RtpSelector.h"
namespace mediakit{

UdpRecver::UdpRecver() {
}

UdpRecver::~UdpRecver() {
    if(_sock){
        _sock->setOnRead(nullptr);
    }
}

bool UdpRecver::initSock(uint16_t local_port,const char *local_ip) {
    _sock.reset(new Socket(nullptr, false));
    onceToken token(nullptr,[&](){
        SockUtil::setRecvBuf(_sock->rawFD(),4 * 1024 * 1024);
    });

    auto &ref = RtpSelector::Instance();
    auto sock = _sock;
    _sock->setOnRead([&ref, sock](const Buffer::Ptr &buf, struct sockaddr *addr, int ){
        ref.inputRtp(sock, buf->data(),buf->size(),addr);
    });
    return _sock->bindUdpSock(local_port,local_ip);
}

EventPoller::Ptr UdpRecver::getPoller() {
    return _sock->getPoller();
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)