/*
 * RtspSession.cpp
 *
 *  Created on: 2016年8月12日
 *      Author: xzl
 */

#include <atomic>
#include "Common/config.h"
#include "UDPServer.h"
#include "RtspSession.h"
#include "Device/base64.h"
#include "Util/mini.h"
#include "Util/onceToken.h"
#include "Util/TimeTicker.h"
#include "Util/NoticeCenter.h"
#include "Network/sockutil.h"

using namespace Config;
using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

string dateHeader() {
	char buf[200];
	time_t tt = time(NULL);
	strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	return buf;
}

unordered_map<string, weak_ptr<RtspSession> > RtspSession::g_mapGetter;
unordered_map<void *, std::shared_ptr<RtspSession> > RtspSession::g_mapPostter;
recursive_mutex RtspSession::g_mtxGetter; //对quicktime上锁保护
recursive_mutex RtspSession::g_mtxPostter; //对quicktime上锁保护
unordered_map<string, RtspSession::rtspCMDHandle> RtspSession::g_mapCmd;
string RtspSession::g_serverName;
RtspSession::RtspSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock) :
		TcpLimitedSession(pTh, pSock), m_pSender(pSock) {
	static onceToken token( []() {
		g_mapCmd.emplace("OPTIONS",&RtspSession::handleReq_Options);
		g_mapCmd.emplace("DESCRIBE",&RtspSession::handleReq_Describe);
		g_mapCmd.emplace("SETUP",&RtspSession::handleReq_Setup);
		g_mapCmd.emplace("PLAY",&RtspSession::handleReq_Play);
		g_mapCmd.emplace("PAUSE",&RtspSession::handleReq_Pause);
		g_mapCmd.emplace("TEARDOWN",&RtspSession::handleReq_Teardown);
		g_mapCmd.emplace("GET",&RtspSession::handleReq_Get);
		g_mapCmd.emplace("POST",&RtspSession::handleReq_Post);
		g_mapCmd.emplace("SET_PARAMETER",&RtspSession::handleReq_SET_PARAMETER);
		g_mapCmd.emplace("GET_PARAMETER",&RtspSession::handleReq_SET_PARAMETER);
		g_serverName = mINI::Instance()[Config::Rtsp::kServerName];
	}, []() {});

#ifndef __x86_64__
	SockUtil::setSendBuf(pSock->rawFD(), 32*1024);
	SockUtil::setCloseWait(pSock->rawFD(), 0);
	pSock->setSendPktSize(32);
#endif//__x86_64__

	DebugL <<  getPeerIp();
}

RtspSession::~RtspSession() {
	if (m_onDestory) {
		m_onDestory();
	}
	DebugL << getPeerIp();
}

void RtspSession::shutdown(){
	if (sock) {
		sock->emitErr(SockException(Err_other, "self shutdown"));
	}
	if (m_bBase64need && !sock) {
		//quickTime http postter,and self is detached from tcpServer
		lock_guard<recursive_mutex> lock(g_mtxPostter);
		g_mapPostter.erase(this);
	}
	if (m_pBrdcaster) {
		m_pBrdcaster->setDetachCB(this, nullptr);
		m_pBrdcaster.reset();
	}
	if (m_pRtpReader) {
		m_pRtpReader.reset();
	}
}

void RtspSession::onError(const SockException& err) {
	TraceL << err.getErrCode() << " " << err.what();
	if (m_bListenPeerUdpPort) {
		//取消UDP断口监听
		UDPServer::Instance().stopListenPeer(getPeerIp().data(), this);
		m_bListenPeerUdpPort = false;
	}
	if (!m_bBase64need && m_strSessionCookie.size() != 0) {
		//quickTime http getter
		lock_guard<recursive_mutex> lock(g_mtxGetter);
		g_mapGetter.erase(m_strSessionCookie);
	}

	if (m_bBase64need && err.getErrCode() == Err_eof) {
		//quickTime http postter,正在发送rtp; QuickTime只是断开了请求连接,请继续发送rtp
		sock = nullptr;
		lock_guard<recursive_mutex> lock(g_mtxPostter);
		//为了保证脱离TCPServer后还能正常运作,需要保持本对象的强引用
		g_mapPostter.emplace(this, dynamic_pointer_cast<RtspSession>(shared_from_this()));
		TraceL << "quickTime will not send request any more!";
	}
}

