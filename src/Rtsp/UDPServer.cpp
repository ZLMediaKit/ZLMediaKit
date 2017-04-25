/*
 * UDPServer.cpp
 *
 *  Created on: 2016年8月12日
 *      Author: xzl
 */

#include <arpa/inet.h>
#include "UDPServer.h"
#include "Util/TimeTicker.h"

using namespace ZL::Util;

namespace ZL {
namespace Rtsp {

UDPServer::UDPServer() {
}

UDPServer::~UDPServer() {
	InfoL;
}

Socket::Ptr UDPServer::getSock(const char* strLocalIp, int iTrackIndex) {
	lock_guard<mutex> lck(m_mtxUpdSock);
	string strKey = StrPrinter << strLocalIp << ":" << iTrackIndex << endl;
	auto it = m_mapUpdSock.find(strKey);
	if (it == m_mapUpdSock.end()) {
		Socket::Ptr pSock(new Socket());
		//InfoL<<localIp;
		if (!pSock->bindUdpSock(0, strLocalIp)) {
			//系统随机分配端口
			return nullptr;
		}

		pSock->setOnRead(bind(&UDPServer::onRcvData, this, iTrackIndex, placeholders::_1,placeholders::_2));
		pSock->setOnErr(bind(&UDPServer::onErr, this, strKey, placeholders::_1));
		m_mapUpdSock[strKey] = pSock;
		DebugL << strLocalIp << " " << pSock->get_local_port() << " " << iTrackIndex;
		return pSock;
	}
	return it->second;
}

void UDPServer::listenPeer(const char* strPeerIp, void* pSelf, const onRecvData& cb) {
	lock_guard<mutex> lck(m_mtxDataHandler);
	auto &mapRef = m_mapDataHandler[strPeerIp];
	mapRef.emplace(pSelf, cb);
}

void UDPServer::stopListenPeer(const char* strPeerIp, void* pSelf) {
	lock_guard<mutex> lck(m_mtxDataHandler);
	auto it0 = m_mapDataHandler.find(strPeerIp);
	if (it0 == m_mapDataHandler.end()) {
		return;
	}
	auto &mapRef = it0->second;
	auto it1 = mapRef.find(pSelf);
	if (it1 != mapRef.end()) {
		mapRef.erase(it1);
	}
	if (mapRef.size() == 0) {
		m_mapDataHandler.erase(it0);
	}

}
void UDPServer::onErr(const string& strKey, const SockException& err) {
	WarnL << err.what();
	lock_guard<mutex> lck(m_mtxUpdSock);
	m_mapUpdSock.erase(strKey);
}
void UDPServer::onRcvData(int iTrackIndex, const Socket::Buffer::Ptr &pBuf, struct sockaddr* pPeerAddr) {
	//TraceL << trackIndex;
	struct sockaddr_in *in = (struct sockaddr_in *) pPeerAddr;
	string peerIp = inet_ntoa(in->sin_addr);
	lock_guard<mutex> lck(m_mtxDataHandler);
	auto it0 = m_mapDataHandler.find(peerIp);
	if (it0 == m_mapDataHandler.end()) {
		return;
	}
	auto &mapRef = it0->second;
	for (auto it1 = mapRef.begin(); it1 != mapRef.end(); ++it1) {
		onRecvData &funRef = it1->second;
		if (!funRef(iTrackIndex, pBuf, pPeerAddr)) {
			it1 = mapRef.erase(it1);
		}
	}
	if (mapRef.size() == 0) {
		m_mapDataHandler.erase(it0);
	}
}

} /* namespace Rtsp */
} /* namespace ZL */


