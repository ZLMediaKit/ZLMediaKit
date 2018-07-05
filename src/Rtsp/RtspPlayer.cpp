/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 * Copyright (c) 2018 huohuo <913481084@qq.com>
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
#include "Util/MD5.h"
#include "Util/mini.h"
#include "Util/util.h"
#include "Network/sockutil.h"
#include "Device/base64.h"

using namespace ZL::Util;


namespace ZL {
namespace Rtsp {

#define POP_HEAD(trackidx) \
		auto it = m_amapRtpSort[trackidx].begin(); \
		onRecvRTP_l(it->second, trackidx); \
		m_amapRtpSort[trackidx].erase(it);

const char kRtspMd5Nonce[] = "rtsp_md5_nonce";
const char kRtspRealm[] = "rtsp_realm";

RtspPlayer::RtspPlayer(void){
	m_pktPool.setSize(64);
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
		sendRtspRequest("TEARDOWN" ,m_strContentBase);
		shutdown();
	}

    erase(kRtspMd5Nonce);
    erase(kRtspRealm);
    m_uiTrackCnt = 0;
    m_onHandshake = nullptr;
    m_uiRtpBufLen = 0;
    m_strSession.clear();
    m_uiCseq = 1;
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
        (*this)[kRtspUser] = strUser;
    }
    if(strPwd){
        (*this)[kRtspPwd] = strPwd;
		(*this)[kRtspPwdIsMD5] = false;
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
	if(!(*this)[PlayerBase::kNetAdapter].empty()){
		setNetAdapter((*this)[PlayerBase::kNetAdapter]);
	}
	startConnect(ip.data(), port);
}
void RtspPlayer::onConnect(const SockException &err){
	if(err.getErrCode()!=Err_success) {
		onPlayResult_l(err);
		teardown();
		return;
	}

	sendDescribe();

	weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
	m_pPlayTimer.reset( new Timer(10,  [weakSelf]() {
		auto strongSelf=weakSelf.lock();
		if(!strongSelf) {
			return false;
		}
		strongSelf->onPlayResult_l(SockException(Err_timeout,"play rtsp timeout"));
		strongSelf->teardown();
		return false;
	}));
}

void RtspPlayer::onRecv(const Buffer::Ptr& pBuf) {
	const char *buf = pBuf->data();
	int size = pBuf->size();
	if (m_onHandshake) {
	    //rtsp回复
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
                    onPlayResult_l(ex);
                    onShutdown_l(ex);
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
	onShutdown_l (ex);
}
// from live555
bool RtspPlayer::handleAuthenticationFailure(const string &paramsStr) {
    if(!(*this)[kRtspRealm].empty()){
        //已经认证过了
        return false;
    }

    char realm[paramsStr.size()];
    char nonce[paramsStr.size()];
    char stale[paramsStr.size()];

    if (sscanf(paramsStr.data(), "Digest realm=\"%[^\"]\", nonce=\"%[^\"]\", stale=%[a-zA-Z]", realm, nonce, stale) == 3) {
        (*this)[kRtspRealm] = realm;
        (*this)[kRtspMd5Nonce] = nonce;
        return true;
    }
    if (sscanf(paramsStr.data(), "Digest realm=\"%[^\"]\", nonce=\"%[^\"]\"", realm, nonce) == 2) {
        (*this)[kRtspRealm] = realm;
        (*this)[kRtspMd5Nonce] = nonce;
        return true;
    }
    if (sscanf(paramsStr.data(), "Basic realm=\"%[^\"]\"", realm) == 1) {
        (*this)[kRtspRealm] = realm;
        return true;
    }
    return false;
}
void RtspPlayer::handleResDESCRIBE(const Parser& parser) {
	string authInfo = parser["WWW-Authenticate"];
	//发送DESCRIBE命令后的回复
	if ((parser.Url() == "401") && handleAuthenticationFailure(authInfo)) {
		sendDescribe();
		return;
	}
	if (parser.Url() != "200") {
		throw std::runtime_error(
		StrPrinter << "DESCRIBE:" << parser.Url() << " " << parser.Tail() << endl);
	}
	auto strSdp = parser.Content();
	m_strContentBase = parser["Content-Base"];

    if(m_strContentBase.empty()){
        m_strContentBase = m_strUrl;
    }
    if (m_strContentBase.back() == '/') {
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
	sendSetup(0);
}
//发送SETUP命令
bool RtspPlayer::sendSetup(unsigned int trackIndex) {
    m_onHandshake = std::bind(&RtspPlayer::handleResSETUP,this, placeholders::_1,trackIndex);

    auto &track = m_aTrackInfo[trackIndex];
	auto baseUrl = m_strContentBase + "/" + track.controlSuffix;
	switch (m_eType) {
		case RTP_TCP: {
			StrCaseMap header;
			header["Transport"] = StrPrinter << "RTP/AVP/TCP;unicast;interleaved=" << track.type * 2 << "-" << track.type * 2 + 1;
			return sendRtspRequest("SETUP",baseUrl,header);
		}
			break;
		case RTP_MULTICAST: {
			StrCaseMap header;
			header["Transport"] = "Transport: RTP/AVP;multicast";
			return sendRtspRequest("SETUP",baseUrl,header);
		}
			break;
		case RTP_UDP: {
			m_apUdpSock[trackIndex].reset(new Socket());
			if (!m_apUdpSock[trackIndex]->bindUdpSock(0, get_local_ip().data())) {
				m_apUdpSock[trackIndex].reset();
				throw std::runtime_error("open udp sock err");
			}
			int port = m_apUdpSock[trackIndex]->get_local_port();
			StrCaseMap header;
			header["Transport"] = StrPrinter << "RTP/AVP;unicast;client_port=" << port << "-" << port + 1;
			return sendRtspRequest("SETUP",baseUrl,header);
		}
			break;
		default:
			return false;
			break;
	}
}

void RtspPlayer::handleResSETUP(const Parser &parser, unsigned int uiTrackIndex) {
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
			strongSelf->handleOneRtp(i,(unsigned char *)buf->data(),buf->size());
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
	return sendRtspRequest("OPTIONS",m_strContentBase);
}

bool RtspPlayer::sendDescribe() {
	//发送DESCRIBE命令后处理函数:handleResDESCRIBE
	m_onHandshake = std::bind(&RtspPlayer::handleResDESCRIBE,this, placeholders::_1);
	StrCaseMap header;
	header["Accept"] = "application/sdp";
	return sendRtspRequest("DESCRIBE",m_strUrl,header);
}


bool RtspPlayer::sendPause(bool bPause,float fTime){
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

	//开启或暂停rtsp
	m_onHandshake = std::bind(&RtspPlayer::handleResPAUSE,this, placeholders::_1,bPause);

	StrCaseMap header;
	char buf[8];
	sprintf(buf,"%.2f",fTime);
	header["Range"] = StrPrinter << "npt=" << buf << "-";
	return sendRtspRequest(bPause ? "PAUSE" : "PLAY",m_strContentBase,header);
}
void RtspPlayer::pause(bool bPause) {
    sendPause(bPause,getProgressTime());
}

void RtspPlayer::handleResPAUSE(const Parser& parser, bool bPause) {
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
                auto strControlSuffix = strTrack.substr(1 + strTrack.rfind('/'),strTrack.find(';') - strTrack.rfind('/') - 1);
                auto strRtpTime = FindField(strTrack.data(), "rtptime=", ";");
                auto iIdx = getTrackIndexByControlSuffix(strControlSuffix);
                m_adFistStamp[iIdx] = atoll(strRtpTime.data());
                m_adNowStamp[iIdx] = m_adFistStamp[iIdx];
                DebugL << "rtptime:" << strControlSuffix <<" " << strRtpTime;
            }
        }
		onPlayResult_l(SockException(Err_success, "rtsp play success"));
	} else {
		m_pRtpTimer.reset();
	}
}