void RtspSession::onManager() {
	if (m_ticker.createdTime() > 10 * 1000) {
		if (m_strSession.size() == 0) {
			WarnL << "非法链接:" << getPeerIp();
			shutdown();
			return;
		}
		if (m_bListenPeerUdpPort) {
			UDPServer::Instance().stopListenPeer(getPeerIp().data(), this);
			m_bListenPeerUdpPort = false;
		}
	}
	if (m_rtpType != PlayerBase::RTP_TCP && m_ticker.elapsedTime() > 15 * 1000) {
		WarnL << "RTSP会话超时:" << getPeerIp();
		shutdown();
		return;
	}
}

void RtspSession::onRecv(const Socket::Buffer::Ptr &pBuf) {
	m_ticker.resetTime();
	char tmp[2 * 1024];
	m_pcBuf = tmp;
	if (m_bBase64need) {
		//quicktime 加密后的rtsp请求，需要解密
		av_base64_decode((uint8_t *) m_pcBuf, pBuf->data(), sizeof(tmp));
		m_parser.Parse(m_pcBuf); //rtsp请求解析
		//TraceL << buf;
	} else {
		//TraceL << request;
		m_parser.Parse(pBuf->data()); //rtsp请求解析
	}

	string strCmd = m_parser.Method(); //提取出请求命令字
	m_iCseq = atoi(m_parser["CSeq"].data());

	bool ret = false;
	auto it = g_mapCmd.find(strCmd);
	if (it != g_mapCmd.end()) {
		auto fun = it->second;
		ret = (this->*fun)();
		m_parser.Clear();
	} else {
		ret = (m_rtpType == PlayerBase::RTP_TCP);
	}
	if (!ret) {
		shutdown();
		WarnL << "cmd=" << strCmd;
	}
}

bool RtspSession::handleReq_Options() {
//支持这些命令
	int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY,"
			" PAUSE, SET_PARAMETER, GET_PARAMETER\r\n\r\n",
			m_iCseq, g_serverName.data(),
			RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	send(m_pcBuf, n);
	return true;
}

bool RtspSession::handleReq_Describe() {
	m_strUrl = m_parser.Url();
	if (m_strSession.size() != 0) {
		//会话id这时还没生成，这个逻辑可以注释以提高响应速度
		return false;
	}
	if (!findStream()) {
		//未找到相应的MediaSource
		send_StreamNotFound();
		return false;
	}
//回复sdp
	int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"x-Accept-Retransmit: our-retransmit\r\n"
			"x-Accept-Dynamic-Rate: 1\r\n"
			"Content-Base: %s/\r\n"
			"Content-Type: application/sdp\r\n"
			"Content-Length: %d\r\n\r\n%s",
			m_iCseq, g_serverName.data(),
			RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data(), m_strUrl.data(),
			(int) m_strSdp.length(), m_strSdp.data());
	send(m_pcBuf, n);
	return true;
}

