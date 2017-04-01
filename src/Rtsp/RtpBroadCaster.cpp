/*
 * RtpBroadCaster.cpp
 *
 *  Created on: 2016年12月23日
 *      Author: xzl
 */

#include "Network/sockutil.h"
#include "RtpBroadCaster.h"
#include "Util/util.h"
#include <arpa/inet.h>
#include <list>

namespace ZL {
namespace Rtsp {


std::shared_ptr<uint32_t> MultiCastAddressMaker::obtain(uint32_t iTry) {
	lock_guard<recursive_mutex> lck(m_mtx);
	static uint32_t addrMin = mINI::Instance()[Config::MultiCast::kAddrMin].as<uint32_t>();
	static uint32_t addrMax = mINI::Instance()[Config::MultiCast::kAddrMax].as<uint32_t>();
	if(m_iAddr > addrMax){
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
		auto val = *ptr;
		MultiCastAddressMaker::Instance().release(val);
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
	DebugL;
}
RtpBroadCaster::RtpBroadCaster(const string &strLocalIp,const string &strApp,const string &strStream) {
	auto src = RtspMediaSource::find(strApp, strStream);
	if(!src){
		auto strErr = StrPrinter << "未找到媒体源：" << strApp << " " << strStream << endl;
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
		static uint32_t udpTTL = mINI::Instance()[Config::MultiCast::kUdpTTL].as<uint32_t>();
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
		pSock->sendTo((char *) pkt->payload + 4, pkt->length - 4,(struct sockaddr *)(&peerAddr));
	});
	m_pReader->setDetachCB([this](){
		lock_guard<recursive_mutex> lck(m_mtx);
		list<onDetach> lst;
		for(auto &pr : m_mapDetach){
			lst.emplace_back(pr.second);
		}
		m_mapDetach.clear();

		for(auto &cb : lst){
			cb();
		}
	});
	DebugL << MultiCastAddressMaker::toString(*m_multiAddr) << " "
			<< m_apUdpSock[0]->get_local_port() << " "
			<< m_apUdpSock[1]->get_local_port() << " "
			<< strApp << " " << strStream;
}
uint16_t RtpBroadCaster::getPort(int iTrackId){
	int i = iTrackId%2;
	return m_apUdpSock[i]->get_local_port();
}
string RtpBroadCaster::getIP(){
	return inet_ntoa(m_aPeerUdpAddr[0].sin_addr);
}
RtpBroadCaster::Ptr RtpBroadCaster::make(const string &strLocalIp,const string &strApp,const string &strStream){
	try{
		auto ret = Ptr(new RtpBroadCaster(strLocalIp,strApp,strStream));
		lock_guard<recursive_mutex> lck(g_mtx);
		string strKey = StrPrinter << strLocalIp << " " << strApp << " " << strStream << endl;
		weak_ptr<RtpBroadCaster> weakPtr = ret;
		g_mapBroadCaster.emplace(strKey,weakPtr);
		return ret;
	}catch (std::exception &ex) {
		WarnL << ex.what();
		return nullptr;
	}
}

RtpBroadCaster::Ptr RtpBroadCaster::get(const string &strLocalIp,const string& strApp, const string& strStream) {
	string strKey = StrPrinter << strLocalIp << " " << strApp << " " << strStream << endl;
	lock_guard<recursive_mutex> lck(g_mtx);
	auto it = g_mapBroadCaster.find(strKey);
	if (it == g_mapBroadCaster.end()) {
		return make(strLocalIp,strApp, strStream);
	}
	auto ret = it->second.lock();
	if (!ret) {
		g_mapBroadCaster.erase(it);
		return make(strLocalIp,strApp, strStream);
	}
	return ret;
}



} /* namespace Rtsp */
} /* namespace ZL */


