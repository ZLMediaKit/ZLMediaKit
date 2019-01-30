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

#include <atomic>
#include <iomanip>
#include "Common/config.h"
#include "UDPServer.h"
#include "RtspSession.h"
#include "Util/mini.h"
#include "Util/MD5.h"
#include "Util/base64.h"
#include "Util/onceToken.h"
#include "Util/TimeTicker.h"
#include "Util/NoticeCenter.h"
#include "Network/sockutil.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

/**
 * rtsp协议有多种方式传输rtp数据包，目前已支持包括以下4种
 * 1: rtp over udp ,这种方式是rtp通过单独的udp端口传输
 * 2: rtp over udp_multicast,这种方式是rtp通过共享udp组播端口传输
 * 3: rtp over tcp,这种方式是通过rtsp信令tcp通道完成传输
 * 4: rtp over http，下面着重讲解：rtp over http
 *
 * rtp over http 是把rtsp协议伪装成http协议以达到穿透防火墙的目的，
 * 此时播放器会发送两次http请求至rtsp服务器，第一次是http get请求，
 * 第二次是http post请求。
 *
 * 这两次请求通过http请求头中的x-sessioncookie键完成绑定
 *
 * 第一次http get请求用于接收rtp、rtcp和rtsp回复，后续该链接不再发送其他请求
 * 第二次http post请求用于发送rtsp请求，rtsp握手结束后可能会断开连接，此时我们还要维持rtp发送
 * 需要指出的是http post请求中的content负载就是base64编码后的rtsp请求包，
 * 播放器会把rtsp请求伪装成http content负载发送至rtsp服务器，然后rtsp服务器又把回复发送给第一次http get请求的tcp链接
 * 这样，对防火墙而言，本次rtsp会话就是两次http请求，防火墙就会放行数据
 *
 * zlmediakit在处理rtsp over http的请求时，会把http poster中的content数据base64解码后转发给http getter处理
 */


//rtsp over http 情况下get请求实例，在请求实例用于接收rtp数据包
static unordered_map<string, weak_ptr<RtspSession> > g_mapGetter;
//对g_mapGetter上锁保护
static recursive_mutex g_mtxGetter;

static int kSockFlags = SOCKET_DEFAULE_FLAGS | FLAG_MORE;

RtspSession::RtspSession(const Socket::Ptr &pSock) : TcpSession(pSock) {
	//设置10秒发送缓存
	pSock->setSendBufSecond(10);
	//设置15秒发送超时时间
	pSock->setSendTimeOutSecond(15);

	DebugL <<  get_peer_ip();
}

RtspSession::~RtspSession() {
    DebugL <<  get_peer_ip();
}

void RtspSession::onError(const SockException& err) {
	TraceL << err.getErrCode() << " " << err.what();
	if (_rtpType == PlayerBase::RTP_MULTICAST) {
		//取消UDP端口监听
		UDPServer::Instance().stopListenPeer(get_peer_ip().data(), this);
	}

	if (_http_x_sessioncookie.size() != 0) {
		//移除http getter的弱引用记录
		lock_guard<recursive_mutex> lock(g_mtxGetter);
		g_mapGetter.erase(_http_x_sessioncookie);
	}

    //流量统计事件广播
    GET_CONFIG_AND_REGISTER(uint32_t,iFlowThreshold,Broadcast::kFlowThreshold);
    if(_ui64TotalBytes > iFlowThreshold * 1024){
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport,
										   _mediaInfo,
										   _ui64TotalBytes,
										   _ticker.createdTime()/1000,
										   *this);
    }

}

void RtspSession::onManager() {
	if (_ticker.createdTime() > 15 * 1000) {
		if (_strSession.size() == 0) {
			WarnL << "非法链接:" << get_peer_ip();
			shutdown();
			return;
		}
	}


	if ((_rtpType == PlayerBase::RTP_UDP || _pushSrc ) && _ticker.elapsedTime() > 15 * 1000) {
		//如果是推流端或者rtp over udp类型的播放端，那么就做超时检测
		WarnL << "RTSP会话超时:" << get_peer_ip();
		shutdown();
		return;
	}

    if(_delayTask){
        if(time(NULL) > _iTaskTimeLine){
            _delayTask();
            _delayTask = nullptr;
        }
    }
}

void RtspSession::onRecv(const Buffer::Ptr &pBuf) {
	_ticker.resetTime();
    _ui64TotalBytes += pBuf->size();
    if (_onRecv) {
		//http poster的请求数据转发给http getter处理
		_onRecv(pBuf);
	} else {
//    	TraceL << pBuf->size() << "\r\n" << pBuf->data();
		input(pBuf->data(),pBuf->size());
	}
}

