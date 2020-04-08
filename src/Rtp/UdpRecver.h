/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_UDPRECVER_H
#define ZLMEDIAKIT_UDPRECVER_H

#if defined(ENABLE_RTPPROXY)
#include <memory>
#include "Network/Socket.h"
using namespace std;
using namespace toolkit;

namespace mediakit{

/**
 * 组播接收器
 */
class UdpRecver {
public:
    typedef std::shared_ptr<UdpRecver> Ptr;
    typedef function<void(const Buffer::Ptr &buf)> onRecv;

    UdpRecver();
    virtual ~UdpRecver();
    bool initSock(uint16_t local_port,const char *local_ip = "0.0.0.0");
    EventPoller::Ptr getPoller();
protected:
    Socket::Ptr _sock;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_UDPRECVER_H
