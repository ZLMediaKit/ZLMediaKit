/*
 * UDPServer.h
 *
 *  Created on: 2016年8月12日
 *      Author: xzl
 */

#ifndef RTSP_UDPSERVER_H_
#define RTSP_UDPSERVER_H_

#include <mutex>
#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/Socket.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

class UDPServer {
public:
	typedef function< bool(int, const Socket::Buffer::Ptr &, struct sockaddr *)> onRecvData;
	UDPServer();
	virtual ~UDPServer();
	static UDPServer &Instance() {
		static UDPServer *instance(new UDPServer());
		return *instance;
	}
	static void Destory() {
		delete &UDPServer::Instance();
	}
	Socket::Ptr getSock(const char *strLocalIp, int iTrackIndex);
	void listenPeer(const char *strPeerIp, void *pSelf, const onRecvData &cb);
	void stopListenPeer(const char *strPeerIp, void *pSelf);
private:
	void onRcvData(int iTrackId, const Socket::Buffer::Ptr &pBuf,struct sockaddr *pPeerAddr);
	void onErr(const string &strKey,const SockException &err);
	unordered_map<string, Socket::Ptr> m_mapUpdSock;
	mutex m_mtxUpdSock;

	unordered_map<string, unordered_map<void *, onRecvData> > m_mapDataHandler;
	mutex m_mtxDataHandler;
};

} /* namespace Rtsp */
} /* namespace ZL */

#endif /* RTSP_UDPSERVER_H_ */