void RtspSession::onWholeRtspPacket(Parser &parser) {
	string strCmd = parser.Method(); //提取出请求命令字
	_iCseq = atoi(parser["CSeq"].data());
	if(_strContentBase.empty()){
		_strContentBase = parser.Url();
		_mediaInfo.parse(parser.FullUrl());
	}

	typedef bool (RtspSession::*rtsp_request_handler)(const Parser &parser);
	static unordered_map<string, rtsp_request_handler> s_handler_map;
	static onceToken token( []() {
		s_handler_map.emplace("OPTIONS",&RtspSession::handleReq_Options);
		s_handler_map.emplace("DESCRIBE",&RtspSession::handleReq_Describe);
		s_handler_map.emplace("ANNOUNCE",&RtspSession::handleReq_ANNOUNCE);
		s_handler_map.emplace("RECORD",&RtspSession::handleReq_RECORD);
		s_handler_map.emplace("SETUP",&RtspSession::handleReq_Setup);
		s_handler_map.emplace("PLAY",&RtspSession::handleReq_Play);
		s_handler_map.emplace("PAUSE",&RtspSession::handleReq_Pause);
		s_handler_map.emplace("TEARDOWN",&RtspSession::handleReq_Teardown);
		s_handler_map.emplace("GET",&RtspSession::handleReq_Get);
		s_handler_map.emplace("POST",&RtspSession::handleReq_Post);
		s_handler_map.emplace("SET_PARAMETER",&RtspSession::handleReq_SET_PARAMETER);
		s_handler_map.emplace("GET_PARAMETER",&RtspSession::handleReq_SET_PARAMETER);
	}, []() {});

	auto it = s_handler_map.find(strCmd);
	if (it != s_handler_map.end()) {
		auto &fun = it->second;
		if(!(this->*fun)(parser)){
			shutdown();
		}
	} else{
		shutdown();
		WarnL << "不支持的rtsp命令:" << strCmd;
	}
}

void RtspSession::onRtpPacket(const char *data, uint64_t len) {
	if(!_pushSrc){
		return;
	}
	if(len > 1600){
		//没有大于MTU的包
		return;
	}
	int trackIdx = -1;
	uint8_t interleaved = data[1];
	if(interleaved %2 == 0){
		trackIdx = getTrackIndexByInterleaved(interleaved);
	}
	if (trackIdx != -1) {
		handleOneRtp(trackIdx,_aTrackInfo[trackIdx],(unsigned char *)data + 4, len - 4);
	}
}

int64_t RtspSession::getContentLength(Parser &parser) {
	if(parser.Method() == "POST"){
		//http post请求的content数据部分是base64编码后的rtsp请求信令包
		return remainDataSize();
	}
	return RtspSplitter::getContentLength(parser);
}


bool RtspSession::handleReq_Options(const Parser &parser) {
	//支持这些命令
	sendRtspResponse("200 OK",{"Public" , "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, ANNOUNCE, RECORD, SET_PARAMETER, GET_PARAMETER"});
	return true;
}

bool RtspSession::handleReq_ANNOUNCE(const Parser &parser) {
	auto src = dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTSP_SCHEMA,
																	   _mediaInfo._vhost,
																	   _mediaInfo._app,
																	   _mediaInfo._streamid,
																	   false));
	if(src){
		sendRtspResponse("406 Not Acceptable", {"Content-Type", "text/plain"}, "Already publishing.");
		WarnL << "ANNOUNCE:"
			  << "Already publishing:"
			  << _mediaInfo._vhost << " "
			  << _mediaInfo._app << " "
			  << _mediaInfo._streamid << endl;
		return false;
	}

	_strSession = makeRandStr(12);
    _strSdp = parser.Content();
    _aTrackInfo = SdpAttr(_strSdp).getAvailableTrack();

	_pushSrc = std::make_shared<RtspToRtmpMediaSource>(_mediaInfo._vhost,_mediaInfo._app,_mediaInfo._streamid);
	_pushSrc->setListener(dynamic_pointer_cast<MediaSourceEvent>(shared_from_this()));
	_pushSrc->onGetSDP(_strSdp);
	sendRtspResponse("200 OK");
	return true;
}

bool RtspSession::handleReq_RECORD(const Parser &parser){
	if (_aTrackInfo.empty() || parser["Session"] != _strSession) {
		send_SessionNotFound();
		return false;
	}
	auto onRes = [this](const string &err){
		bool authSuccess = err.empty();
		if(!authSuccess){
			//第一次play是播放，否则是恢复播放。只对播放鉴权
			sendRtspResponse("401 Unauthorized", {"Content-Type", "text/plain"}, err);
			shutdown();
			return;
		}

		_StrPrinter rtp_info;
		for(auto &track : _aTrackInfo){
			if (track->_inited == false) {
				//还有track没有setup
				shutdown();
				return;
			}
			rtp_info << "url=" << _strContentBase << "/" << track->_control_surffix << ",";
		}

		rtp_info.pop_back();
		sendRtspResponse("200 OK", {"RTP-Info",rtp_info});
		SockUtil::setNoDelay(_sock->rawFD(),false);
		(*this) << SocketFlags(kSockFlags);
	};

	weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
	Broadcast::AuthInvoker invoker = [weakSelf,onRes](const string &err){
		auto strongSelf = weakSelf.lock();
		if(!strongSelf){
			return;
		}
		strongSelf->async([weakSelf,onRes,err](){
			auto strongSelf = weakSelf.lock();
			if(!strongSelf){
				return;
			}
			onRes(err);
		});
	};

	//rtsp推流需要鉴权
	auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPublish,_mediaInfo,invoker,*this);
	if(!flag){
		//该事件无人监听,默认不鉴权
		onRes("");
	}
	return true;
}