inline void RtspSession::send_StreamNotFound() {
	int n = sprintf(m_pcBuf, "RTSP/1.0 404 Stream Not Found\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Connection: Close\r\n\r\n",
			m_iCseq, g_serverName.data(),
			RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	send(m_pcBuf, n);
}
inline void RtspSession::send_UnsupportedTransport() {
	int n = sprintf(m_pcBuf, "RTSP/1.0 461 Unsupported Transport\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Connection: Close\r\n\r\n",
			m_iCseq, g_serverName.data(),
			RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	send(m_pcBuf, n);
}

inline void RtspSession::send_SessionNotFound() {
	int n = sprintf(m_pcBuf, "RTSP/1.0 454 Session Not Found\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Connection: Close\r\n\r\n",
			m_iCseq, g_serverName.data(),
			RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	send(m_pcBuf, n);

	/*405 Method Not Allowed*/

}
bool RtspSession::handleReq_Setup() {
//处理setup命令，该函数可能进入多次
//track id
	int trackid = atoi( FindField( m_parser.Url().data(),
									m_aTrackInfo[0].trackStyle.data(),
									NULL).data());
	int trackIdx = getTrackIndexByTrackId(trackid);
	if (trackIdx == -1) {
		//未找到相应track
		return false;
	}
	RtspTrack &trackRef = m_aTrackInfo[trackIdx];
	if (trackRef.inited) {
		//已经初始化过该Track
		return false;
	}
	trackRef.inited = true; //现在初始化

	auto strongRing = m_pWeakRing.lock();
	if (!strongRing) {
		//the media source is released!
		send_NotAcceptable();
		return false;
	}

	if(!m_bSetUped){
		m_bSetUped = true;
		auto strTransport = m_parser["Transport"];
		if(strTransport.find("TCP") != string::npos){
			m_rtpType = PlayerBase::RTP_TCP;
		}else if(strTransport.find("multicast") != string::npos){
			m_rtpType = PlayerBase::RTP_MULTICAST;
		}else{
			m_rtpType = PlayerBase::RTP_UDP;
		}
	}

	if (!m_pRtpReader && m_rtpType != PlayerBase::RTP_MULTICAST) {
		m_pRtpReader = strongRing->attach();
		weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
		m_pRtpReader->setDetachCB([weakSelf]() {
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->safeShutdown();
		});
	}

	switch (m_rtpType) {
	case PlayerBase::RTP_TCP: {
		int iLen = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
				"CSeq: %d\r\n"
				"Server: %s-%0.2f(build in %s)\r\n"
				"%s"
				"Transport: RTP/AVP/TCP;unicast;"
				"interleaved=%d-%d;ssrc=%s;mode=play\r\n"
				"Session: %s\r\n"
				"x-Transport-Options: late-tolerance=1.400000\r\n"
				"x-Dynamic-Rate: 1\r\n\r\n",
				m_iCseq, g_serverName.data(),
				RTSP_VERSION, RTSP_BUILDTIME,
				dateHeader().data(), trackid * 2,
				trackid * 2 + 1,
				printSSRC(trackRef.ssrc).data(),
				m_strSession.data());
		send(m_pcBuf, iLen);
	}
		break;
	case PlayerBase::RTP_UDP: {
		auto pSock = UDPServer::Instance().getSock(getLocalIp().data(),trackIdx);
		if (!pSock) {
			//分配端口失败
			WarnL << "分配端口失败";
			send_NotAcceptable();
			return false;
		}
		m_apUdpSock[trackIdx] = pSock;
		int iSrvPort = pSock->get_local_port();
		string strClientPort = FindField(m_parser["Transport"].data(), "client_port=", NULL);
		uint16_t ui16PeerPort = atoi( FindField(strClientPort.data(), NULL, "-").data());
		struct sockaddr_in peerAddr;
		peerAddr.sin_family = AF_INET;
		peerAddr.sin_port = htons(ui16PeerPort);
		peerAddr.sin_addr.s_addr = inet_addr(getPeerIp().data());
		bzero(&(peerAddr.sin_zero), sizeof peerAddr.sin_zero);
		m_apPeerUdpAddr[trackIdx].reset((struct sockaddr *) (new struct sockaddr_in(peerAddr)));
		tryGetPeerUdpPort();
		//InfoL << "分配端口:" << srv_port;
		int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
				"CSeq: %d\r\n"
				"Server: %s-%0.2f(build in %s)\r\n"
				"%s"
				"Transport: RTP/AVP/UDP;unicast;"
				"client_port=%s;server_port=%d-%d;ssrc=%s;mode=play\r\n"
				"Session: %s\r\n\r\n",
				m_iCseq, g_serverName.data(),
				RTSP_VERSION, RTSP_BUILDTIME,
				dateHeader().data(), strClientPort.data(),
				iSrvPort, iSrvPort + 1,
				printSSRC(trackRef.ssrc).data(),
				m_strSession.data());
		send(m_pcBuf, n);
	}
		break;
	case PlayerBase::RTP_MULTICAST: {
		if(!m_pBrdcaster){
			m_pBrdcaster = RtpBroadCaster::get(getLocalIp(), m_strApp, m_strStream);
			if (!m_pBrdcaster) {
				send_NotAcceptable();
				return false;
			}
			weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
			m_pBrdcaster->setDetachCB(this, [weakSelf]() {
				auto strongSelf = weakSelf.lock();
				if(!strongSelf) {
					return;
				}
				strongSelf->safeShutdown();
			});
		}
		int iSrvPort = m_pBrdcaster->getPort(trackid);
		static uint32_t udpTTL = mINI::Instance()[MultiCast::kUdpTTL].as<uint32_t>();
		int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
				"CSeq: %d\r\n"
				"Server: %s-%0.2f(build in %s)\r\n"
				"%s"
				"Transport: RTP/AVP;multicast;destination=%s;"
				"source=%s;port=%d-%d;ttl=%d;ssrc=%s\r\n"
				"Session: %s\r\n\r\n",
				m_iCseq, g_serverName.data(),
				RTSP_VERSION, RTSP_BUILDTIME,
				dateHeader().data(), m_pBrdcaster->getIP().data(),
				getLocalIp().data(), iSrvPort, iSrvPort + 1,
				udpTTL,printSSRC(trackRef.ssrc).data(),
				m_strSession.data());
		send(m_pcBuf, n);
	}
		break;
	default:
		break;
	}
	return true;
}

