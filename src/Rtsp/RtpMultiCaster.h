/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTSP_RTPBROADCASTER_H_
#define SRC_RTSP_RTPBROADCASTER_H_


#include <mutex>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include "Common/config.h"
#include "RtspMediaSource.h"
#include "Util/mini.h"
#include "Network/Socket.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

class MultiCastAddressMaker
{
public:
    static MultiCastAddressMaker &Instance();

    static bool isMultiCastAddress(uint32_t iAddr){
        static uint32_t addrMin = mINI::Instance()[MultiCast::kAddrMin].as<uint32_t>();
        static uint32_t addrMax = mINI::Instance()[MultiCast::kAddrMax].as<uint32_t>();
        return iAddr >= addrMin && iAddr <= addrMax;
    }
    static string toString(uint32_t iAddr){
        iAddr = htonl(iAddr);
        return SockUtil::inet_ntoa((struct in_addr &)(iAddr));
    }
    virtual ~MultiCastAddressMaker(){}
    std::shared_ptr<uint32_t> obtain(uint32_t iTry = 10);
private:
    MultiCastAddressMaker(){};
    void release(uint32_t iAddr);
    uint32_t _iAddr = 0;
    recursive_mutex _mtx;
    unordered_set<uint32_t> _setBadAddr;
};
class RtpMultiCaster {
public:
    typedef std::shared_ptr<RtpMultiCaster> Ptr;
    typedef function<void()> onDetach;
    virtual ~RtpMultiCaster();
    static Ptr get(const EventPoller::Ptr &poller,const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream);
    void setDetachCB(void *listener,const onDetach &cb);
    uint16_t getPort(TrackType trackType);
    string getIP();
private:
    static recursive_mutex g_mtx;
    static unordered_map<string , weak_ptr<RtpMultiCaster> > g_mapBroadCaster;
    static Ptr make(const EventPoller::Ptr &poller,const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream);

    std::shared_ptr<uint32_t> _multiAddr;
    recursive_mutex _mtx;
    unordered_map<void * , onDetach > _mapDetach;
    RtspMediaSource::RingType::RingReader::Ptr _pReader;
    Socket::Ptr _apUdpSock[2];
    struct sockaddr_in _aPeerUdpAddr[2];

    RtpMultiCaster(const EventPoller::Ptr &poller,const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream);

};

}//namespace mediakit

#endif /* SRC_RTSP_RTPBROADCASTER_H_ */
