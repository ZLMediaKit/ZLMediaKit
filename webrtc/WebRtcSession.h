/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */


#ifndef ZLMEDIAKIT_WEBRTCSESSION_H
#define ZLMEDIAKIT_WEBRTCSESSION_H

#include "Network/Session.h"
#include "IceServer.hpp"
#include "WebRtcTransport.h"

using namespace toolkit;

class WebRtcSession : public UdpSession {
public:
    WebRtcSession(const Socket::Ptr &sock);
    ~WebRtcSession() override;

    void onRecv(const Buffer::Ptr &) override;
    void onError(const SockException &err) override;
    void onManager() override;

private:
    std::shared_ptr<WebRtcTransport> createTransport(const string &user_name);

private:
    struct sockaddr _peer_addr;
    std::shared_ptr<WebRtcTransport> _transport;
};


#endif //ZLMEDIAKIT_WEBRTCSESSION_H