bool RtspSession::handleReq_Play() {
	if (m_uiTrackCnt == 0) {
		//还没有Describe
		return false;
	}
	if (m_parser["Session"] != m_strSession) {
		send_SessionNotFound();
		return false;
	}

	if(m_pRtpReader){
		weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
		m_pRtpReader->setReadCB([weakSelf](const RtpPacket::Ptr &pack) {
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->async([weakSelf,pack](){
				auto strongSelf = weakSelf.lock();
				if(!strongSelf) {
					return;
				}
				strongSelf->sendRtpPacket(*pack);
			});

		});
	}

	auto pMediaSrc = m_pMediaSrc.lock();
	uint32_t iStamp = 0;
	if(pMediaSrc){
        auto strRange = m_parser["Range"];
		if (strRange.size() && !m_bFirstPlay) {
			auto strStart = FindField(strRange.data(), "npt=", "-");
			if (strStart == "now") {
				strStart = "0";
			}
			auto iStartTime = atof(strStart.data());
			InfoL << "rtsp seekTo:" << iStartTime;
			pMediaSrc->seekTo(iStartTime * 1000);
            iStamp = pMediaSrc->getStamp();
		}else if(pMediaSrc->getRing()->readerCount() == 1){
			//第一个消费者
			pMediaSrc->seekTo(0);
			iStamp = 0;
        }else{
            iStamp = pMediaSrc->getStamp();
        }
		for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
			auto &track = m_aTrackInfo[i];
			track.ssrc = pMediaSrc->getSsrc(track.trackId);
			track.seq = pMediaSrc->getSeqence(track.trackId);
			track.timeStamp = pMediaSrc->getTimestamp(track.trackId);
		}
	}
	m_bFirstPlay = false;
	int iLen = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Session: %s\r\n"
			"Range: npt=%.2f-\r\n"
			"RTP-Info: ", m_iCseq, g_serverName.data(), RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data(), m_strSession.data(),iStamp/1000.0);

	for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
		auto &track = m_aTrackInfo[i];
		if (track.inited == false) {
			//还有track没有setup
			return false;
		}
		iLen += sprintf(m_pcBuf + iLen, "url=%s/%s%d;seq=%d;rtptime=%u,",
                        m_strUrl.data(), track.trackStyle.data(),
                        track.trackId, track.seq,track.timeStamp);
	}
	iLen -= 1;
	(m_pcBuf)[iLen] = '\0';
	iLen += sprintf(m_pcBuf + iLen, "\r\n\r\n");
	send(m_pcBuf, iLen);
	NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastRtspSessionPlay, m_strApp.data(),m_strStream.data());
	return true;
}

bool RtspSession::handleReq_Pause() {
	if (m_parser["Session"] != m_strSession) {
		send_SessionNotFound();
		return false;
	}
	int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Session: %s\r\n\r\n", m_iCseq, g_serverName.data(), RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data(), m_strSession.data());
	send(m_pcBuf, n);
	if(m_pRtpReader){
		m_pRtpReader->setReadCB(nullptr);
	}
	return true;

}

bool RtspSession::handleReq_Teardown() {
	int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Session: %s\r\n\r\n", m_iCseq, g_serverName.data(), RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data(), m_strSession.data());

	send(m_pcBuf, n);
	TraceL << "播放器断开连接!";
	return false;
}

bool RtspSession::handleReq_Get() {
	m_strSessionCookie = m_parser["x-sessioncookie"];
	int n = sprintf(m_pcBuf, "HTTP/1.0 200 OK\r\n"
			"%s"
			"Connection: close\r\n"
			"Cache-Control: no-store\r\n"
			"Pragma: no-cache\r\n"
			"Content-Type: application/x-rtsp-tunnelled\r\n\r\n",
			dateHeader().data());
//注册GET
	lock_guard<recursive_mutex> lock(g_mtxGetter);
	g_mapGetter[m_strSessionCookie] = dynamic_pointer_cast<RtspSession>(shared_from_this());
	send(m_pcBuf, n);
	return true;

}