bool RtspSession::handleReq_Describe(const Parser &parser) {
	weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
	auto authorization = parser["Authorization"];

	findStream([weakSelf,authorization](bool success){
    	auto strongSelf = weakSelf.lock();
    	if(!strongSelf){
			return;
    	}

    	if(!success){
			//未找到相应的MediaSource
			WarnL << "No such stream:" <<  strongSelf->_mediaInfo._vhost << " " <<  strongSelf->_mediaInfo._app << " " << strongSelf->_mediaInfo._streamid;
			strongSelf->send_StreamNotFound();
			strongSelf->shutdown();
			return;
    	}
		//该请求中的认证信息
		onGetRealm invoker = [weakSelf,authorization](const string &realm){
			if(realm.empty()){
				//无需认证,回复sdp
				onAuthSuccess(weakSelf);
				return;
			}
			//该流需要认证
			onAuthUser(weakSelf,realm,authorization);
		};

		//广播是否需要认证事件
		if(!NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastOnGetRtspRealm,
											   strongSelf->_mediaInfo,
											   invoker,
											   *strongSelf)){
			//无人监听此事件，说明无需认证
			invoker("");
		}
    });
    return true;
}
void RtspSession::onAuthSuccess(const weak_ptr<RtspSession> &weakSelf) {
    auto strongSelf = weakSelf.lock();
    if(!strongSelf){
        //本对象已销毁
        return;
    }
    strongSelf->async([weakSelf](){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            //本对象已销毁
            return;
        }
		strongSelf->sendRtspResponse("200 OK",
									 {"Content-Base",strongSelf->_strContentBase,
									  "x-Accept-Retransmit","our-retransmit",
									  "x-Accept-Dynamic-Rate","1"
									 },strongSelf->_strSdp);
    });
}
void RtspSession::onAuthFailed(const weak_ptr<RtspSession> &weakSelf,const string &realm) {
    auto strongSelf = weakSelf.lock();
    if(!strongSelf){
        //本对象已销毁
        return;
    }
    strongSelf->async([weakSelf,realm]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            //本对象已销毁
            return;
        }

        GET_CONFIG_AND_REGISTER(bool,authBasic,Rtsp::kAuthBasic);
        if (!authBasic) {
            //我们需要客户端优先以md5方式认证
			strongSelf->_strNonce = makeRandStr(32);
			strongSelf->sendRtspResponse("401 Unauthorized",
										 {"WWW-Authenticate",
										  StrPrinter << "Digest realm=\"" << realm << "\",nonce=\"" << strongSelf->_strNonce << "\"" });
        }else {
            //当然我们也支持base64认证,但是我们不建议这样做
			strongSelf->sendRtspResponse("401 Unauthorized",
										 {"WWW-Authenticate",
										  StrPrinter << "Basic realm=\"" << realm << "\"" });
        }
    });
}

void RtspSession::onAuthBasic(const weak_ptr<RtspSession> &weakSelf,const string &realm,const string &strBase64){
    //base64认证
    char user_pwd_buf[512];
    av_base64_decode((uint8_t *)user_pwd_buf,strBase64.data(),strBase64.size());
    auto user_pwd_vec = split(user_pwd_buf,":");
    if(user_pwd_vec.size() < 2){
        //认证信息格式不合法，回复401 Unauthorized
        onAuthFailed(weakSelf,realm);
        return;
    }
    auto user = user_pwd_vec[0];
    auto pwd = user_pwd_vec[1];
    onAuth invoker = [pwd,realm,weakSelf](bool encrypted,const string &good_pwd){
        if(!encrypted && pwd == good_pwd){
            //提供的是明文密码且匹配正确
            onAuthSuccess(weakSelf);
        }else{
            //密码错误
            onAuthFailed(weakSelf,realm);
        }
    };

    auto strongSelf = weakSelf.lock();
    if(!strongSelf){
        //本对象已销毁
        return;
    }

    //此时必须提供明文密码
    if(!NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastOnRtspAuth,strongSelf->_mediaInfo,user, true,invoker,*strongSelf)){
        //表明该流需要认证却没监听请求密码事件，这一般是大意的程序所为，警告之
        WarnL << "请监听kBroadcastOnRtspAuth事件！";
        //但是我们还是忽略认证以便完成播放
        //我们输入的密码是明文
        invoker(false,pwd);
    }
}

