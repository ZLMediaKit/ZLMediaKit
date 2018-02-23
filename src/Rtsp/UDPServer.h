/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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
	typedef function< bool(int, const Buffer::Ptr &, struct sockaddr *)> onRecvData;
	UDPServer();
	virtual ~UDPServer();
	static UDPServer &Instance() {
		static UDPServer *instance(new UDPServer());
		return *instance;
	}
	static void Destory() {
		delete &UDPServer::Instance();
	}
	Socket::Ptr getSock(const char *strLocalIp, int iTrackIndex,uint16_t iLocalPort = 0);
	void listenPeer(const char *strPeerIp, void *pSelf, const onRecvData &cb);
	void stopListenPeer(const char *strPeerIp, void *pSelf);
private:
	void onRcvData(int iTrackId, const Buffer::Ptr &pBuf,struct sockaddr *pPeerAddr);
	void onErr(const string &strKey,const SockException &err);
	unordered_map<string, Socket::Ptr> m_mapUpdSock;
	mutex m_mtxUpdSock;

	unordered_map<string, unordered_map<void *, onRecvData> > m_mapDataHandler;
	mutex m_mtxDataHandler;
};

} /* namespace Rtsp */
} /* namespace ZL */

#endif /* RTSP_UDPSERVER_H_ */