bool RtspSession::handleReq_Post() {
	lock_guard<recursive_mutex> lock(g_mtxGetter);
	string sessioncookie = m_parser["x-sessioncookie"];
//Poster 找到 Getter
	auto it = g_mapGetter.find(sessioncookie);
	if (it == g_mapGetter.end()) {
		return false;
	}
	m_bBase64need = true;
//Poster 找到Getter的SOCK
	auto strongSession = it->second.lock();
	g_mapGetter.erase(sessioncookie);
	if (!strongSession) {
		send_SessionNotFound();
		return false;
	}
	initSender(strongSession);
	return true;
}

bool RtspSession::handleReq_SET_PARAMETER() {
	//TraceL<<endl;
	int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Session: %s\r\n\r\n", m_iCseq, g_serverName.data(), RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data(), m_strSession.data());
	send(m_pcBuf, n);
	return true;
}

inline void RtspSession::send_NotAcceptable() {
	int n = sprintf(m_pcBuf, "RTSP/1.0 406 Not Acceptable\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Connection: Close\r\n\r\n", m_iCseq, g_serverName.data(), RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	send(m_pcBuf, n);

}

inline bool RtspSession::findStream() {
    
    string strHost = FindField(m_strUrl.data(), "://", "/");
    m_strApp = FindField(m_strUrl.data(), (strHost + "/").data(), "/");
    m_strStream = FindField(m_strUrl.data(), (strHost + "/" + m_strApp + "/").data(), NULL);
    
	auto iPos = m_strStream.find('?');
	if(iPos != string::npos ){
		m_strStream.erase(iPos);
	}
	RtspMediaSource::Ptr pMediaSrc = RtspMediaSource::find(m_strApp,m_strStream);
	if (!pMediaSrc) {
		WarnL << "No such stream:" << m_strApp << " " << m_strStream;
		return false;
	}
	m_strSdp = pMediaSrc->getSdp();
	m_pWeakRing = pMediaSrc->getRing();

	m_uiTrackCnt = parserSDP(m_strSdp, m_aTrackInfo);
	if (m_uiTrackCnt == 0 || m_uiTrackCnt > 2) {
		return false;
	}
	m_strSession = makeRandStr(12);
	m_pMediaSrc = pMediaSrc;

	for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
		auto &track = m_aTrackInfo[i];
		track.ssrc = pMediaSrc->getSsrc(track.trackId);
		track.seq = pMediaSrc->getSeqence(track.trackId);
		track.timeStamp = pMediaSrc->getTimestamp(track.trackId);
	}

	return true;
}

inline void RtspSession::sendRtpPacket(const RtpPacket& pkt) {
	//InfoL<<(int)pkt.Interleaved;
	switch (m_rtpType) {
	case PlayerBase::RTP_TCP: {
		send((char *) pkt.payload, pkt.length);
#ifdef RTSP_SEND_RTCP
		int iTrackIndex = getTrackIndexByTrackId(pkt.interleaved / 2);
		RtcpCounter &counter = m_aRtcpCnt[iTrackIndex];
		counter.pktCnt += 1;
		counter.octCount += (pkt.length - 12);
		auto &m_ticker = m_aRtcpTicker[iTrackIndex];
		if (m_ticker.elapsedTime() > 5 * 1000) {
			//send rtcp every 5 second
			m_ticker.resetTime();
			counter.timeStamp = pkt.timeStamp;
			sendRTCP();
		}
#endif
	}
		break;
	case PlayerBase::RTP_UDP: {
		int iTrackIndex = getTrackIndexByTrackId(pkt.interleaved / 2);
		auto pSock = m_apUdpSock[iTrackIndex].lock();
		if (!pSock) {
			shutdown();
			return;
		}
		auto peerAddr = m_apPeerUdpAddr[iTrackIndex];
		if (!peerAddr) {
			return;
		}
		pSock->sendTo((char *) pkt.payload + 4, pkt.length - 4, peerAddr.get());
	}
		break;
	default:
		break;
	}
}

inline void RtspSession::onRcvPeerUdpData(int iTrackIdx, const Socket::Buffer::Ptr &pBuf, const struct sockaddr& addr) {
	m_apPeerUdpAddr[iTrackIdx].reset(new struct sockaddr(addr));
	m_abGotPeerUdp[iTrackIdx] = true;
	bool bGotAllPeerUdp = true;
	for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
		if (!m_abGotPeerUdp[i]) {
			bGotAllPeerUdp = false;
			break;
		}
	}
	if (bGotAllPeerUdp) {
		if (m_bListenPeerUdpPort) {
			UDPServer::Instance().stopListenPeer(getPeerIp().data(), this);
			m_bListenPeerUdpPort = false;
			InfoL << "获取到客户端端口";
		}
	}
}


