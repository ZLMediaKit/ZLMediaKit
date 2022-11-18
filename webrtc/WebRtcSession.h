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
#include "Http/HttpRequestSplitter.h"

namespace toolkit {
    class TcpServer;
}

namespace mediakit {

class WebRtcSession : public Session, public HttpRequestSplitter {
public:
    WebRtcSession(const Socket::Ptr &sock);
    ~WebRtcSession() override;

    void attachServer(const Server &server) override;
    void onRecv(const Buffer::Ptr &) override;
    void onError(const SockException &err) override;
    void onManager() override;
    static EventPoller::Ptr queryPoller(const Buffer::Ptr &buffer);

private:
    //// HttpRequestSplitter override ////
    ssize_t onRecvHeader(const char *data, size_t len) override;
    const char *onSearchPacketTail(const char *data, size_t len) override;

    void onRecv_l(const char *data, size_t len);

private:
    bool _over_tcp = false;
    bool _find_transport = true;
    Ticker _ticker;
    struct sockaddr_storage _peer_addr;
    std::weak_ptr<toolkit::TcpServer> _server;
    std::shared_ptr<WebRtcTransportImp> _transport;
};

}// namespace mediakit

#endif //ZLMEDIAKIT_WEBRTCSESSION_H
