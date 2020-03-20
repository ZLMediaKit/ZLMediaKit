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