inline void RtspSession::tryGetPeerUdpPort() {
	if(SockUtil::in_same_lan(getLocalIp().data(),getPeerIp().data())){
		return;
	}
	m_bListenPeerUdpPort = true;
	weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
	UDPServer::Instance().listenPeer(getPeerIp().data(), this,
			[weakSelf](int iTrackIdx,const Socket::Buffer::Ptr &pBuf,struct sockaddr *pPeerAddr)->bool {
				auto strongSelf=weakSelf.lock();
				if(!strongSelf) {
					return false;
				}
				struct sockaddr addr=*pPeerAddr;
				strongSelf->async_first([weakSelf,pBuf,addr,iTrackIdx]() {
							auto strongSelf=weakSelf.lock();
							if(!strongSelf) {
								return;
							}
							strongSelf->onRcvPeerUdpData(iTrackIdx,pBuf,addr);
						});
				return true;
			});
}

inline void RtspSession::initSender(const std::shared_ptr<RtspSession>& session) {
	m_pSender = session->sock;
	weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
	session->m_onDestory = [weakSelf]() {
		auto strongSelf=weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		strongSelf->m_pSender->setOnErr([weakSelf](const SockException &err) {
			auto strongSelf=weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->safeShutdown();
		});
	};
	session->shutdown();
}

#ifdef RTSP_SEND_RTCP
inline void RtspSession::sendRTCP() {
	//DebugL;
	uint8_t aui8Rtcp[60] = {0};
	uint8_t *pui8Rtcp_SR = aui8Rtcp + 4, *pui8Rtcp_SDES = pui8Rtcp_SR + 28;
	for (uint8_t i = 0; i < m_uiTrackCnt; i++) {
		auto &track = m_aTrackInfo[i];
		auto &counter = m_aRtcpCnt[i];

		aui8Rtcp[0] = '$';
		aui8Rtcp[1] = track.trackId * 2 + 1;
		aui8Rtcp[2] = 56 / 256;
		aui8Rtcp[3] = 56 % 256;

		pui8Rtcp_SR[0] = 0x80;
		pui8Rtcp_SR[1] = 0xC8;
		pui8Rtcp_SR[2] = 0x00;
		pui8Rtcp_SR[3] = 0x06;

		uint32_t ssrc=htonl(track.ssrc);
		memcpy(&pui8Rtcp_SR[4], &ssrc, 4);

		uint64_t msw;
		uint64_t lsw;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		msw = tv.tv_sec + 0x83AA7E80; /* 0x83AA7E80 is the number of seconds from 1900 to 1970 */
		lsw = (uint32_t) ((double) tv.tv_usec * (double) (((uint64_t) 1) << 32) * 1.0e-6);

		msw = htonl(msw);
		memcpy(&pui8Rtcp_SR[8], &msw, 4);

		lsw = htonl(lsw);
		memcpy(&pui8Rtcp_SR[12], &lsw, 4);

		uint32_t rtpStamp = htonl(counter.timeStamp);
		memcpy(&pui8Rtcp_SR[16], &rtpStamp, 4);

		uint32_t pktCnt = htonl(counter.pktCnt);
		memcpy(&pui8Rtcp_SR[20], &pktCnt, 4);

		uint32_t octCount = htonl(counter.octCount);
		memcpy(&pui8Rtcp_SR[24], &octCount, 4);

		pui8Rtcp_SDES[0] = 0x81;
		pui8Rtcp_SDES[1] = 0xCA;
		pui8Rtcp_SDES[2] = 0x00;
		pui8Rtcp_SDES[3] = 0x06;

		memcpy(&pui8Rtcp_SDES[4], &ssrc, 4);

		pui8Rtcp_SDES[8] = 0x01;
		pui8Rtcp_SDES[9] = 0x0f;
		memcpy(&pui8Rtcp_SDES[10], "_ZL_RtspServer_", 15);
		pui8Rtcp_SDES[25] = 0x00;
		send((char *) aui8Rtcp, 60);
	}
}
#endif

}
/* namespace Session */
} /* namespace ZL */

