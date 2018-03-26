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
#include <set>
#include <cmath>
#include <stdarg.h>
#include <algorithm>

#include "Common/config.h"
#include "RtspPlayer.h"
#include "Device/base64.h"
#include "H264/SPSParser.h"
#include "Util/mini.h"
#include "Util/util.h"
#include "Network/sockutil.h"

using namespace ZL::Util;


namespace ZL {
namespace Rtsp {

#define POP_HEAD(trackidx) \
		auto it = m_amapRtpSort[trackidx].begin(); \
		_onRecvRTP(it->second, trackidx); \
		m_amapRtpSort[trackidx].erase(it);

const char RtspPlayer::kRtpType[] = "rtp_type";

RtspPlayer::RtspPlayer(void){
}
RtspPlayer::~RtspPlayer(void) {
    teardown();
    if (m_pucRtpBuf) {
        delete[] m_pucRtpBuf;
        m_pucRtpBuf = nullptr;
    }
    DebugL<<endl;
}
void RtspPlayer::teardown(){
	if (alive()) {
		write("TEARDOWN %s RTSP/1.0\r\n"
				"CSeq: %d\r\n"
                "Authorization: Basic %s\r\n"
                "Session: %s\r\n\r\n",
                m_strContentBase.c_str(), m_uiCseq++,
                m_strAuthorization.c_str(),
				m_strSession.c_str());

		m_uiTrackCnt = 0;
		m_onHandshake = nullptr;
		m_uiRtpBufLen = 0;
		m_strSession.clear();
		m_uiCseq = 1;
		m_strAuthorization.clear();
		m_strContentBase.clear();
		CLEAR_ARR(m_apUdpSock);
		CLEAR_ARR(m_aui16LastSeq)
		CLEAR_ARR(m_aui16FirstSeq)
		CLEAR_ARR(m_aui32SsrcErrorCnt)
		CLEAR_ARR(m_aui64RtpRecv)
		CLEAR_ARR(m_aui64SeqOkCnt)
		CLEAR_ARR(m_abSortStarted)
		CLEAR_ARR(m_aui64RtpRecv)
		CLEAR_ARR(m_aui16NowSeq)
		m_amapRtpSort[0].clear();
		m_amapRtpSort[1].clear();

		m_pBeatTimer.reset();
		m_pPlayTimer.reset();
		m_pRtpTimer.reset();
		m_fSeekTo = 0;
		CLEAR_ARR(m_adFistStamp);
		CLEAR_ARR(m_adNowStamp);
		shutdown();
	}
}

void RtspPlayer::play(const char* strUrl){
	auto userAndPwd = FindField(strUrl,"://","@");
	eRtpType eType = (eRtpType)(int)(*this)[kRtpType];
	if(userAndPwd.empty()){
		play(strUrl,nullptr,nullptr,eType);
		return;
	}
	auto suffix = FindField(strUrl,"@",nullptr);
	auto url = StrPrinter << "rtsp://" << suffix << endl;
	if(userAndPwd.find(":") == string::npos){
		play(url.data(),userAndPwd.data(),nullptr,eType);
		return;
	}
	auto user = FindField(userAndPwd.data(),nullptr,":");
	auto pwd = FindField(userAndPwd.data(),":",nullptr);
	play(url.data(),user.data(),pwd.data(),eType);
}
//播放，指定是否走rtp over tcp
void RtspPlayer::play(const char* strUrl, const char *strUser, const char *strPwd,  eRtpType eType ) {
	DebugL   << strUrl << " "
			<< (strUser ? strUser : "null") << " "
			<< (strPwd ? strPwd:"null") << " "
			<< eType;
	teardown();
    if(strUser){
        char _authorization[30];
        string tmp = StrPrinter << strUser << ":" << (strPwd ? strPwd : "") << endl;
        av_base64_encode(_authorization, sizeof(_authorization), (const unsigned char *) tmp.c_str(), tmp.size());
        m_strAuthorization = _authorization;
    }
	m_eType = eType;
	if (m_eType == RTP_TCP && !m_pucRtpBuf) {
        GET_CONFIG_AND_REGISTER(uint32_t,rtpSize,Config::Rtp::kUdpBufSize);
        m_pucRtpBuf = new uint8_t[rtpSize];
	}
	auto ip = FindField(strUrl, "://", "/");
	if (!ip.size()) {
		ip = FindField(strUrl, "://", NULL);
	}
	auto port = atoi(FindField(ip.c_str(), ":", NULL).c_str());
	if (port <= 0) {
		//rtsp 默认端口554
		port = 554;
	} else {
		//服务器域名
		ip = FindField(ip.c_str(), NULL, ":");
	}

	m_strUrl = strUrl;
	startConnect(ip.data(), port);
}
void RtspPlayer::onConnect(const SockException &err){
	if(err.getErrCode()!=Err_success) {
		_onPlayResult(err);
		teardown();
		return;
	}
	//发送DESCRIBE命令后处理函数:HandleResDESCRIBE
	m_onHandshake = std::bind(&RtspPlayer::HandleResDESCRIBE,this, placeholders::_1);
	write("DESCRIBE %s RTSP/1.0\r\n"
		  "CSeq: %d\r\n"
		  "Accept: application/sdp\r\n"
		  "Authorization: Basic %s\r\n\r\n",
		  m_strUrl.c_str(), m_uiCseq++,
		  m_strAuthorization.c_str());

	weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
	m_pPlayTimer.reset( new Timer(10,  [weakSelf]() {
		auto strongSelf=weakSelf.lock();
		if(!strongSelf) {
			return false;
		}
		strongSelf->_onPlayResult(SockException(Err_timeout,"play rtsp timeout"));
		strongSelf->teardown();
		return false;
	}));
}

void RtspPlayer::onRecv(const Buffer::Ptr& pBuf) {
	const char *buf = pBuf->data();
	int size = pBuf->size();
	if (m_onHandshake) {
        int offset = 0;
        while(offset < size - 4){
            char *pos = (char *)memchr(buf + offset, 'R', size - offset);
            if(pos == NULL){
                break;
            }
            if(memcmp(pos, "RTSP", 4) == 0){
                try {
                    pos += onProcess(pos);
                } catch (std::exception &err) {
                    SockException ex(Err_other, err.what());
                    _onPlayResult(ex);
                    _onShutdown(ex);
                    teardown();
                    return;
                }
            }else{
                pos += 1;
            }
            offset = pos - buf;
        }
	}

	if (m_eType == RTP_TCP && m_pucRtpBuf) {
		//RTP data
		while (size > 0) {
            GET_CONFIG_AND_REGISTER(uint32_t,rtpSize,Config::Rtp::kUdpBufSize);
            int added = rtpSize - m_uiRtpBufLen;
			added = (added > size ? size : added);
			memcpy(m_pucRtpBuf + m_uiRtpBufLen, buf, added);
			m_uiRtpBufLen += added;
			size -= added;
			buf += added;
			splitRtp(m_pucRtpBuf, m_uiRtpBufLen);
		}
	}
}
void RtspPlayer::onErr(const SockException &ex) {
	_onShutdown (ex);
}
inline void RtspPlayer::HandleResDESCRIBE(const Parser& parser) {
	//发送DESCRIBE命令后的回复
	if (parser.Url() != "200") {
		throw std::runtime_error(
		StrPrinter << "DESCRIBE:" << parser.Url() << " " << parser.Tail() << endl);
	}
	auto strSdp = parser.Content();
	m_strContentBase = parser["Content-Base"];

    if(m_strContentBase.empty()){
        m_strContentBase = m_strUrl;
    }
    if (m_strContentBase[m_strContentBase.length() - 1] == '/') {
        m_strContentBase.pop_back();
    }

	auto iLen = atoi(parser["Content-Length"].data());
	if(iLen > 0){
		strSdp.erase(iLen);
	}

	//解析sdp
	m_uiTrackCnt = parserSDP(strSdp, m_aTrackInfo);
    for (unsigned int i=0; i<m_uiTrackCnt; i++) {
    	m_aTrackInfo[i].ssrc=0;
        m_aui32SsrcErrorCnt[i]=0;
    }
	if (!m_uiTrackCnt) {
		throw std::runtime_error("解析SDP失败");
	}
	if (!onCheckSDP(strSdp, m_aTrackInfo, m_uiTrackCnt)) {
		throw std::runtime_error("onCheckSDP faied");
	}
	m_onHandshake = std::bind(&RtspPlayer::HandleResSETUP,this, placeholders::_1,0);
	sendSetup(0);
}
//发送SETUP命令
inline void RtspPlayer::sendSetup(unsigned int trackIndex) {
	//TCP
	int iLen = 0;
	char acRtspbuf[1024];
	auto &track = m_aTrackInfo[trackIndex];
	switch (m_eType) {
	case RTP_TCP: {
		iLen = sprintf(acRtspbuf, "SETUP %s/%s RTSP/1.0\r\n"
				"CSeq: %d\r\n"
				"Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n"
				"Authorization: Basic %s\r\n\r\n", m_strContentBase.c_str(),
				track.controlSuffix.c_str(), m_uiCseq++,
				track.trackId * 2, track.trackId * 2 + 1,
				m_strAuthorization.c_str());
	}
		break;
	case RTP_MULTICAST: {
		iLen = sprintf(acRtspbuf, "SETUP %s/%s RTSP/1.0\r\n"
				"CSeq: %d\r\n"
				"Transport: RTP/AVP;multicast\r\n"
				"Authorization: Basic %s\r\n\r\n", m_strContentBase.c_str(),
				track.controlSuffix.c_str(), m_uiCseq++,
				m_strAuthorization.c_str());
	}
		break;
	case RTP_UDP: {
		m_apUdpSock[trackIndex].reset(new Socket());
		if (!m_apUdpSock[trackIndex]->bindUdpSock(0, get_local_ip().data())) {
			m_apUdpSock[trackIndex].reset();
			throw std::runtime_error("open udp sock err");
		}
		int port = m_apUdpSock[trackIndex]->get_local_port();
		iLen = sprintf(acRtspbuf, "SETUP %s/%s RTSP/1.0\r\n"
				"CSeq: %d\r\n"
				"Transport: RTP/AVP;unicast;client_port=%d-%d\r\n"
				"Authorization: Basic %s\r\n\r\n", m_strContentBase.c_str(),
				track.controlSuffix.c_str(), m_uiCseq++, port,
				port + 1, m_strAuthorization.c_str());
	}
		break;
            
    default: break;
	}

	if (m_strSession.size() != 0) {
		iLen += sprintf(acRtspbuf + iLen - 2, "Session: %s\r\n\r\n", m_strSession.c_str()) - 2;
	}
	write(acRtspbuf);
}

void RtspPlayer::HandleResSETUP(const Parser& parser, unsigned int uiTrackIndex) {
	if (parser.Url() != "200") {
		throw std::runtime_error(
		StrPrinter << "SETUP:" << parser.Url() << " " << parser.Tail() << endl);
	}
	if (uiTrackIndex == 0) {
		m_strSession = parser["Session"];
        m_strSession.append(";");
        m_strSession = FindField(m_strSession.data(), nullptr, ";");
	}

	auto strTransport = parser["Transport"];
	if(strTransport.find("TCP") != string::npos){
		m_eType = RTP_TCP;
	}else if(strTransport.find("multicast") != string::npos){
		m_eType = RTP_MULTICAST;
	}else{
		m_eType = RTP_UDP;
	}

	if(m_eType == RTP_TCP)  {
		string interleaved = FindField( FindField((strTransport + ";").c_str(), "interleaved=", ";").c_str(), NULL, "-");
		m_aTrackInfo[uiTrackIndex].interleaved = atoi(interleaved.c_str());
	}else{
		const char *strPos = (m_eType == RTP_MULTICAST ? "port=" : "server_port=") ;
		auto port_str = FindField((strTransport + ";").c_str(), strPos, ";");
		uint16_t port = atoi(FindField(port_str.c_str(), NULL, "-").c_str());
        auto &pUdpSockRef = m_apUdpSock[uiTrackIndex];
        if(!pUdpSockRef){
            pUdpSockRef.reset(new Socket());
        }
        
		if (m_eType == RTP_MULTICAST) {
			auto multiAddr = FindField((strTransport + ";").c_str(), "destination=", ";");
			if (!pUdpSockRef->bindUdpSock(port, "0.0.0.0")) {
				pUdpSockRef.reset();
				throw std::runtime_error("open udp sock err");
			}
			auto fd = pUdpSockRef->rawFD();
			if (-1 == SockUtil::joinMultiAddrFilter(fd, multiAddr.data(), get_peer_ip().data(),get_local_ip().data())) {
				SockUtil::joinMultiAddr(fd, multiAddr.data(),get_local_ip().data());
			}
		} else {
			struct sockaddr_in rtpto;
			rtpto.sin_port = ntohs(port);
			rtpto.sin_family = AF_INET;
			rtpto.sin_addr.s_addr = inet_addr(get_peer_ip().c_str());
			pUdpSockRef->send("\xce\xfa\xed\xfe", 4,SOCKET_DEFAULE_FLAGS, (struct sockaddr *) &rtpto);
		}
	}

	if (uiTrackIndex < m_uiTrackCnt - 1) {
		//需要继续发送SETUP命令
		m_onHandshake = std::bind(&RtspPlayer::HandleResSETUP,this, placeholders::_1,uiTrackIndex + 1);
		sendSetup(uiTrackIndex + 1);
		return;
	}

	for (unsigned int i = 0; i < m_uiTrackCnt && m_eType != RTP_TCP; i++) {
		auto &pUdpSockRef = m_apUdpSock[i];
		if(!pUdpSockRef){
			continue;
		}
		auto srcIP = inet_addr(get_peer_ip().data());
		weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
		pUdpSockRef->setOnRead([srcIP,i,weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr) {
			auto strongSelf=weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			if(((struct sockaddr_in *)addr)->sin_addr.s_addr != srcIP) {
				WarnL << "收到请他地址的UDP数据:" << inet_ntoa(((struct sockaddr_in *) addr)->sin_addr);
				return;
			}
			strongSelf->HandleOneRtp(i,(unsigned char *)buf->data(),buf->size());
		});
	}
	/////////////////////////心跳/////////////////////////////////
	weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
	m_pBeatTimer.reset(new Timer(5, [weakSelf](){
		auto strongSelf = weakSelf.lock();
		if (!strongSelf){
			return false;
		}
		return strongSelf->sendOptions();
	}));
	pause(false);
}

bool RtspPlayer::sendOptions() {
	m_onHandshake = [](const Parser& parser){
		return true;
	};
	return -1 != write(	"OPTIONS %s RTSP/1.0\r\n"
						"CSeq: %d\r\n"
                        "Authorization: Basic %s\r\n"
						"Session: %s\r\n\r\n",
						m_strContentBase.c_str(), m_uiCseq++,
                        m_strAuthorization.c_str(),
                        m_strSession.c_str());
}
inline void RtspPlayer::sendPause(bool bPause,float fTime){
    //开启或暂停rtsp
    m_onHandshake = std::bind(&RtspPlayer::HandleResPAUSE,this, placeholders::_1,bPause);
    write("%s %s RTSP/1.0\r\n"
          "CSeq: %d\r\n"
          "Session: %s\r\n"
          "Authorization: Basic %s\r\n"
          "Range: npt=%.2f-\r\n\r\n", bPause ? "PAUSE" : "PLAY",
          m_strContentBase.c_str(), m_uiCseq++,
          m_strSession.c_str(),m_strAuthorization.c_str(),fTime);
    
    if(!bPause){
        //修改时间轴
        m_aNowStampTicker[0].resetTime();
        m_aNowStampTicker[1].resetTime();
        float iTimeInc = fTime - getProgressTime();
        for(unsigned int i = 0 ;i < m_uiTrackCnt ;i++){
            if (m_aTrackInfo[i].type == TrackVideo) {
                m_adFistStamp[i] = m_adNowStamp[i] + iTimeInc * 90000.0;
            }else if (m_aTrackInfo[i].type == TrackAudio){
                m_adFistStamp[i] = m_adNowStamp[i] + iTimeInc * getAudioSampleRate();
            }
            m_adNowStamp[i] = m_adFistStamp[i];
        }
        m_fSeekTo = fTime;
    }
}
void RtspPlayer::pause(bool bPause) {
    sendPause(bPause,getProgressTime());
}

void RtspPlayer::HandleResPAUSE(const Parser& parser, bool bPause) {
	if (parser.Url() != "200") {
		WarnL <<(bPause ? "Pause" : "Play") << " failed:" << parser.Url() << " " << parser.Tail() << endl;
		return;
	}
	if (!bPause) {
        //修正时间轴
        m_aNowStampTicker[0].resetTime();
        m_aNowStampTicker[1].resetTime();
        auto strRange = parser["Range"];
        if (strRange.size()) {
            auto strStart = FindField(strRange.data(), "npt=", "-");
            if (strStart == "now") {
                strStart = "0";
            }
            m_fSeekTo = atof(strStart.data());
            DebugL << "Range:" << m_fSeekTo << " " << strStart ;
        }
        auto strRtpInfo =  parser["RTP-Info"];
        if (strRtpInfo.size()) {
            strRtpInfo.append(",");
            vector<string> vec = split(strRtpInfo, ",");
            for(auto &strTrack : vec){
                strTrack.append(";");
                auto strControlSuffix = strTrack.substr(1 + strTrack.find_last_of('/'),strTrack.find(';') - strTrack.find_last_of('/') - 1);
                auto strRtpTime = FindField(strTrack.data(), "rtptime=", ";");
                auto iIdx = getTrackIndex(strControlSuffix);
                m_adFistStamp[iIdx] = atoll(strRtpTime.data());
                m_adNowStamp[iIdx] = m_adFistStamp[iIdx];
                DebugL << "rtptime:" << strControlSuffix <<" " << strRtpTime;
            }
        }
		_onPlayResult(SockException(Err_success, "rtsp play success"));
	} else {
		m_pRtpTimer.reset();
	}
}

inline int RtspPlayer::onProcess(const char* pcBuf) {
    auto strRtsp = FindField(pcBuf, "RTSP", "\r\n\r\n");
    if(strRtsp.empty()){
        return 4;
    }
    strRtsp = string("RTSP") + strRtsp + "\r\n\r\n";
	Parser parser;
	parser.Parse(strRtsp.data());
    int iLen = 0;
	if (parser.Url() == "200") {
		iLen = atoi(parser["Content-Length"].data());
		if (iLen) {
			string strContent(pcBuf + strRtsp.size(), iLen);
			parser.setContent(strContent);
		}
	}
	auto fun = m_onHandshake;
	m_onHandshake = nullptr;
    if(fun){
        fun(parser);
    }
	parser.Clear();
    return strRtsp.size() + iLen;
}

inline int RtspPlayer::write(const char *strFmt, ...) {
	va_list pArg;
	va_start(pArg, strFmt);
	char rtspbuf[1024];
	int n = vsprintf(rtspbuf, strFmt, pArg);
	va_end(pArg);
	return send(rtspbuf, n);
}

inline void RtspPlayer::splitRtp(unsigned char* pucRtp, unsigned int uiLen) {
	unsigned char* rtp_ptr = pucRtp;
	while (uiLen >= 4) {
		if (rtp_ptr[0] == '$') {
			//通道0
			uint8_t interleaved = rtp_ptr[1];
			uint16_t length = (rtp_ptr[2] << 8) | rtp_ptr[3];
			if (length > 1600) {
				//没有大于MTU的包
				//WarnL << "没有大于MTU的包:" << length;
				rtp_ptr += 1;
				uiLen -= 1;
				continue;
			}
			if ((unsigned int) length + 4 + 4 > uiLen) {
				//buf 太小,还没到该RTP包的结尾
				break;
			}
			auto nextPkt = rtp_ptr + length + 4;
			if (*nextPkt != '$' && memcmp(nextPkt,"RTSP",4)!=0 ) {
				//没有找到该包的尾部
				//WarnL << "没有找到该包的尾部";
				rtp_ptr += 1;
				uiLen -= 1;
				continue;
			}
			int trackIdx = -1;
			if(interleaved %2 ==0){
				trackIdx = getTrackIndex(interleaved/2);
			}
			if (trackIdx != -1) {
				HandleOneRtp(trackIdx, rtp_ptr + 4, length);
			}
			rtp_ptr += (length + 4);
			uiLen -= (length + 4);
			continue;
		}
		unsigned char *pos = (unsigned char *) memchr(rtp_ptr + 1, '$', uiLen - 1);
		if (pos == NULL) {
			//缓存里面没有任何RTP包
			//WarnL << "缓存里面没有任何RTP包";
			uiLen = 0;
			break;
		}
		//有RTP包起始头
		uiLen -= (pos - rtp_ptr);
		rtp_ptr = pos;
	}
	m_uiRtpBufLen = uiLen;
	if (rtp_ptr != pucRtp) {
		memmove(pucRtp, rtp_ptr, uiLen);
	}
}

inline bool RtspPlayer::HandleOneRtp(int iTrackidx, unsigned char *pucData, unsigned int uiLen) {
	auto &track = m_aTrackInfo[iTrackidx];
	auto pt_ptr=m_pktPool.obtain();
	auto &rtppt=*pt_ptr;
	rtppt.interleaved = track.trackId * 2;
	rtppt.length = uiLen + 4;

	rtppt.mark = pucData[1] >> 7;
	rtppt.PT = pucData[1] & 0x7F;
	//序列号
	memcpy(&rtppt.sequence,pucData+2,2);//内存对齐
	rtppt.sequence = ntohs(rtppt.sequence);
	//时间戳
    memcpy(&rtppt.timeStamp, pucData+4, 4);//内存对齐
    rtppt.timeStamp = ntohl(rtppt.timeStamp);
	//ssrc
	memcpy(&rtppt.ssrc,pucData+8,4);//内存对齐
	rtppt.ssrc = ntohl(rtppt.ssrc);
	rtppt.type = track.type;
	if (track.ssrc == 0) {
		track.ssrc = rtppt.ssrc;
		//保存SSRC
	} else if (track.ssrc != rtppt.ssrc) {
		//ssrc错误
		WarnL << "ssrc错误";
		if (m_aui32SsrcErrorCnt[iTrackidx]++ > 10) {
			track.ssrc = rtppt.ssrc;
			WarnL << "ssrc更换!";
		}
		return false;
	}
	m_aui32SsrcErrorCnt[iTrackidx] = 0;

	rtppt.payload[0] = '$';
	rtppt.payload[1] = rtppt.interleaved;
	rtppt.payload[2] = (uiLen & 0xFF00) >> 8;
	rtppt.payload[3] = (uiLen & 0x00FF);
	memcpy(rtppt.payload + 4, pucData, uiLen);

	/////////////////////////////////RTP排序逻辑///////////////////////////////////
	if(rtppt.sequence != (uint16_t)(m_aui16LastSeq[iTrackidx] + 1) && m_aui16LastSeq[iTrackidx] != 0){
		//包乱序或丢包
		m_aui64SeqOkCnt[iTrackidx] = 0;
		m_abSortStarted[iTrackidx] = true;
		//WarnL << "包乱序或丢包:" << trackidx <<" " << rtppt.sequence << " " << m_aui16LastSeq[trackidx];
	}else{
		//正确序列的包
		m_aui64SeqOkCnt[iTrackidx]++;
	}
	m_aui16LastSeq[iTrackidx] = rtppt.sequence;

	//开始排序缓存
	if (m_abSortStarted[iTrackidx]) {
		m_amapRtpSort[iTrackidx].emplace(rtppt.sequence, pt_ptr);
        GET_CONFIG_AND_REGISTER(uint32_t,clearCount,Config::Rtp::kClearCount);
        GET_CONFIG_AND_REGISTER(uint32_t,maxRtpCount,Config::Rtp::kMaxRtpCount);
		if (m_aui64SeqOkCnt[iTrackidx] >= clearCount) {
			//网络环境改善，需要清空排序缓存
			m_aui64SeqOkCnt[iTrackidx] = 0;
			m_abSortStarted[iTrackidx] = false;
			while (m_amapRtpSort[iTrackidx].size()) {
				POP_HEAD(iTrackidx)
			}
		} else if (m_amapRtpSort[iTrackidx].size() >= maxRtpCount) {
			//排序缓存溢出
			POP_HEAD(iTrackidx)
		}
	}else{
		//正确序列
		_onRecvRTP(pt_ptr, iTrackidx);
	}
	//////////////////////////////////////////////////////////////////////////////////
	return true;
}
inline void RtspPlayer::_onRecvRTP(const RtpPacket::Ptr &rtppt, int trackidx){
	//统计丢包率
	if (m_aui16FirstSeq[trackidx] == 0 || rtppt->sequence < m_aui16FirstSeq[trackidx]) {
		m_aui16FirstSeq[trackidx] = rtppt->sequence;
		m_aui64RtpRecv[trackidx] = 0;
	}
	m_aui64RtpRecv[trackidx] ++;
	m_aui16NowSeq[trackidx] = rtppt->sequence;
    
    if (m_aNowStampTicker[trackidx].elapsedTime() > 500) {
        m_adNowStamp[trackidx] = rtppt->timeStamp;
    }
    
	_onRecvRTP(rtppt,m_aTrackInfo[trackidx]);
}
float RtspPlayer::getRtpLossRate(int iTrackId) const{
	int iTrackIdx = getTrackIndex(iTrackId);
	if(iTrackIdx == -1){
		uint64_t totalRecv = 0;
		uint64_t totalSend = 0;
		for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
			totalRecv += m_aui64RtpRecv[i];
			totalSend += (m_aui16NowSeq[i] - m_aui16FirstSeq[i] + 1);
		}
		if(totalSend == 0){
			return 0;
		}
		return 1.0 - (double)totalRecv / totalSend;
	}


	if(m_aui16NowSeq[iTrackIdx] - m_aui16FirstSeq[iTrackIdx] + 1 == 0){
		return 0;
	}
	return 1.0 - (double)m_aui64RtpRecv[iTrackIdx] / (m_aui16NowSeq[iTrackIdx] - m_aui16FirstSeq[iTrackIdx] + 1);
}

float RtspPlayer::getProgressTime() const{
    double iTime[2] = {0,0};
    for(unsigned int i = 0 ;i < m_uiTrackCnt ;i++){
        if (m_aTrackInfo[i].type == TrackVideo) {
            iTime[i] = (m_adNowStamp[i] - m_adFistStamp[i]) / 90000.0;
        }else if (m_aTrackInfo[i].type == TrackAudio){
            iTime[i] = (m_adNowStamp[i] - m_adFistStamp[i]) / getAudioSampleRate();
        }
    }
    return m_fSeekTo + MAX(iTime[0],iTime[1]);
}
void RtspPlayer::seekToTime(float fTime) {
    sendPause(false,fTime);
}
    
} /* namespace Rtsp */
} /* namespace ZL */