int RtspPlayer::onProcess(const char* pcBuf) {
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


void RtspPlayer::splitRtp(unsigned char* pucRtp, unsigned int uiLen) {
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
				trackIdx = getTrackIndexByInterleaved(interleaved);
			}
			if (trackIdx != -1) {
				handleOneRtp(trackIdx, rtp_ptr + 4, length);
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

bool RtspPlayer::handleOneRtp(int iTrackidx, unsigned char *pucData, unsigned int uiLen) {
	auto &track = m_aTrackInfo[iTrackidx];
	auto pt_ptr=m_pktPool.obtain();
	auto &rtppt=*pt_ptr;
	rtppt.interleaved = track.interleaved;
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
		onRecvRTP_l(pt_ptr, iTrackidx);
	}
	//////////////////////////////////////////////////////////////////////////////////
	return true;
}
void RtspPlayer::onRecvRTP_l(const RtpPacket::Ptr &rtppt, int trackidx){
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
    
	onRecvRTP_l(rtppt,m_aTrackInfo[trackidx]);
}
float RtspPlayer::getRtpLossRate(int iTrackType) const{
	int iTrackIdx = getTrackIndexByTrackType((TrackType)iTrackType);
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

bool RtspPlayer::sendRtspRequest(const string &cmd, const string &url,const StrCaseMap &header_const) {
	auto header = header_const;
	header.emplace("CSeq",StrPrinter << m_uiCseq++);
	if(!m_strSession.empty()){
		header.emplace("Session",m_strSession);
	}

	if(!(*this)[kRtspRealm].empty() && !(*this)[PlayerBase::kRtspUser].empty()){
		if(!(*this)[kRtspMd5Nonce].empty()){
			//MD5认证
			/*
			response计算方法如下：
			RTSP客户端应该使用username + password并计算response如下:
			(1)当password为MD5编码,则
				response = md5( password:nonce:md5(public_method:url)  );
			(2)当password为ANSI字符串,则
				response= md5( md5(username:realm:password):nonce:md5(public_method:url) );
			 */
			string encrypted_pwd = (*this)[PlayerBase::kRtspPwd];
			if(!(*this)[PlayerBase::kRtspPwdIsMD5].as<bool>()){
				encrypted_pwd = MD5((*this)[PlayerBase::kRtspUser]+ ":" + (*this)[kRtspRealm] + ":" + encrypted_pwd).hexdigest();
			}
			auto response = MD5( encrypted_pwd + ":" + (*this)[kRtspMd5Nonce]  + ":" + MD5(cmd + ":" + url).hexdigest()).hexdigest();
			_StrPrinter printer;
			printer << "Digest ";
			printer << "username=\"" << (*this)[PlayerBase::kRtspUser] << "\", ";
			printer << "realm=\"" << (*this)[kRtspRealm] << "\", ";
			printer << "nonce=\"" << (*this)[kRtspMd5Nonce] << "\", ";
			printer << "uri=\"" << url << "\", ";
			printer << "response=\"" << response << "\"";
			header.emplace("Authorization",printer);
		}else if(!(*this)[PlayerBase::kRtspPwdIsMD5].as<bool>()){
			//base64认证
			string authStr = StrPrinter << (*this)[PlayerBase::kRtspUser] << ":" << (*this)[PlayerBase::kRtspPwd];
			char authStrBase64[1024] = {0};
			av_base64_encode(authStrBase64,sizeof(authStrBase64),(uint8_t *)authStr.data(),authStr.size());
			header.emplace("Authorization",StrPrinter << "Basic " << authStrBase64 );
		}
	}

	_StrPrinter printer;
	printer << cmd << " " << url << " RTSP/1.0\r\n";
	for (auto &pr : header){
		printer << pr.first << ": " << pr.second << "\r\n";
	}
	return send(printer << "\r\n") > 0;
}


void RtspPlayer::onShutdown_l(const SockException &ex) {
	WarnL << ex.getErrCode() << " " << ex.what();
	m_pPlayTimer.reset();
	m_pRtpTimer.reset();
	m_pBeatTimer.reset();
	onShutdown(ex);
}
void RtspPlayer::onRecvRTP_l(const RtpPacket::Ptr &pRtppt, const RtspTrack &track) {
	m_rtpTicker.resetTime();
	onRecvRTP(pRtppt,track);
}
void RtspPlayer::onPlayResult_l(const SockException &ex) {
	WarnL << ex.getErrCode() << " " << ex.what();
	m_pPlayTimer.reset();
	m_pRtpTimer.reset();
	if (!ex) {
		m_rtpTicker.resetTime();
		weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
		m_pRtpTimer.reset( new Timer(5, [weakSelf]() {
			auto strongSelf=weakSelf.lock();
			if(!strongSelf) {
				return false;
			}
			if(strongSelf->m_rtpTicker.elapsedTime()>10000) {
				//recv rtp timeout!
				strongSelf->onShutdown_l(SockException(Err_timeout,"recv rtp timeout"));
				strongSelf->teardown();
				return false;
			}
			return true;
		}));
	}
	onPlayResult(ex);
}

int RtspPlayer::getTrackIndexByControlSuffix(const string &controlSuffix) const{
	for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
		if (m_aTrackInfo[i].controlSuffix == controlSuffix) {
			return i;
		}
	}
	return -1;
}
int RtspPlayer::getTrackIndexByInterleaved(int interleaved) const{
	for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
		if (m_aTrackInfo[i].interleaved == interleaved) {
			return i;
		}
	}
	return -1;
}

int RtspPlayer::getTrackIndexByTrackType(TrackType trackType) const {
	for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
		if (m_aTrackInfo[i].type == trackType) {
			return i;
		}
	}
	return -1;
}

} /* namespace Rtsp */
} /* namespace ZL */