void RtspSession::onAuthDigest(const weak_ptr<RtspSession> &weakSelf,const string &realm,const string &strMd5){
    auto strongSelf = weakSelf.lock();
    if(!strongSelf){
        return;
    }

	DebugL << strMd5;
    auto mapTmp = Parser::parseArgs(strMd5,",","=");
    decltype(mapTmp) map;
    for(auto &pr : mapTmp){
        map[trim(string(pr.first)," \"")] = trim(pr.second," \"");
    }
    //check realm
    if(realm != map["realm"]){
        TraceL << "realm not mached:" << realm << "," << map["realm"];
        onAuthFailed(weakSelf,realm);
        return ;
    }
    //check nonce
    auto nonce = map["nonce"];
    if(strongSelf->_strNonce != nonce){
        TraceL << "nonce not mached:" << nonce << "," << strongSelf->_strNonce;
        onAuthFailed(weakSelf,realm);
        return ;
    }
    //check username and uri
    auto username = map["username"];
    auto uri = map["uri"];
    auto response = map["response"];
    if(username.empty() || uri.empty() || response.empty()){
        TraceL << "username/uri/response empty:" << username << "," << uri << "," << response;
        onAuthFailed(weakSelf,realm);
        return ;
    }

    auto realInvoker = [weakSelf,realm,nonce,uri,username,response](bool ignoreAuth,bool encrypted,const string &good_pwd){
        if(ignoreAuth){
            //忽略认证
            onAuthSuccess(weakSelf);
            TraceL << "auth ignored";
            return;
        }
        /*
        response计算方法如下：
        RTSP客户端应该使用username + password并计算response如下:
        (1)当password为MD5编码,则
            response = md5( password:nonce:md5(public_method:url)  );
        (2)当password为ANSI字符串,则
            response= md5( md5(username:realm:password):nonce:md5(public_method:url) );
         */
        auto encrypted_pwd = good_pwd;
        if(!encrypted){
            //提供的是明文密码
            encrypted_pwd = MD5(username+ ":" + realm + ":" + good_pwd).hexdigest();
        }

        auto good_response = MD5( encrypted_pwd + ":" + nonce + ":" + MD5(string("DESCRIBE") + ":" + uri).hexdigest()).hexdigest();
        if(strcasecmp(good_response.data(),response.data()) == 0){
            //认证成功！md5不区分大小写
            onAuthSuccess(weakSelf);
            TraceL << "onAuthSuccess";
        }else{
            //认证失败！
            onAuthFailed(weakSelf,realm);
            TraceL << "onAuthFailed";
        }
    };
    onAuth invoker = [realInvoker](bool encrypted,const string &good_pwd){
        realInvoker(false,encrypted,good_pwd);
    };

    //此时可以提供明文或md5加密的密码
    if(!NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastOnRtspAuth,strongSelf->_mediaInfo,username, false,invoker,*strongSelf)){
        //表明该流需要认证却没监听请求密码事件，这一般是大意的程序所为，警告之
        WarnL << "请监听kBroadcastOnRtspAuth事件！";
        //但是我们还是忽略认证以便完成播放
        realInvoker(true,true,"");
    }
}

void RtspSession::onAuthUser(const weak_ptr<RtspSession> &weakSelf,const string &realm,const string &authorization){
    //请求中包含认证信息
    auto authType = FindField(authorization.data(),NULL," ");
	auto authStr = FindField(authorization.data()," ",NULL);
    if(authType.empty() || authStr.empty()){
        //认证信息格式不合法，回复401 Unauthorized
        onAuthFailed(weakSelf,realm);
        return;
    }
    if(authType == "Basic"){
        //base64认证，需要明文密码
        onAuthBasic(weakSelf,realm,authStr);
    }else if(authType == "Digest"){
        //md5认证
        onAuthDigest(weakSelf,realm,authStr);
    }else{
        //其他认证方式？不支持！
        onAuthFailed(weakSelf,realm);
    }
}
inline void RtspSession::send_StreamNotFound() {
	sendRtspResponse("404 Stream Not Found",{"Connection","Close"});
}
inline void RtspSession::send_UnsupportedTransport() {
	sendRtspResponse("461 Unsupported Transport",{"Connection","Close"});
}

