/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <type_traits>
#include "RtpMultiCaster.h"
#include "Util/util.h"
#include "Network/sockutil.h"
#include "RtspSession.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

MultiCastAddressMaker &MultiCastAddressMaker::Instance() {
    static MultiCastAddressMaker instance;
    return instance;
}

static uint32_t addressToInt(const string &ip){
    struct in_addr addr;
    bzero(&addr,sizeof(addr));
    addr.s_addr =  inet_addr(ip.data());
    return (uint32_t)ntohl((uint32_t &)addr.s_addr);
}

std::shared_ptr<uint32_t> MultiCastAddressMaker::obtain(uint32_t iTry) {
    lock_guard<recursive_mutex> lck(_mtx);
    GET_CONFIG(string,addrMinStr,MultiCast::kAddrMin);
    GET_CONFIG(string,addrMaxStr,MultiCast::kAddrMax);
    uint32_t addrMin = addressToInt(addrMinStr);
    uint32_t addrMax = addressToInt(addrMaxStr);

    if(_iAddr > addrMax || _iAddr == 0){
        _iAddr = addrMin;
    }
    auto iGotAddr =  _iAddr++;
    if(_setBadAddr.find(iGotAddr) != _setBadAddr.end()){
        //已经分配过了
        if(iTry){
            return obtain(--iTry);
        }
        //分配完了,应该不可能到这里
        ErrorL;
        return nullptr;
    }
    _setBadAddr.emplace(iGotAddr);
    std::shared_ptr<uint32_t> ret(new uint32_t(iGotAddr),[](uint32_t *ptr){
        MultiCastAddressMaker::Instance().release(*ptr);
        delete ptr;
    });
    return ret;
}
void MultiCastAddressMaker::release(uint32_t iAddr){
    lock_guard<recursive_mutex> lck(_mtx);
    _setBadAddr.erase(iAddr);
}


recursive_mutex RtpMultiCaster::g_mtx;
unordered_map<string, weak_ptr<RtpMultiCaster> > RtpMultiCaster::g_mapBroadCaster;

void RtpMultiCaster::setDetachCB(void* listener, const onDetach& cb) {
    lock_guard<recursive_mutex> lck(_mtx);
    if(cb){
        _mapDetach.emplace(listener,cb);
    }else{
        _mapDetach.erase(listener);
    }
}
RtpMultiCaster::~RtpMultiCaster() {
    _pReader->setReadCB(nullptr);
    _pReader->setDetachCB(nullptr);
    DebugL;
}

RtpMultiCaster::RtpMultiCaster(const EventPoller::Ptr &poller,const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream) {
    auto src = dynamic_pointer_cast<RtspMediaSource>(MediaSource::find(RTSP_SCHEMA,strVhost,strApp, strStream));
    if(!src){
        auto strErr = StrPrinter << "未找到媒体源:" << strVhost << " " << strApp << " " << strStream << endl;
        throw std::runtime_error(strErr);
    }
    _multiAddr = MultiCastAddressMaker::Instance().obtain();
    for(auto i = 0; i < 2; i++){
        _apUdpSock[i].reset(new Socket(poller));
        if(!_apUdpSock[i]->bindUdpSock(0, strLocalIp.data())){
            auto strErr = StrPrinter << "绑定UDP端口失败:" << strLocalIp << endl;
            throw std::runtime_error(strErr);
        }
        auto fd = _apUdpSock[i]->rawFD();
        GET_CONFIG(uint32_t,udpTTL,MultiCast::kUdpTTL);

        SockUtil::setMultiTTL(fd, udpTTL);
        SockUtil::setMultiLOOP(fd, false);
        SockUtil::setMultiIF(fd, strLocalIp.data());

        struct sockaddr_in &peerAddr = _aPeerUdpAddr[i];
        peerAddr.sin_family = AF_INET;
        peerAddr.sin_port = htons(_apUdpSock[i]->get_local_port());
        peerAddr.sin_addr.s_addr = htonl(*_multiAddr);
        bzero(&(peerAddr.sin_zero), sizeof peerAddr.sin_zero);
        _apUdpSock[i]->setSendPeerAddr((struct sockaddr *)&peerAddr);
    }
    _pReader = src->getRing()->attach(poller);
    _pReader->setReadCB([this](const RtspMediaSource::RingDataType &pkt){
        int i = 0;
        int size = pkt->size();
        pkt->for_each([&](const RtpPacket::Ptr &rtp) {
            int i = (int) (rtp->type);
            auto &pSock = _apUdpSock[i];
            auto &peerAddr = _aPeerUdpAddr[i];
            BufferRtp::Ptr buffer(new BufferRtp(rtp, 4));
            pSock->send(buffer, nullptr, 0, ++i == size);
        });
    });

    _pReader->setDetachCB([this](){
        unordered_map<void * , onDetach > _mapDetach_copy;
        {
            lock_guard<recursive_mutex> lck(_mtx);
            _mapDetach_copy = std::move(_mapDetach);
        }
        for(auto &pr : _mapDetach_copy){
            pr.second();
        }
    });
    DebugL << MultiCastAddressMaker::toString(*_multiAddr) << " "
            << _apUdpSock[0]->get_local_port() << " "
            << _apUdpSock[1]->get_local_port() << " "
            << strVhost << " "
            << strApp << " " << strStream;
}
uint16_t RtpMultiCaster::getPort(TrackType trackType){
    return _apUdpSock[trackType]->get_local_port();
}
string RtpMultiCaster::getIP(){
    return SockUtil::inet_ntoa(_aPeerUdpAddr[0].sin_addr);
}
RtpMultiCaster::Ptr RtpMultiCaster::make(const EventPoller::Ptr &poller,const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream){
    try{
        auto ret = Ptr(new RtpMultiCaster(poller,strLocalIp,strVhost,strApp,strStream),[poller](RtpMultiCaster *ptr){
            poller->async([ptr]() {
                delete ptr;
            });
        });
        lock_guard<recursive_mutex> lck(g_mtx);
        string strKey = StrPrinter << strLocalIp << " "  << strVhost << " " << strApp << " " << strStream << endl;
        weak_ptr<RtpMultiCaster> weakPtr = ret;
        g_mapBroadCaster.emplace(strKey,weakPtr);
        return ret;
    }catch (std::exception &ex) {
        WarnL << ex.what();
        return nullptr;
    }
}

RtpMultiCaster::Ptr RtpMultiCaster::get(const EventPoller::Ptr &poller,const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream) {
    string strKey = StrPrinter << strLocalIp << " " << strVhost << " " << strApp << " " << strStream << endl;
    lock_guard<recursive_mutex> lck(g_mtx);
    auto it = g_mapBroadCaster.find(strKey);
    if (it == g_mapBroadCaster.end()) {
        return make(poller,strLocalIp,strVhost,strApp, strStream);
    }
    auto ret = it->second.lock();
    if (!ret) {
        g_mapBroadCaster.erase(it);
        return make(poller,strLocalIp,strVhost,strApp, strStream);
    }
    return ret;
}


}//namespace mediakit
