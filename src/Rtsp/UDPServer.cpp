/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
	string peerIp = inet_ntoa(in->sin_addr);
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


