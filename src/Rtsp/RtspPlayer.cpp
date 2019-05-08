/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
#include <iomanip>

#include "Common/config.h"
#include "RtspPlayer.h"
#include "Util/MD5.h"
#include "Util/mini.h"
#include "Util/util.h"
#include "Util/base64.h"
#include "Network/sockutil.h"
using namespace toolkit;
using namespace mediakit::Client;

namespace mediakit {

RtspPlayer::RtspPlayer(const EventPoller::Ptr &poller) : TcpClient(poller){
	RtpReceiver::setPoolSize(64);
}
RtspPlayer::~RtspPlayer(void) {
    DebugL<<endl;
}
void RtspPlayer::teardown(){
	if (alive()) {
		sendRtspRequest("TEARDOWN" ,_strContentBase);
		shutdown();
	}

	_rtspMd5Nonce.clear();
	_rtspRealm.clear();

	_aTrackInfo.clear();
    _strSession.clear();
    _strContentBase.clear();
	RtpReceiver::clear();

    CLEAR_ARR(_apRtpSock);
    CLEAR_ARR(_apRtcpSock);
    CLEAR_ARR(_aui16FirstSeq)
    CLEAR_ARR(_aui64RtpRecv)
    CLEAR_ARR(_aui64RtpRecv)
    CLEAR_ARR(_aui16NowSeq)
	CLEAR_ARR(_aiFistStamp);
	CLEAR_ARR(_aiNowStamp);

    _pBeatTimer.reset();
    _pPlayTimer.reset();
    _pRtpTimer.reset();
    _iSeekTo = 0;
	_uiCseq = 1;
	_onHandshake = nullptr;
}

void RtspPlayer::play(const string &strUrl){
	auto userAndPwd = FindField(strUrl.data(),"://","@");
	Rtsp::eRtpType eType = (Rtsp::eRtpType)(int)(*this)[kRtpType];
	if(userAndPwd.empty()){
		play(strUrl,"","",eType);
		return;
	}
	auto suffix = FindField(strUrl.data(),"@",nullptr);
	auto url = StrPrinter << "rtsp://" << suffix << endl;
	if(userAndPwd.find(":") == string::npos){
		play(url,userAndPwd,"",eType);
		return;
	}
	auto user = FindField(userAndPwd.data(),nullptr,":");
	auto pwd = FindField(userAndPwd.data(),":",nullptr);
	play(url,user,pwd,eType);
}
//播放，指定是否走rtp over tcp
void RtspPlayer::play(const string &strUrl, const string &strUser, const string &strPwd,  Rtsp::eRtpType eType ) {
	DebugL   << strUrl << " "
			<< (strUser.size() ? strUser : "null") << " "
			<< (strPwd.size() ? strPwd:"null") << " "
			<< eType;
	teardown();

	if(strUser.size()){
        (*this)[kRtspUser] = strUser;
    }
    if(strPwd.size()){
        (*this)[kRtspPwd] = strPwd;
		(*this)[kRtspPwdIsMD5] = false;
    }

	_eType = eType;

	auto ip = FindField(strUrl.data(), "://", "/");
	if (!ip.size()) {
		ip = FindField(strUrl.data(), "://", NULL);
	}
	auto port = atoi(FindField(ip.data(), ":", NULL).data());
	if (port <= 0) {
		//rtsp 默认端口554
		port = 554;
	} else {
		//服务器域名
		ip = FindField(ip.data(), NULL, ":");
	}

	_strUrl = strUrl;

	weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
	float playTimeOutSec = (*this)[kTimeoutMS].as<int>() / 1000.0;
	_pPlayTimer.reset( new Timer(playTimeOutSec,  [weakSelf]() {
		auto strongSelf=weakSelf.lock();
		if(!strongSelf) {
			return false;
		}
		strongSelf->onPlayResult_l(SockException(Err_timeout,"play rtsp timeout"));
		return false;
	},getPoller()));

	if(!(*this)[kNetAdapter].empty()){
		setNetAdapter((*this)[kNetAdapter]);
	}
	startConnect(ip, port , playTimeOutSec);
}
void RtspPlayer::onConnect(const SockException &err){
	if(err.getErrCode()!=Err_success) {
		onPlayResult_l(err);
		return;
	}

	sendDescribe();
}

void RtspPlayer::onRecv(const Buffer::Ptr& pBuf) {
    input(pBuf->data(),pBuf->size());
}
void RtspPlayer::onErr(const SockException &ex) {
	onPlayResult_l(ex);
}
// from live555
bool RtspPlayer::handleAuthenticationFailure(const string &paramsStr) {
    if(!_rtspRealm.empty()){
        //已经认证过了
        return false;
    }

    char *realm = new char[paramsStr.size()];
    char *nonce = new char[paramsStr.size()];
    char *stale = new char[paramsStr.size()];
    onceToken token(nullptr,[&](){
        delete[] realm;
        delete[] nonce;
        delete[] stale;
    });

    if (sscanf(paramsStr.data(), "Digest realm=\"%[^\"]\", nonce=\"%[^\"]\", stale=%[a-zA-Z]", realm, nonce, stale) == 3) {
        _rtspRealm = (const char *)realm;
        _rtspMd5Nonce = (const char *)nonce;
        return true;
    }
    if (sscanf(paramsStr.data(), "Digest realm=\"%[^\"]\", nonce=\"%[^\"]\"", realm, nonce) == 2) {
        _rtspRealm = (const char *)realm;
        _rtspMd5Nonce = (const char *)nonce;
        return true;
    }
    if (sscanf(paramsStr.data(), "Basic realm=\"%[^\"]\"", realm) == 1) {
        _rtspRealm = (const char *)realm;
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
	if(parser.Url() == "302"){
		auto newUrl = parser["Location"];
		if(newUrl.empty()){
			throw std::runtime_error("未找到Location字段(跳转url)");
		}
		play(newUrl);
		return;
	}
	if (parser.Url() != "200") {
		throw std::runtime_error(
		StrPrinter << "DESCRIBE:" << parser.Url() << " " << parser.Tail() << endl);
	}
	_strContentBase = parser["Content-Base"];

    if(_strContentBase.empty()){
        _strContentBase = _strUrl;
    }
    if (_strContentBase.back() == '/') {
        _strContentBase.pop_back();
    }

	//解析sdp
	_sdpAttr.load(parser.Content());
	_aTrackInfo = _sdpAttr.getAvailableTrack();

	if (_aTrackInfo.empty()) {
		throw std::runtime_error("无有效的Sdp Track");
	}
	if (!onCheckSDP(parser.Content(), _sdpAttr)) {
		throw std::runtime_error("onCheckSDP faied");
	}

	sendSetup(0);
}
//发送SETUP命令
void RtspPlayer::sendSetup(unsigned int trackIndex) {
    _onHandshake = std::bind(&RtspPlayer::handleResSETUP,this, placeholders::_1,trackIndex);
    auto &track = _aTrackInfo[trackIndex];
	auto baseUrl = _strContentBase + "/" + track->_control_surffix;
	switch (_eType) {
		case Rtsp::RTP_TCP: {
			sendRtspRequest("SETUP",baseUrl,{"Transport",StrPrinter << "RTP/AVP/TCP;unicast;interleaved=" << track->_type * 2 << "-" << track->_type * 2 + 1});
		}
			break;
		case Rtsp::RTP_MULTICAST: {
			sendRtspRequest("SETUP",baseUrl,{"Transport","Transport: RTP/AVP;multicast"});
		}
			break;
		case Rtsp::RTP_UDP: {
			_apRtpSock[trackIndex].reset(new Socket());
			if (!_apRtpSock[trackIndex]->bindUdpSock(0, get_local_ip().data())) {
				_apRtpSock[trackIndex].reset();
				throw std::runtime_error("open rtp sock err");
			}
            _apRtcpSock[trackIndex].reset(new Socket());
            if (!_apRtcpSock[trackIndex]->bindUdpSock(0, get_local_ip().data())) {
                _apRtcpSock[trackIndex].reset();
                throw std::runtime_error("open rtcp sock err");
            }
			sendRtspRequest("SETUP",baseUrl,{"Transport",
                                    StrPrinter << "RTP/AVP;unicast;client_port="
                                    << _apRtpSock[trackIndex]->get_local_port() << "-"
                                    << _apRtcpSock[trackIndex]->get_local_port()});
		}
			break;
		default:
			break;
	}
}

void RtspPlayer::handleResSETUP(const Parser &parser, unsigned int uiTrackIndex) {
	if (parser.Url() != "200") {
		throw std::runtime_error(
		StrPrinter << "SETUP:" << parser.Url() << " " << parser.Tail() << endl);
	}
	if (uiTrackIndex == 0) {
		_strSession = parser["Session"];
        _strSession.append(";");
        _strSession = FindField(_strSession.data(), nullptr, ";");
	}

	auto strTransport = parser["Transport"];
	if(strTransport.find("TCP") != string::npos){
		_eType = Rtsp::RTP_TCP;
	}else if(strTransport.find("multicast") != string::npos){
		_eType = Rtsp::RTP_MULTICAST;
	}else{
		_eType = Rtsp::RTP_UDP;
	}

	RtspSplitter::enableRecvRtp(_eType == Rtsp::RTP_TCP);

	if(_eType == Rtsp::RTP_TCP)  {
		string interleaved = FindField( FindField((strTransport + ";").data(), "interleaved=", ";").data(), NULL, "-");
		_aTrackInfo[uiTrackIndex]->_interleaved = atoi(interleaved.data());
	}else{
		const char *strPos = (_eType == Rtsp::RTP_MULTICAST ? "port=" : "server_port=") ;
		auto port_str = FindField((strTransport + ";").data(), strPos, ";");
		uint16_t rtp_port = atoi(FindField(port_str.data(), NULL, "-").data());
        uint16_t rtcp_port = atoi(FindField(port_str.data(), "-",NULL).data());
        auto &pRtpSockRef = _apRtpSock[uiTrackIndex];
        auto &pRtcpSockRef = _apRtcpSock[uiTrackIndex];

		if (_eType == Rtsp::RTP_MULTICAST) {
		    //udp组播
			auto multiAddr = FindField((strTransport + ";").data(), "destination=", ";");
            pRtpSockRef.reset(new Socket());
			if (!pRtpSockRef->bindUdpSock(rtp_port, "0.0.0.0")) {
				pRtpSockRef.reset();
				throw std::runtime_error("open udp sock err");
			}
			auto fd = pRtpSockRef->rawFD();
			if (-1 == SockUtil::joinMultiAddrFilter(fd, multiAddr.data(), get_peer_ip().data(),get_local_ip().data())) {
				SockUtil::joinMultiAddr(fd, multiAddr.data(),get_local_ip().data());
			}
		} else {
		    //udp单播
			struct sockaddr_in rtpto;
			rtpto.sin_port = ntohs(rtp_port);
			rtpto.sin_family = AF_INET;
			rtpto.sin_addr.s_addr = inet_addr(get_peer_ip().data());
			pRtpSockRef->setSendPeerAddr((struct sockaddr *)&(rtpto));
			//发送rtp打洞包
			pRtpSockRef->send("\xce\xfa\xed\xfe", 4);

			//设置rtcp发送目标，为后续发送rtcp做准备
            rtpto.sin_port = ntohs(rtcp_port);
            rtpto.sin_family = AF_INET;
            rtpto.sin_addr.s_addr = inet_addr(get_peer_ip().data());
            pRtcpSockRef->setSendPeerAddr((struct sockaddr *)&(rtpto));
		}

        auto srcIP = inet_addr(get_peer_ip().data());
        weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
        //设置rtp over udp接收回调处理函数
        pRtpSockRef->setOnRead([srcIP, uiTrackIndex, weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr) {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            if (((struct sockaddr_in *) addr)->sin_addr.s_addr != srcIP) {
                WarnL << "收到其他地址的rtp数据:" << inet_ntoa(((struct sockaddr_in *) addr)->sin_addr);
                return;
            }
            strongSelf->handleOneRtp(uiTrackIndex, strongSelf->_aTrackInfo[uiTrackIndex], (unsigned char *) buf->data(), buf->size());
        });

        if(pRtcpSockRef) {
            //设置rtcp over udp接收回调处理函数
            pRtcpSockRef->setOnRead([srcIP, uiTrackIndex, weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                if (((struct sockaddr_in *) addr)->sin_addr.s_addr != srcIP) {
                    WarnL << "收到其他地址的rtcp数据:" << inet_ntoa(((struct sockaddr_in *) addr)->sin_addr);
                    return;
                }
                strongSelf->onRecvRtcp(uiTrackIndex, strongSelf->_aTrackInfo[uiTrackIndex], (unsigned char *) buf->data(), buf->size());
            });
        }
	}

	if (uiTrackIndex < _aTrackInfo.size() - 1) {
		//需要继续发送SETUP命令
		sendSetup(uiTrackIndex + 1);
		return;
	}
	//所有setup命令发送完毕
	//设置心跳包发送定时器
    weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
    _pBeatTimer.reset(new Timer((*this)[kBeatIntervalMS].as<int>() / 1000.0, [weakSelf](){
        auto strongSelf = weakSelf.lock();
        if (!strongSelf){
            return false;
        }
        strongSelf->sendRtcpPacket();
        return true;
    },getPoller()));

	//发送play命令
	pause(false);
}

void RtspPlayer::sendOptions() {
	_onHandshake = [](const Parser& parser){
//		DebugL << "options response";
	};
	sendRtspRequest("OPTIONS",_strContentBase);
}

void RtspPlayer::sendDescribe() {
	//发送DESCRIBE命令后处理函数:handleResDESCRIBE
	_onHandshake = std::bind(&RtspPlayer::handleResDESCRIBE,this, placeholders::_1);
	sendRtspRequest("DESCRIBE",_strUrl,{"Accept","application/sdp"});
}


void RtspPlayer::sendPause(bool bPause,uint32_t seekMS){
    if(!bPause){
        //修改时间轴
        int iTimeInc = seekMS - getProgressMilliSecond();
        for(unsigned int i = 0 ;i < _aTrackInfo.size() ;i++){
			_aiFistStamp[i] = _aiNowStamp[i] + iTimeInc;
            _aiNowStamp[i] = _aiFistStamp[i];
        }
        _iSeekTo = seekMS;
    }

	//开启或暂停rtsp
	_onHandshake = std::bind(&RtspPlayer::handleResPAUSE,this, placeholders::_1,bPause);
	sendRtspRequest(bPause ? "PAUSE" : "PLAY",
					_strContentBase,
					{"Range",StrPrinter << "npt=" << setiosflags(ios::fixed) << setprecision(2) << seekMS / 1000.0 << "-"});
}
void RtspPlayer::pause(bool bPause) {
    sendPause(bPause, getProgressMilliSecond());
}

void RtspPlayer::handleResPAUSE(const Parser& parser, bool bPause) {
	if (parser.Url() != "200") {
		WarnL <<(bPause ? "Pause" : "Play") << " failed:" << parser.Url() << " " << parser.Tail() << endl;
		return;
	}
	if (!bPause) {
        //修正时间轴
        auto strRange = parser["Range"];
        if (strRange.size()) {
            auto strStart = FindField(strRange.data(), "npt=", "-");
            if (strStart == "now") {
                strStart = "0";
            }
            _iSeekTo = 1000 * atof(strStart.data());
            DebugL << "seekTo(ms):" << _iSeekTo ;
        }
        auto strRtpInfo =  parser["RTP-Info"];
        if (strRtpInfo.size()) {
            strRtpInfo.append(",");
            vector<string> vec = split(strRtpInfo, ",");
            for(auto &strTrack : vec){
                strTrack.append(";");
                auto strControlSuffix = strTrack.substr(1 + strTrack.rfind('/'),strTrack.find(';') - strTrack.rfind('/') - 1);
                auto strRtpTime = FindField(strTrack.data(), "rtptime=", ";");
                auto idx = getTrackIndexByControlSuffix(strControlSuffix);
                if(idx != -1){
                    _aiFistStamp[idx] = atoll(strRtpTime.data()) * 1000 / _aTrackInfo[idx]->_samplerate;
                    _aiNowStamp[idx] = _aiFistStamp[idx];
                    DebugL << "rtptime(ms):" << strControlSuffix <<" " << strRtpTime;
                }
            }
        }
		onPlayResult_l(SockException(Err_success, "rtsp play success"));
	} else {
		_pRtpTimer.reset();
	}
}

void RtspPlayer::onWholeRtspPacket(Parser &parser) {
    try {
		decltype(_onHandshake) fun;
		_onHandshake.swap(fun);
        if(fun){
            fun(parser);
        }
        parser.Clear();
    } catch (std::exception &err) {
        SockException ex(Err_other, err.what());
        onPlayResult_l(ex);
    }
}

void RtspPlayer::onRtpPacket(const char *data, uint64_t len) {
    int trackIdx = -1;
    uint8_t interleaved = data[1];
    if(interleaved %2 == 0){
        trackIdx = getTrackIndexByInterleaved(interleaved);
        if (trackIdx != -1) {
            handleOneRtp(trackIdx,_aTrackInfo[trackIdx],(unsigned char *)data + 4, len - 4);
        }
    }else{
        trackIdx = getTrackIndexByInterleaved(interleaved - 1);
        if (trackIdx != -1) {
            onRecvRtcp(trackIdx,_aTrackInfo[trackIdx],(unsigned char *)data + 4, len - 4);
        }
    }
}

void RtspPlayer::onRecvRtcp(int iTrackidx,SdpTrack::Ptr &track, unsigned char *pucData, unsigned int uiLen){

}

void RtspPlayer::sendRtcpPacket(){
    //目前只实现了通过options命令实现心跳包
    sendOptions();
}

void RtspPlayer::onRtpSorted(const RtpPacket::Ptr &rtppt, int trackidx){
	//统计丢包率
	if (_aui16FirstSeq[trackidx] == 0 || rtppt->sequence < _aui16FirstSeq[trackidx]) {
		_aui16FirstSeq[trackidx] = rtppt->sequence;
		_aui64RtpRecv[trackidx] = 0;
	}
	_aui64RtpRecv[trackidx] ++;
	_aui16NowSeq[trackidx] = rtppt->sequence;
	_aiNowStamp[trackidx] = rtppt->timeStamp;
	if( _aiFistStamp[trackidx] == 0){
		_aiFistStamp[trackidx] = _aiNowStamp[trackidx];
	}

    rtppt->timeStamp -= _aiFistStamp[trackidx];
	onRecvRTP_l(rtppt,_aTrackInfo[trackidx]);
}
float RtspPlayer::getPacketLossRate(TrackType type) const{
	int iTrackIdx = getTrackIndexByTrackType(type);
	if(iTrackIdx == -1){
		uint64_t totalRecv = 0;
		uint64_t totalSend = 0;
		for (unsigned int i = 0; i < _aTrackInfo.size(); i++) {
			totalRecv += _aui64RtpRecv[i];
			totalSend += (_aui16NowSeq[i] - _aui16FirstSeq[i] + 1);
		}
		if(totalSend == 0){
			return 0;
		}
		return 1.0 - (double)totalRecv / totalSend;
	}


	if(_aui16NowSeq[iTrackIdx] - _aui16FirstSeq[iTrackIdx] + 1 == 0){
		return 0;
	}
	return 1.0 - (double)_aui64RtpRecv[iTrackIdx] / (_aui16NowSeq[iTrackIdx] - _aui16FirstSeq[iTrackIdx] + 1);
}

uint32_t RtspPlayer::getProgressMilliSecond() const{
	uint32_t iTime[2] = {0,0};
    for(unsigned int i = 0 ;i < _aTrackInfo.size() ;i++){
		iTime[i] = _aiNowStamp[i] - _aiFistStamp[i];
    }
    return _iSeekTo + MAX(iTime[0],iTime[1]);
}
void RtspPlayer::seekToMilliSecond(uint32_t ms) {
    sendPause(false,ms);
}

void RtspPlayer::sendRtspRequest(const string &cmd, const string &url, const std::initializer_list<string> &header) {
	string key;
	StrCaseMap header_map;
	int i = 0;
	for(auto &val : header){
		if(++i % 2 == 0){
			header_map.emplace(key,val);
		}else{
			key = val;
		}
	}
	sendRtspRequest(cmd,url,header_map);
}
void RtspPlayer::sendRtspRequest(const string &cmd, const string &url,const StrCaseMap &header_const) {
	auto header = header_const;
	header.emplace("CSeq",StrPrinter << _uiCseq++);
	header.emplace("User-Agent",SERVER_NAME "(build in " __DATE__ " " __TIME__ ")");

	if(!_strSession.empty()){
		header.emplace("Session",_strSession);
	}

	if(!_rtspRealm.empty() && !(*this)[kRtspUser].empty()){
		if(!_rtspMd5Nonce.empty()){
			//MD5认证
			/*
			response计算方法如下：
			RTSP客户端应该使用username + password并计算response如下:
			(1)当password为MD5编码,则
				response = md5( password:nonce:md5(public_method:url)  );
			(2)当password为ANSI字符串,则
				response= md5( md5(username:realm:password):nonce:md5(public_method:url) );
			 */
			string encrypted_pwd = (*this)[kRtspPwd];
			if(!(*this)[kRtspPwdIsMD5].as<bool>()){
				encrypted_pwd = MD5((*this)[kRtspUser]+ ":" + _rtspRealm + ":" + encrypted_pwd).hexdigest();
			}
			auto response = MD5( encrypted_pwd + ":" + _rtspMd5Nonce  + ":" + MD5(cmd + ":" + url).hexdigest()).hexdigest();
			_StrPrinter printer;
			printer << "Digest ";
			printer << "username=\"" << (*this)[kRtspUser] << "\", ";
			printer << "realm=\"" << _rtspRealm << "\", ";
			printer << "nonce=\"" << _rtspMd5Nonce << "\", ";
			printer << "uri=\"" << url << "\", ";
			printer << "response=\"" << response << "\"";
			header.emplace("Authorization",printer);
		}else if(!(*this)[kRtspPwdIsMD5].as<bool>()){
			//base64认证
			string authStr = StrPrinter << (*this)[kRtspUser] << ":" << (*this)[kRtspPwd];
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
	send(printer << "\r\n");
}

void RtspPlayer::onRecvRTP_l(const RtpPacket::Ptr &pRtppt, const SdpTrack::Ptr &track) {
	_rtpTicker.resetTime();
	onRecvRTP(pRtppt,track);
}
void RtspPlayer::onPlayResult_l(const SockException &ex) {
	WarnL << ex.getErrCode() << " " << ex.what();

    if(!ex){
        //播放成功，恢复rtp接收超时定时器
        _rtpTicker.resetTime();
        weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
        int timeoutMS = (*this)[kMediaTimeoutMS].as<int>();
        _pRtpTimer.reset( new Timer(timeoutMS / 2000.0, [weakSelf,timeoutMS]() {
            auto strongSelf=weakSelf.lock();
            if(!strongSelf) {
                return false;
            }
            if(strongSelf->_rtpTicker.elapsedTime()> timeoutMS) {
                //recv rtp timeout!
                strongSelf->onPlayResult_l(SockException(Err_timeout,"recv rtp timeout"));
                return false;
            }
            return true;
        },getPoller()));
    }

    if (_pPlayTimer) {
        //开始播放阶段
        _pPlayTimer.reset();
        onPlayResult(ex);
    }else {
        //播放中途阶段
        if (ex) {
            //播放成功后异常断开回调
            onShutdown(ex);
        }else{
            //恢复播放
            onResume();
        }
    }

    if(ex){
        teardown();
    }
}

int RtspPlayer::getTrackIndexByControlSuffix(const string &controlSuffix) const{
	for (unsigned int i = 0; i < _aTrackInfo.size(); i++) {
		auto pos = _aTrackInfo[i]->_control_surffix.find(controlSuffix);
		if (pos == 0) {
			return i;
		}
	}
	return -1;
}
int RtspPlayer::getTrackIndexByInterleaved(int interleaved) const{
	for (unsigned int i = 0; i < _aTrackInfo.size(); i++) {
		if (_aTrackInfo[i]->_interleaved == interleaved) {
			return i;
		}
	}
	return -1;
}

int RtspPlayer::getTrackIndexByTrackType(TrackType trackType) const {
	for (unsigned int i = 0; i < _aTrackInfo.size(); i++) {
		if (_aTrackInfo[i]->_type == trackType) {
			return i;
		}
	}
	return -1;
}

} /* namespace mediakit */