inline void RtspSession::send_SessionNotFound() {
	sendRtspResponse("454 Session Not Found",{"Connection","Close"});
}
bool RtspSession::handleReq_Setup(const Parser &parser) {
//处理setup命令，该函数可能进入多次
    auto controlSuffix = split(parser.Url(),"/").back();// parser.FullUrl().substr(_strContentBase.size());
    if(controlSuffix.front() == '/'){
		controlSuffix = controlSuffix.substr(1);
    }
	int trackIdx = getTrackIndexByControlSuffix(controlSuffix);
	if (trackIdx == -1) {
		//未找到相应track
		return false;
	}
	SdpTrack::Ptr &trackRef = _aTrackInfo[trackIdx];
	if (trackRef->_inited) {
		//已经初始化过该Track
		return false;
	}
	trackRef->_inited = true; //现在初始化

	if(_rtpType == PlayerBase::RTP_Invalid){
		auto strTransport = parser["Transport"];
		if(strTransport.find("TCP") != string::npos){
			_rtpType = PlayerBase::RTP_TCP;
		}else if(strTransport.find("multicast") != string::npos){
			_rtpType = PlayerBase::RTP_MULTICAST;
		}else{
			_rtpType = PlayerBase::RTP_UDP;
		}
	}

	//允许接收rtp、rtcp包
	RtspSplitter::enableRecvRtp(_rtpType == PlayerBase::RTP_TCP);

	switch (_rtpType) {
	case PlayerBase::RTP_TCP: {
		trackRef->_interleaved = trackRef->_type * 2;
		sendRtspResponse("200 OK",
						 {"Transport",StrPrinter << "RTP/AVP/TCP;unicast;"
												 << "interleaved=" << trackRef->_type * 2 << "-" << trackRef->_type * 2 + 1 << ";"
												 << "ssrc=" << printSSRC(trackRef->_ssrc),
						  "x-Transport-Options" , "late-tolerance=1.400000",
						  "x-Dynamic-Rate" , "1"
						 });
	}
		break;
	case PlayerBase::RTP_UDP: {
		//我们用trackIdx区分rtp和rtcp包
		auto pSockRtp = std::make_shared<Socket>(_sock->getPoller());
		if (!pSockRtp->bindUdpSock(0,get_local_ip().data())) {
			//分配端口失败
			WarnL << "分配rtp端口失败";
			send_NotAcceptable();
			return false;
		}
		auto pSockRtcp = std::make_shared<Socket>(_sock->getPoller());
		if (!pSockRtcp->bindUdpSock(pSockRtp->get_local_port() + 1,get_local_ip().data())) {
			//分配端口失败
			WarnL << "分配rtcp端口失败";
			send_NotAcceptable();
			return false;
		}
		_apRtpSock[trackIdx] = pSockRtp;
		_apRtcpSock[trackIdx] = pSockRtcp;
		//设置客户端内网端口信息
		string strClientPort = FindField(parser["Transport"].data(), "client_port=", NULL);
		uint16_t ui16PeerPort = atoi( FindField(strClientPort.data(), NULL, "-").data());
		struct sockaddr_in peerAddr;
		peerAddr.sin_family = AF_INET;
		peerAddr.sin_port = htons(ui16PeerPort);
		peerAddr.sin_addr.s_addr = inet_addr(get_peer_ip().data());
		bzero(&(peerAddr.sin_zero), sizeof peerAddr.sin_zero);
		_apPeerRtpPortAddr[trackIdx].reset((struct sockaddr *) (new struct sockaddr_in(peerAddr)));
		//尝试获取客户端nat映射地址
		startListenPeerUdpData(trackIdx);
		//InfoL << "分配端口:" << srv_port;

		sendRtspResponse("200 OK",
						 {"Transport",StrPrinter << "RTP/AVP/UDP;unicast;"
												 << "client_port=" << strClientPort << ";"
												 << "server_port=" << pSockRtp->get_local_port() << "-" << pSockRtcp->get_local_port() << ";"
												 << "ssrc=" << printSSRC(trackRef->_ssrc)
						 });
	}
		break;
	case PlayerBase::RTP_MULTICAST: {
		if(!_pBrdcaster){
			_pBrdcaster = RtpBroadCaster::get(get_local_ip(),_mediaInfo._vhost, _mediaInfo._app, _mediaInfo._streamid);
			if (!_pBrdcaster) {
				send_NotAcceptable();
				return false;
			}
			weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
			_pBrdcaster->setDetachCB(this, [weakSelf]() {
				auto strongSelf = weakSelf.lock();
				if(!strongSelf) {
					return;
				}
				strongSelf->safeShutdown();
			});
		}
		int iSrvPort = _pBrdcaster->getPort(trackRef->_type);
		//我们用trackIdx区分rtp和rtcp包
		//由于组播udp端口是共享的，而rtcp端口为组播udp端口+1，所以rtcp端口需要改成共享端口
		auto pSockRtcp = UDPServer::Instance().getSock(get_local_ip().data(),2*trackIdx + 1,iSrvPort + 1);
		if (!pSockRtcp) {
			//分配端口失败
			WarnL << "分配rtcp端口失败";
			send_NotAcceptable();
			return false;
		}
		startListenPeerUdpData(trackIdx);
        GET_CONFIG_AND_REGISTER(uint32_t,udpTTL,MultiCast::kUdpTTL);

		sendRtspResponse("200 OK",
						 {"Transport",StrPrinter << "RTP/AVP;multicast;"
												 << "destination=" << _pBrdcaster->getIP() << ";"
												 << "source=" << get_local_ip() << ";"
												 << "port=" << iSrvPort << "-" << pSockRtcp->get_local_port() << ";"
												 << "ttl=" << udpTTL << ";"
												 << "ssrc=" << printSSRC(trackRef->_ssrc)
						 });
	}
		break;
	default:
		break;
	}
	return true;
}

bool RtspSession::handleReq_Play(const Parser &parser) {
	if (_aTrackInfo.empty() || parser["Session"] != _strSession) {
		send_SessionNotFound();
		return false;
	}
	auto strRange = parser["Range"];
    auto onRes = [this,strRange](const string &err){
        bool authSuccess = err.empty();
        if(!authSuccess){
            //第一次play是播放，否则是恢复播放。只对播放鉴权
			sendRtspResponse("401 Unauthorized", {"Content-Type", "text/plain"}, err);
            shutdown();
            return;
        }

        auto pMediaSrc = _pMediaSrc.lock();
        if(!pMediaSrc){
        	send_StreamNotFound();
        	shutdown();
			return;
        }

        bool useBuf = true;
		_enableSendRtp = false;

		if (strRange.size() && !_bFirstPlay) {
            //这个是seek操作
			auto strStart = FindField(strRange.data(), "npt=", "-");
			if (strStart == "now") {
				strStart = "0";
			}
			auto iStartTime = 1000 * atof(strStart.data());
			InfoL << "rtsp seekTo(ms):" << iStartTime;
			useBuf = !pMediaSrc->seekTo(iStartTime);
		}else if(pMediaSrc->getRing()->readerCount() == 0){
			//第一个消费者
			pMediaSrc->seekTo(0);
		}
		_bFirstPlay = false;

		_StrPrinter rtp_info;
		for(auto &track : _aTrackInfo){
			if (track->_inited == false) {
				//还有track没有setup
				shutdown();
				return;
			}
			track->_ssrc = pMediaSrc->getSsrc(track->_type);
			track->_seq = pMediaSrc->getSeqence(track->_type);
			track->_time_stamp = pMediaSrc->getTimeStamp(track->_type);

			rtp_info << "url=" << _strContentBase << "/" << track->_control_surffix << ";"
					 << "seq=" << track->_seq << ";"
					 << "rtptime=" << (int)(track->_time_stamp * (track->_samplerate / 1000)) << ",";
		}

		rtp_info.pop_back();

		sendRtspResponse("200 OK",
						 {"Range", StrPrinter << "npt=" << setiosflags(ios::fixed) << setprecision(2) <<  pMediaSrc->getTimeStamp(TrackInvalid) / 1000.0,
						  "RTP-Info",rtp_info
						 });

		_enableSendRtp = true;

		//提高发送性能
		(*this) << SocketFlags(kSockFlags);
		SockUtil::setNoDelay(_sock->rawFD(),false);
		(*this) << SocketFlags(kSockFlags);

		if (!_pRtpReader && _rtpType != PlayerBase::RTP_MULTICAST) {
			weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
			_pRtpReader = pMediaSrc->getRing()->attach(useBuf);
			_pRtpReader->setDetachCB([weakSelf]() {
				auto strongSelf = weakSelf.lock();
				if(!strongSelf) {
					return;
				}
				strongSelf->safeShutdown();
			});
			_pRtpReader->setReadCB([weakSelf](const RtpPacket::Ptr &pack) {
				auto strongSelf = weakSelf.lock();
				if(!strongSelf) {
					return;
				}
				strongSelf->async([weakSelf,pack](){
					auto strongSelf = weakSelf.lock();
					if(!strongSelf) {
						return;
					}
					if(strongSelf->_enableSendRtp) {
						strongSelf->sendRtpPacket(pack);
					}
				});
			});
		}
    };

    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    Broadcast::AuthInvoker invoker = [weakSelf,onRes](const string &err){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        strongSelf->async([weakSelf,onRes,err](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                return;
            }
            onRes(err);
        });
    };
    if(_bFirstPlay){
        //第一次收到play命令，需要鉴权
        auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed,_mediaInfo,invoker,*this);
        if(!flag){
            //该事件无人监听,默认不鉴权
            onRes("");
        }
    }else{
        //后面是seek或恢复命令，不需要鉴权
        onRes("");
    }
	return true;
}

