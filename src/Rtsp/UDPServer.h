/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTSP_UDPSERVER_H_
#define RTSP_UDPSERVER_H_

#include <stdint.h>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/Socket.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class UDPServer : public std::enable_shared_from_this<UDPServer> {
public:
    typedef function< bool(int intervaled, const Buffer::Ptr &buffer, struct sockaddr *peer_addr)> onRecvData;
    ~UDPServer();
    static UDPServer &Instance();
    Socket::Ptr getSock(const EventPoller::Ptr &poller,const char *strLocalIp, int intervaled,uint16_t iLocalPort = 0);
    void listenPeer(const char *strPeerIp, void *pSelf, const onRecvData &cb);
    void stopListenPeer(const char *strPeerIp, void *pSelf);
private:
    UDPServer();
    void onRcvData(int intervaled, const Buffer::Ptr &pBuf,struct sockaddr *pPeerAddr);
    void onErr(const string &strKey,const SockException &err);
    unordered_map<string, Socket::Ptr> _mapUpdSock;
    mutex _mtxUpdSock;

    unordered_map<string, unordered_map<void *, onRecvData> > _mapDataHandler;
    mutex _mtxDataHandler;
};

} /* namespace mediakit */

#endif /* RTSP_UDPSERVER_H_ */
