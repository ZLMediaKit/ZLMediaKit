/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "UDPServer.h"
#include "Util/TimeTicker.h"
#include "Util/onceToken.h"

using namespace toolkit;

namespace mediakit {

INSTANCE_IMP(UDPServer);
    
UDPServer::UDPServer() {
}

UDPServer::~UDPServer() {
    InfoL;
}

Socket::Ptr UDPServer::getSock(const EventPoller::Ptr &poller,const char* strLocalIp, int intervaled,uint16_t iLocalPort) {
    lock_guard<mutex> lck(_mtxUpdSock);
    string strKey = StrPrinter << strLocalIp << ":" << intervaled << endl;
    auto it = _mapUpdSock.find(strKey);
    if (it == _mapUpdSock.end()) {
        Socket::Ptr pSock(new Socket(poller));
        //InfoL<<localIp;
        if (!pSock->bindUdpSock(iLocalPort, strLocalIp)) {
            //分配失败
            return nullptr;
        }

        pSock->setOnRead(bind(&UDPServer::onRcvData, this, intervaled, placeholders::_1,placeholders::_2));
        pSock->setOnErr(bind(&UDPServer::onErr, this, strKey, placeholders::_1));
        _mapUpdSock[strKey] = pSock;
        DebugL << strLocalIp << " " << pSock->get_local_port() << " " << intervaled;
        return pSock;
    }
    return it->second;
}

void UDPServer::listenPeer(const char* strPeerIp, void* pSelf, const onRecvData& cb) {
    lock_guard<mutex> lck(_mtxDataHandler);
    auto &mapRef = _mapDataHandler[strPeerIp];
    mapRef.emplace(pSelf, cb);
}

void UDPServer::stopListenPeer(const char* strPeerIp, void* pSelf) {
    lock_guard<mutex> lck(_mtxDataHandler);
    auto it0 = _mapDataHandler.find(strPeerIp);
    if (it0 == _mapDataHandler.end()) {
        return;
    }
    auto &mapRef = it0->second;
    auto it1 = mapRef.find(pSelf);
    if (it1 != mapRef.end()) {
        mapRef.erase(it1);
    }
    if (mapRef.size() == 0) {
        _mapDataHandler.erase(it0);
    }

}
void UDPServer::onErr(const string& strKey, const SockException& err) {
    WarnL << err.what();
    lock_guard<mutex> lck(_mtxUpdSock);
    _mapUpdSock.erase(strKey);
}
void UDPServer::onRcvData(int intervaled, const Buffer::Ptr &pBuf, struct sockaddr* pPeerAddr) {
    //TraceL << trackIndex;
    struct sockaddr_in *in = (struct sockaddr_in *) pPeerAddr;
    string peerIp = SockUtil::inet_ntoa(in->sin_addr);
    lock_guard<mutex> lck(_mtxDataHandler);
    auto it0 = _mapDataHandler.find(peerIp);
    if (it0 == _mapDataHandler.end()) {
        return;
    }
    auto &mapRef = it0->second;
    for (auto it1 = mapRef.begin(); it1 != mapRef.end(); ++it1) {
        onRecvData &funRef = it1->second;
        if (!funRef(intervaled, pBuf, pPeerAddr)) {
            it1 = mapRef.erase(it1);
        }
    }
    if (mapRef.size() == 0) {
        _mapDataHandler.erase(it0);
    }
}

} /* namespace mediakit */


