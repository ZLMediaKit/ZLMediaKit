/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_TCP_PRIVATE_H
#define MK_TCP_PRIVATE_H

#include "mk_tcp.h"
#include "Network/TcpClient.h"
#include "Network/TcpSession.h"
using namespace toolkit;

class TcpClientForC : public TcpClient {
public:
    typedef std::shared_ptr<TcpClientForC> Ptr;
    TcpClientForC(mk_tcp_client_events *events) ;
    ~TcpClientForC() override ;
    void onRecv(const Buffer::Ptr &pBuf) override;
    void onErr(const SockException &ex) override;
    void onManager() override;
    void onConnect(const SockException &ex) override;
    void setClient(mk_tcp_client client);
    void *_user_data;
private:
    mk_tcp_client_events _events;
    mk_tcp_client _client;
};

class TcpSessionForC : public TcpSession {
public:
    TcpSessionForC(const Socket::Ptr &pSock) ;
    ~TcpSessionForC() override = default;
    void onRecv(const Buffer::Ptr &buffer) override ;
    void onError(const SockException &err) override;
    void onManager() override;
    void *_user_data;
    uint16_t _local_port;
};

#endif //MK_TCP_PRIVATE_H
