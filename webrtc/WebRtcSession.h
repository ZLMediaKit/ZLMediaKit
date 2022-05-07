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

class WebRtcSession : public UdpSession {
public:
    WebRtcSession(const Socket::Ptr &sock);
    ~WebRtcSession() override;

    void onRecv(const Buffer::Ptr &) override;
    void onError(const SockException &err) override;
    void onManager() override;
    std::string getIdentifier() const override;

    static EventPoller::Ptr queryPoller(const Buffer::Ptr &buffer);

private:
    std::string _identifier;
    bool _find_transport = true;
    Ticker _ticker;
    struct sockaddr_storage _peer_addr;
    std::shared_ptr<WebRtcTransportImp> _transport;
};


#endif //ZLMEDIAKIT_WEBRTCSESSION_H
