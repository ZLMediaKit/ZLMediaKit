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

#include <list>
#include <type_traits>
#include "RtpBroadCaster.h"
#include "Util/util.h"
#include "Network/sockutil.h"
#include "RtspSession.h"

using namespace std;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

static uint32_t addressToInt(const string &ip){
    struct in_addr addr;
    bzero(&addr,sizeof(addr));
	addr.s_addr =  inet_addr(ip.data());
    return (uint32_t)ntohl((uint32_t &)addr.s_addr);
}

std::shared_ptr<uint32_t> MultiCastAddressMaker::obtain(uint32_t iTry) {
	lock_guard<recursive_mutex> lck(m_mtx);
    GET_CONFIG_AND_REGISTER(string,addrMinStr,Config::MultiCast::kAddrMin);
    GET_CONFIG_AND_REGISTER(string,addrMaxStr,Config::MultiCast::kAddrMax);
    uint32_t addrMin = addressToInt(addrMinStr);
	uint32_t addrMax = addressToInt(addrMaxStr);

	if(m_iAddr > addrMax || m_iAddr == 0){
		m_iAddr = addrMin;
	}
	auto iGotAddr =  m_iAddr++;
	if(m_setBadAddr.find(iGotAddr) != m_setBadAddr.end()){
		//已经分配过了
		if(iTry){
			return obtain(--iTry);
		}
		//分配完了,应该不可能到这里
		FatalL;
		return nullptr;
	}
	m_setBadAddr.emplace(iGotAddr);
	std::shared_ptr<uint32_t> ret(new uint32_t(iGotAddr),[](uint32_t *ptr){
		MultiCastAddressMaker::Instance().release(*ptr);
		delete ptr;
	});
	return ret;
}
void MultiCastAddressMaker::release(uint32_t iAddr){
	lock_guard<recursive_mutex> lck(m_mtx);
	m_setBadAddr.erase(iAddr);
}


recursive_mutex RtpBroadCaster::g_mtx;
unordered_map<string, weak_ptr<RtpBroadCaster> > RtpBroadCaster::g_mapBroadCaster;

void RtpBroadCaster::setDetachCB(void* listener, const onDetach& cb) {
	lock_guard<recursive_mutex> lck(m_mtx);
	if(cb){
		m_mapDetach.emplace(listener,cb);
	}else{
		m_mapDetach.erase(listener);
	}
}
RtpBroadCaster::~RtpBroadCaster() {
	m_pReader->setReadCB(nullptr);
	m_pReader->setDetachCB(nullptr);
	DebugL;
}
RtpBroadCaster::RtpBroadCaster(const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream) {
	auto src = dynamic_pointer_cast<RtspMediaSource>(MediaSource::find(RTSP_SCHEMA,strVhost,strApp, strStream));
	if(!src){
		auto strErr = StrPrinter << "未找到媒体源:" << strVhost << " " << strApp << " " << strStream << endl;
		throw std::runtime_error(strErr);
	}
	m_multiAddr = MultiCastAddressMaker::Instance().obtain();
	for(auto i = 0; i < 2; i++){
		m_apUdpSock[i].reset(new Socket());
		if(!m_apUdpSock[i]->bindUdpSock(0, strLocalIp.data())){
			auto strErr = StrPrinter << "绑定UDP端口失败:" << strLocalIp << endl;
			throw std::runtime_error(strErr);
		}
		auto fd = m_apUdpSock[i]->rawFD();
        GET_CONFIG_AND_REGISTER(uint32_t,udpTTL,Config::MultiCast::kUdpTTL);

        SockUtil::setMultiTTL(fd, udpTTL);
		SockUtil::setMultiLOOP(fd, false);
		SockUtil::setMultiIF(fd, strLocalIp.data());

		struct sockaddr_in &peerAddr = m_aPeerUdpAddr[i];
		peerAddr.sin_family = AF_INET;
		peerAddr.sin_port = htons(m_apUdpSock[i]->get_local_port());
		peerAddr.sin_addr.s_addr = htonl(*m_multiAddr);
		bzero(&(peerAddr.sin_zero), sizeof peerAddr.sin_zero);
	}
	m_pReader = src->getRing()->attach();
	m_pReader->setReadCB([this](const RtpPacket::Ptr &pkt){
		int i = (pkt->interleaved/2)%2;
		auto &pSock = m_apUdpSock[i];
		auto &peerAddr = m_aPeerUdpAddr[i];
        BufferRtp::Ptr buffer(new BufferRtp(pkt,4));
		pSock->send(buffer,SOCKET_DEFAULE_FLAGS,(struct sockaddr *)(&peerAddr));
	});
	m_pReader->setDetachCB([this](){
		unordered_map<void * , onDetach > m_mapDetach_copy;
		{
			lock_guard<recursive_mutex> lck(m_mtx);
			m_mapDetach_copy = std::move(m_mapDetach);
		}
		for(auto &pr : m_mapDetach_copy){
			pr.second();
		}
	});
	DebugL << MultiCastAddressMaker::toString(*m_multiAddr) << " "
			<< m_apUdpSock[0]->get_local_port() << " "
			<< m_apUdpSock[1]->get_local_port() << " "
            << strVhost << " "
			<< strApp << " " << strStream;
}
uint16_t RtpBroadCaster::getPort(int iTrackId){
	int i = iTrackId%2;
	return m_apUdpSock[i]->get_local_port();
}
string RtpBroadCaster::getIP(){
	return inet_ntoa(m_aPeerUdpAddr[0].sin_addr);
}
RtpBroadCaster::Ptr RtpBroadCaster::make(const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream){
	try{
		auto ret = Ptr(new RtpBroadCaster(strLocalIp,strVhost,strApp,strStream));
		lock_guard<recursive_mutex> lck(g_mtx);
		string strKey = StrPrinter << strLocalIp << " "  << strVhost << " " << strApp << " " << strStream << endl;
		weak_ptr<RtpBroadCaster> weakPtr = ret;
		g_mapBroadCaster.emplace(strKey,weakPtr);
		return ret;
	}catch (std::exception &ex) {
		WarnL << ex.what();
		return nullptr;
	}
}

RtpBroadCaster::Ptr RtpBroadCaster::get(const string &strLocalIp,const string &strVhost,const string &strApp,const string &strStream) {
	string strKey = StrPrinter << strLocalIp << " " << strVhost << " " << strApp << " " << strStream << endl;
	lock_guard<recursive_mutex> lck(g_mtx);
	auto it = g_mapBroadCaster.find(strKey);
	if (it == g_mapBroadCaster.end()) {
		return make(strLocalIp,strVhost,strApp, strStream);
	}
	auto ret = it->second.lock();
	if (!ret) {
		g_mapBroadCaster.erase(it);
		return make(strLocalIp,strVhost,strApp, strStream);
	}
	return ret;
}



} /* namespace Rtsp */
} /* namespace ZL */


