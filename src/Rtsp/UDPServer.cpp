/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "UDPServer.h"
#include "Util/TimeTicker.h"
#include "Util/onceToken.h"

using namespace toolkit;
using namespace std;

namespace mediakit {

INSTANCE_IMP(UDPServer);
    
UDPServer::UDPServer() {
}

UDPServer::~UDPServer() {
    InfoL;
}

Socket::Ptr UDPServer::getSock(SocketHelper &helper, const char* local_ip, int interleaved, uint16_t local_port) {
    lock_guard<mutex> lck(_mtx_udp_sock);
    string key = StrPrinter << local_ip << ":" << interleaved << endl;
    auto it = _udp_sock_map.find(key);
    if (it == _udp_sock_map.end()) {
        Socket::Ptr sock = helper.createSocket();
        if (!sock->bindUdpSock(local_port, local_ip)) {
            //分配失败
            return nullptr;
        }

        sock->setOnErr(bind(&UDPServer::onErr, this, key, placeholders::_1));
        sock->setOnRead(bind(&UDPServer::onRecv, this, interleaved, placeholders::_1, placeholders::_2));
        _udp_sock_map[key] = sock;
        DebugL << local_ip << " " << sock->get_local_port() << " " << interleaved;
        return sock;
    }
    return it->second;
}

void UDPServer::listenPeer(const char* peer_ip, void* obj, const onRecvData &cb) {
    lock_guard<mutex> lck(_mtx_on_recv);
    auto &ref = _on_recv_map[peer_ip];
    ref.emplace(obj, cb);
}

void UDPServer::stopListenPeer(const char* peer_ip, void* obj) {
    lock_guard<mutex> lck(_mtx_on_recv);
    auto it0 = _on_recv_map.find(peer_ip);
    if (it0 == _on_recv_map.end()) {
        return;
    }
    auto &ref = it0->second;
    auto it1 = ref.find(obj);
    if (it1 != ref.end()) {
        ref.erase(it1);
    }
    if (ref.size() == 0) {
        _on_recv_map.erase(it0);
    }
}

void UDPServer::onErr(const string &key, const SockException &err) {
    WarnL << err.what();
    lock_guard<mutex> lck(_mtx_udp_sock);
    _udp_sock_map.erase(key);
}

void UDPServer::onRecv(int interleaved, const Buffer::Ptr &buf, struct sockaddr* peer_addr) {
    string peer_ip = SockUtil::inet_ntoa(peer_addr);
    lock_guard<mutex> lck(_mtx_on_recv);
    auto it0 = _on_recv_map.find(peer_ip);
    if (it0 == _on_recv_map.end()) {
        return;
    }
    auto &ref = it0->second;
    for (auto it1 = ref.begin(); it1 != ref.end(); ++it1) {
        auto &func = it1->second;
        if (!func(interleaved, buf, peer_addr)) {
            it1 = ref.erase(it1);
        }
    }
    if (ref.size() == 0) {
        _on_recv_map.erase(it0);
    }
}

} /* namespace mediakit */