bool RtspSession::handleReq_Pause(const Parser &parser) {
	if (parser["Session"] != _strSession) {
		send_SessionNotFound();
		return false;
	}

	sendRtspResponse("200 OK");
	_enableSendRtp = false;
	return true;
}

bool RtspSession::handleReq_Teardown(const Parser &parser) {
	sendRtspResponse("200 OK");
	TraceL << "播放器断开连接!";
	return true;
}

bool RtspSession::handleReq_Get(const Parser &parser) {
	_http_x_sessioncookie = parser["x-sessioncookie"];
	sendRtspResponse("200 OK",
					 {"Connection","Close",
					  "Cache-Control","no-store",
					  "Pragma","no-store",
					  "Content-Type","application/x-rtsp-tunnelled",
					 },"","HTTP/1.0");

	//注册http getter，以便http poster绑定
	lock_guard<recursive_mutex> lock(g_mtxGetter);
	g_mapGetter[_http_x_sessioncookie] = dynamic_pointer_cast<RtspSession>(shared_from_this());
	return true;

}

bool RtspSession::handleReq_Post(const Parser &parser) {
	lock_guard<recursive_mutex> lock(g_mtxGetter);
	string sessioncookie = parser["x-sessioncookie"];
	//Poster 找到 Getter
	auto it = g_mapGetter.find(sessioncookie);
	if (it == g_mapGetter.end()) {
		WarnL << "Http Poster未找到Http Getter";
		return false;
	}

	//Poster 找到Getter的SOCK
	auto httpGetterWeak = it->second;
	//移除http getter的弱引用记录
	g_mapGetter.erase(sessioncookie);

	//http poster收到请求后转发给http getter处理
	_onRecv = [this,httpGetterWeak](const Buffer::Ptr &pBuf){
		auto httpGetterStrong = httpGetterWeak.lock();
		if(!httpGetterStrong){
			WarnL << "Http Getter已经释放";
			shutdown();
			return;
		}

		//切换到http getter的线程
		httpGetterStrong->async([pBuf,httpGetterWeak](){
			auto httpGetterStrong = httpGetterWeak.lock();
			if(!httpGetterStrong){
				return;
			}
			httpGetterStrong->onRecv(std::make_shared<BufferString>(decodeBase64(string(pBuf->data(),pBuf->size()))));
		});
	};

	if(!parser.Content().empty()){
		//http poster后面的粘包
		_onRecv(std::make_shared<BufferString>(parser.Content()));
	}
	return true;
}

bool RtspSession::handleReq_SET_PARAMETER(const Parser &parser) {
	//TraceL<<endl;
	sendRtspResponse("200 OK");
	return true;
}

inline void RtspSession::send_NotAcceptable() {
	sendRtspResponse("406 Not Acceptable",{"Connection","Close"});
}

void RtspSession::doDelay(int delaySec, const std::function<void()> &fun) {
    if(_delayTask){
        _delayTask();
    }
    _delayTask = fun;
    _iTaskTimeLine = time(NULL) + delaySec;
}

void RtspSession::cancelDelyaTask(){
    _delayTask = nullptr;
}

