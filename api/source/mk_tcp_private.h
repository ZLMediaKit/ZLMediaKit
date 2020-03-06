/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
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