void RtspSession::findStream(const function<void(bool)> &cb) {
	bool success = findStream();
	if (success) {
		cb(true);
		return;
	}

	//广播未找到流
	NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastNotFoundStream,_mediaInfo,*this);

	weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
	auto task_id = this;
	auto media_info = _mediaInfo;

	auto onRegist = [task_id, weakSelf, media_info, cb](BroadcastMediaChangedArgs) {
		if (bRegist &&
			schema == media_info._schema &&
			vhost == media_info._vhost &&
			app == media_info._app &&
			stream == media_info._streamid) {
			//播发器请求的rtsp流终于注册上了
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return;
			}
			//切换到自己的线程再回复
			//如果触发 kBroadcastMediaChanged 事件的线程与本RtspSession绑定的线程相同,
			//那么strongSelf->async操作可能是同步操作,
			//通过指定参数may_sync为false确保 NoticeCenter::delListener操作延后执行,
			//以便防止遍历事件监听对象map时做删除操作
			strongSelf->async([task_id, weakSelf, media_info, cb]() {
				auto strongSelf = weakSelf.lock();
				if (!strongSelf) {
					return;
				}
				DebugL << "收到rtsp注册事件,回复播放器:" << media_info._schema << "/" << media_info._vhost << "/"
					   << media_info._app << "/" << media_info._streamid;
				cb(strongSelf->findStream());
				//取消延时任务，防止多次回复
				strongSelf->cancelDelyaTask();

				//取消事件监听
				//在事件触发时不能在当前线程移除事件监听,否则会导致遍历map时做删除操作导致程序崩溃
				NoticeCenter::Instance().delListener(task_id, Broadcast::kBroadcastMediaChanged);
			}, false);
		}
	};

	NoticeCenter::Instance().addListener(task_id, Broadcast::kBroadcastMediaChanged, onRegist);
	//5秒后执行失败回调
	doDelay(5, [cb,task_id]() {
		NoticeCenter::Instance().delListener(task_id,Broadcast::kBroadcastMediaChanged);
		cb(false);
	});
}

inline bool RtspSession::findStream() {
	RtspMediaSource::Ptr pMediaSrc =
    dynamic_pointer_cast<RtspMediaSource>( MediaSource::find(RTSP_SCHEMA,_mediaInfo._vhost, _mediaInfo._app,_mediaInfo._streamid) );
	if (!pMediaSrc) {
		return false;
	}
	_strSdp = pMediaSrc->getSdp();
	SdpAttr sdpAttr(_strSdp);
	_aTrackInfo = sdpAttr.getAvailableTrack();

	if (_aTrackInfo.empty()) {
		return false;
	}
	_strSession = makeRandStr(12);
	_pMediaSrc = pMediaSrc;

	for(auto &track : _aTrackInfo){
		track->_ssrc = pMediaSrc->getSsrc(track->_type);
		track->_seq = pMediaSrc->getSeqence(track->_type);
		track->_time_stamp = pMediaSrc->getTimeStamp(track->_type);
	}
	return true;
}


inline void RtspSession::sendRtpPacket(const RtpPacket::Ptr & pkt) {
	//InfoL<<(int)pkt.Interleaved;
	switch (_rtpType) {
	case PlayerBase::RTP_TCP: {
        BufferRtp::Ptr buffer(new BufferRtp(pkt));
		send(buffer);
#ifdef RTSP_SEND_RTCP
		int iTrackIndex = getTrackIndexByTrackId(pkt.interleaved / 2);
		RtcpCounter &counter = _aRtcpCnt[iTrackIndex];
		counter.pktCnt += 1;
		counter.octCount += (pkt.length - 12);
		auto &_ticker = _aRtcpTicker[iTrackIndex];
		if (_ticker.elapsedTime() > 5 * 1000) {
			//send rtcp every 5 second
			_ticker.resetTime();
			counter.timeStamp = pkt.timeStamp;
			sendRTCP();
		}
#endif
	}
		break;
	case PlayerBase::RTP_UDP: {
		int iTrackIndex = getTrackIndexByTrackType(pkt->type);
		auto &pSock = _apRtpSock[iTrackIndex];
		if (!pSock) {
			shutdown();
			return;
		}
		auto &peerAddr = _apPeerRtpPortAddr[iTrackIndex];
		if (!peerAddr) {
			return;
		}
        BufferRtp::Ptr buffer(new BufferRtp(pkt,4));
        _ui64TotalBytes += buffer->size();
        pSock->send(buffer,SOCKET_DEFAULE_FLAGS, peerAddr.get());
	}
		break;
	default:
		break;
	}
}

void RtspSession::onRtpSorted(const RtpPacket::Ptr &rtppt, int trackidx) {
	_pushSrc->onWrite(rtppt, false);
}
inline void RtspSession::onRcvPeerUdpData(int iTrackIdx, const Buffer::Ptr &pBuf, const struct sockaddr& addr) {
	//这是rtcp心跳包，说明播放器还存活
	_ticker.resetTime();

	if(iTrackIdx % 2 == 0){

		if(_pushSrc){
			handleOneRtp(iTrackIdx / 2,_aTrackInfo[iTrackIdx / 2],( unsigned char *)pBuf->data(),pBuf->size());
		}

		//这是rtp探测包
		if(!_bGotAllPeerUdp){
			//还没有获取完整的rtp探测包
			if(SockUtil::in_same_lan(get_local_ip().data(),get_peer_ip().data())){
				//在内网中，客户端上报的端口号是真实的，所以我们忽略udp打洞包
				_bGotAllPeerUdp = true;
				return;
			}
			//设置真实的客户端nat映射端口号
			_apPeerRtpPortAddr[iTrackIdx / 2].reset(new struct sockaddr(addr));
			_abGotPeerUdp[iTrackIdx / 2] = true;
			_bGotAllPeerUdp = true;//先假设获取到完整的rtp探测包
			for (unsigned int i = 0; i < _aTrackInfo.size(); i++) {
				if (!_abGotPeerUdp[i]) {
					//还有track没获取到rtp探测包
					_bGotAllPeerUdp = false;
					break;
				}
			}
		}
	}
}


inline void RtspSession::startListenPeerUdpData(int trackIdx) {
	weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());

	auto onUdpData = [weakSelf](const Buffer::Ptr &pBuf, struct sockaddr *pPeerAddr,int iTrackIdx){
		auto strongSelf=weakSelf.lock();
		if(!strongSelf) {
			return false;
		}
		struct sockaddr addr=*pPeerAddr;
		strongSelf->async([weakSelf,pBuf,addr,iTrackIdx]() {
			auto strongSelf=weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->onRcvPeerUdpData(iTrackIdx,pBuf,addr);
		});
		return true;
	};

	switch (_rtpType){
		case PlayerBase::RTP_MULTICAST:{
			//组播使用的共享rtcp端口
			UDPServer::Instance().listenPeer(get_peer_ip().data(), this, [onUdpData](
					int iTrackIdx, const Buffer::Ptr &pBuf, struct sockaddr *pPeerAddr) {
				return onUdpData(pBuf,pPeerAddr,iTrackIdx);
			});
		}
			break;
		case PlayerBase::RTP_UDP:{
			auto setEvent = [&](Socket::Ptr &sock,int iTrackIdx){
				if(!sock){
					WarnL << "udp端口为空:" << iTrackIdx;
					return;
				}
				sock->setOnRead([onUdpData,iTrackIdx](const Buffer::Ptr &pBuf, struct sockaddr *pPeerAddr){
					onUdpData(pBuf,pPeerAddr,iTrackIdx);
				});
			};
			setEvent(_apRtpSock[trackIdx], 2*trackIdx );
			setEvent(_apRtcpSock[trackIdx], 2*trackIdx + 1 );
		}
			break;

		default:
			break;
	}

}

static string dateStr(){
	char buf[64];
	time_t tt = time(NULL);
	strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
	return buf;
}

bool RtspSession::sendRtspResponse(const string &res_code,
								   const StrCaseMap &header_const,
								   const string &sdp,
								   const char *protocol){
	auto header = header_const;
	header.emplace("CSeq",StrPrinter << _iCseq);
	if(!_strSession.empty()){
		header.emplace("Session",_strSession);
	}

	header.emplace("Server",SERVER_NAME "(build in " __DATE__ " " __TIME__ ")");
	header.emplace("Date",dateStr());

	if(!sdp.empty()){
		header.emplace("Content-Length",StrPrinter << sdp.size());
		header.emplace("Content-Type","application/sdp");
	}

	_StrPrinter printer;
	printer << protocol << " " << res_code << "\r\n";
	for (auto &pr : header){
		printer << pr.first << ": " << pr.second << "\r\n";
	}

	printer << "\r\n";

	if(!sdp.empty()){
		printer << sdp;
	}
//	DebugL << printer;
	return send(std::make_shared<BufferString>(printer)) > 0 ;
}

int RtspSession::send(const Buffer::Ptr &pkt){
//	if(!_enableSendRtp){
//		DebugL << pkt->data();
//	}
	_ui64TotalBytes += pkt->size();
	return TcpSession::send(pkt);
}

bool RtspSession::sendRtspResponse(const string &res_code,
								   const std::initializer_list<string> &header,
								   const string &sdp,
								   const char *protocol) {
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
	return sendRtspResponse(res_code,header_map,sdp,protocol);
}

inline string RtspSession::printSSRC(uint32_t ui32Ssrc) {
	char tmp[9] = { 0 };
	ui32Ssrc = htonl(ui32Ssrc);
	uint8_t *pSsrc = (uint8_t *) &ui32Ssrc;
	for (int i = 0; i < 4; i++) {
		sprintf(tmp + 2 * i, "%02X", pSsrc[i]);
	}
	return tmp;
}
inline int RtspSession::getTrackIndexByTrackType(TrackType type) {
	for (unsigned int i = 0; i < _aTrackInfo.size(); i++) {
		if (type == _aTrackInfo[i]->_type) {
			return i;
		}
	}
	return -1;
}
inline int RtspSession::getTrackIndexByControlSuffix(const string &controlSuffix) {
	for (unsigned int i = 0; i < _aTrackInfo.size(); i++) {
		if (controlSuffix == _aTrackInfo[i]->_control_surffix) {
			return i;
		}
	}
	if(_aTrackInfo.size() == 1){
        return 0;
	}
	return -1;
}

inline int RtspSession::getTrackIndexByInterleaved(int interleaved){
	for (unsigned int i = 0; i < _aTrackInfo.size(); i++) {
		if (_aTrackInfo[i]->_interleaved == interleaved) {
			return i;
		}
	}
	return -1;
}

bool RtspSession::close() {
	InfoL << "kick out:" << _mediaInfo._vhost << " " << _mediaInfo._app << " " << _mediaInfo._streamid;
	safeShutdown();
	return true;
}

#ifdef RTSP_SEND_RTCP
inline void RtspSession::sendRTCP() {
	//DebugL;
	uint8_t aui8Rtcp[60] = {0};
	uint8_t *pui8Rtcp_SR = aui8Rtcp + 4, *pui8Rtcp_SDES = pui8Rtcp_SR + 28;
	for (uint8_t i = 0; i < _uiTrackCnt; i++) {
		auto &track = _aTrackInfo[i];
		auto &counter = _aRtcpCnt[i];

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
/* namespace mediakit */

