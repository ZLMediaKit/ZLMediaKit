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

using namespace Config;
using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

static int kSockFlags = SOCKET_DEFAULE_FLAGS | FLAG_MORE;

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
RtspSession::RtspSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock) :
		TcpSession(pTh, pSock), m_pSender(pSock) {
	//设置10秒发送缓存
	pSock->setSendBufSecond(10);
	//设置15秒发送超时时间
	pSock->setSendTimeOutSecond(15);

	DebugL <<  get_peer_ip();
}

RtspSession::~RtspSession() {
	if (m_onDestory) {
		m_onDestory();
	}
    DebugL <<  get_peer_ip();
}

void RtspSession::shutdown(){
	shutdown_l(true);
}
void RtspSession::shutdown_l(bool close){
	if (_sock) {
		_sock->emitErr(SockException(Err_other, "self shutdown"),close);
	}
	if (m_bBase64need && !_sock) {
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
	if (m_bListenPeerUdpData) {
		//取消UDP端口监听
		UDPServer::Instance().stopListenPeer(get_peer_ip().data(), this);
		m_bListenPeerUdpData = false;
	}
	if (!m_bBase64need && m_strSessionCookie.size() != 0) {
		//quickTime http getter
		lock_guard<recursive_mutex> lock(g_mtxGetter);
		g_mapGetter.erase(m_strSessionCookie);
	}

	if (m_bBase64need && err.getErrCode() == Err_eof) {
		//quickTime http postter,正在发送rtp; QuickTime只是断开了请求连接,请继续发送rtp
		_sock = nullptr;
		lock_guard<recursive_mutex> lock(g_mtxPostter);
		//为了保证脱离TCPServer后还能正常运作,需要保持本对象的强引用
		try {
			g_mapPostter.emplace(this, dynamic_pointer_cast<RtspSession>(shared_from_this()));
		}catch (std::exception &ex){
		}
		TraceL << "quickTime will not send request any more!";
	}

    //流量统计事件广播
    GET_CONFIG_AND_REGISTER(uint32_t,iFlowThreshold,Broadcast::kFlowThreshold);
    if(m_ui64TotalBytes > iFlowThreshold * 1024){
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport,
										   m_mediaInfo,
										   m_ui64TotalBytes,
										   m_ticker.createdTime()/1000,
										   *this);
    }
}

void RtspSession::onManager() {
	if (m_ticker.createdTime() > 15 * 1000) {
		if (m_strSession.size() == 0) {
			WarnL << "非法链接:" << get_peer_ip();
			shutdown();
			return;
		}
	}
	if (m_rtpType != PlayerBase::RTP_TCP && m_ticker.elapsedTime() > 15 * 1000) {
		WarnL << "RTSP会话超时:" << get_peer_ip();
		shutdown();
		return;
	}
}


int64_t RtspSession::onRecvHeader(const char *header,uint64_t len) {
    char tmp[2 * 1024];
    m_pcBuf = tmp;

	m_parser.Parse(header); //rtsp请求解析
	string strCmd = m_parser.Method(); //提取出请求命令字
	m_iCseq = atoi(m_parser["CSeq"].data());

	typedef bool (RtspSession::*rtspCMDHandle)();
	static unordered_map<string, rtspCMDHandle> g_mapCmd;
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
	}, []() {});

	auto it = g_mapCmd.find(strCmd);
	if (it != g_mapCmd.end()) {
		auto fun = it->second;
		if(!(this->*fun)()){
		    shutdown();
		}
	} else{
		shutdown();
		WarnL << "cmd=" << strCmd;
	}

    m_parser.Clear();
    return 0;
}


void RtspSession::onRecv(const Buffer::Ptr &pBuf) {
	m_ticker.resetTime();
    m_ui64TotalBytes += pBuf->size();
    if (m_bBase64need) {
		//quicktime 加密后的rtsp请求，需要解密
		auto str = decodeBase64(string(pBuf->data(),pBuf->size()));
		inputRtspOrRtcp(str.data(),str.size());
	} else {
        inputRtspOrRtcp(pBuf->data(),pBuf->size());
	}
}

void RtspSession::inputRtspOrRtcp(const char *data,uint64_t len) {
	if(data[0] == '$' && m_rtpType == PlayerBase::RTP_TCP){
		//这是rtcp
		return;
	}
    input(data,len);
}

bool RtspSession::handleReq_Options() {
//支持这些命令
	int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY,"
			" PAUSE, SET_PARAMETER, GET_PARAMETER\r\n\r\n",
			m_iCseq, SERVER_NAME,
			RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	SocketHelper::send(m_pcBuf, n);
	return true;
}

bool RtspSession::handleReq_Describe() {
    {
        //解析url获取媒体名称
        m_strUrl = m_parser.Url();
        m_mediaInfo.parse(m_parser.FullUrl());
    }

	if (!findStream()) {
		//未找到相应的MediaSource
		send_StreamNotFound();
		return false;
	}

    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    //该请求中的认证信息
    auto authorization = m_parser["Authorization"];
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
                                           m_mediaInfo,
                                           invoker,
                                            *this)){
        //无人监听此事件，说明无需认证
        invoker("");
    }
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
        char response[2 * 1024];
        int n = sprintf(response,
                        "RTSP/1.0 200 OK\r\n"
                        "CSeq: %d\r\n"
                        "Server: %s-%0.2f(build in %s)\r\n"
                        "%s"
                        "x-Accept-Retransmit: our-retransmit\r\n"
                        "x-Accept-Dynamic-Rate: 1\r\n"
                        "Content-Base: %s/\r\n"
                        "Content-Type: application/sdp\r\n"
                        "Content-Length: %d\r\n\r\n%s",
                        strongSelf->m_iCseq, SERVER_NAME,
                        RTSP_VERSION, RTSP_BUILDTIME,
                        dateHeader().data(), strongSelf->m_strUrl.data(),
                        (int) strongSelf->m_strSdp.length(), strongSelf->m_strSdp.data());
        strongSelf->SocketHelper::send(response, n);
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

        int n;
        char response[2 * 1024];
        GET_CONFIG_AND_REGISTER(bool,authBasic,Config::Rtsp::kAuthBasic);
        if (!authBasic) {
            //我们需要客户端优先以md5方式认证
            strongSelf->m_strNonce = makeRandStr(32);
            n = sprintf(response,
                        "RTSP/1.0 401 Unauthorized\r\n"
                        "CSeq: %d\r\n"
                        "Server: %s-%0.2f(build in %s)\r\n"
                        "%s"
                        "WWW-Authenticate: Digest realm=\"%s\",nonce=\"%s\"\r\n\r\n",
                        strongSelf->m_iCseq, SERVER_NAME,
                        RTSP_VERSION, RTSP_BUILDTIME,
                        dateHeader().data(), realm.data(), strongSelf->m_strNonce.data());
        }else {
            //当然我们也支持base64认证,但是我们不建议这样做
            n = sprintf(response,
                        "RTSP/1.0 401 Unauthorized\r\n"
                        "CSeq: %d\r\n"
                        "Server: %s-%0.2f(build in %s)\r\n"
                        "%s"
                        "WWW-Authenticate: Basic realm=\"%s\"\r\n\r\n",
                        strongSelf->m_iCseq, SERVER_NAME,
                        RTSP_VERSION, RTSP_BUILDTIME,
                        dateHeader().data(), realm.data());
        }
        strongSelf->SocketHelper::send(response, n);
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
    if(!NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastOnRtspAuth,strongSelf->m_mediaInfo,user, true,invoker,*strongSelf)){
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
    if(strongSelf->m_strNonce != nonce){
        TraceL << "nonce not mached:" << nonce << "," << strongSelf->m_strNonce;
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
    if(!NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastOnRtspAuth,strongSelf->m_mediaInfo,username, false,invoker,*strongSelf)){
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
	int n = sprintf(m_pcBuf, "RTSP/1.0 404 Stream Not Found\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Connection: Close\r\n\r\n",
			m_iCseq, SERVER_NAME,
			RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	SocketHelper::send(m_pcBuf, n);
}
inline void RtspSession::send_UnsupportedTransport() {
	int n = sprintf(m_pcBuf, "RTSP/1.0 461 Unsupported Transport\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Connection: Close\r\n\r\n",
			m_iCseq, SERVER_NAME,
			RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	SocketHelper::send(m_pcBuf, n);
}

inline void RtspSession::send_SessionNotFound() {
	int n = sprintf(m_pcBuf, "RTSP/1.0 454 Session Not Found\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Connection: Close\r\n\r\n",
			m_iCseq, SERVER_NAME,
			RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	SocketHelper::send(m_pcBuf, n);

	/*40 Method Not Allowed*/

}
bool RtspSession::handleReq_Setup() {
//处理setup命令，该函数可能进入多次
    auto controlSuffix = m_parser.FullUrl().substr(1 + m_parser.FullUrl().rfind('/'));
	int trackIdx = getTrackIndexByControlSuffix(controlSuffix);
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
				m_iCseq, SERVER_NAME,
				RTSP_VERSION, RTSP_BUILDTIME,
				dateHeader().data(), trackRef.type * 2,
                trackRef.type * 2 + 1,
				printSSRC(trackRef.ssrc).data(),
				m_strSession.data());
		SocketHelper::send(m_pcBuf, iLen);
	}
		break;
	case PlayerBase::RTP_UDP: {
		//我们用trackIdx区分rtp和rtcp包
		auto pSockRtp = UDPServer::Instance().getSock(get_local_ip().data(),2*trackIdx);
		if (!pSockRtp) {
			//分配端口失败
			WarnL << "分配rtp端口失败";
			send_NotAcceptable();
			return false;
		}
		auto pSockRtcp = UDPServer::Instance().getSock(get_local_ip().data(),2*trackIdx + 1 ,pSockRtp->get_local_port() + 1);
		if (!pSockRtcp) {
			//分配端口失败
			WarnL << "分配rtcp端口失败";
			send_NotAcceptable();
			return false;
		}
		m_apUdpSock[trackIdx] = pSockRtp;
		//设置客户端内网端口信息
		string strClientPort = FindField(m_parser["Transport"].data(), "client_port=", NULL);
		uint16_t ui16PeerPort = atoi( FindField(strClientPort.data(), NULL, "-").data());
		struct sockaddr_in peerAddr;
		peerAddr.sin_family = AF_INET;
		peerAddr.sin_port = htons(ui16PeerPort);
		peerAddr.sin_addr.s_addr = inet_addr(get_peer_ip().data());
		bzero(&(peerAddr.sin_zero), sizeof peerAddr.sin_zero);
		m_apPeerUdpAddr[trackIdx].reset((struct sockaddr *) (new struct sockaddr_in(peerAddr)));
		//尝试获取客户端nat映射地址
		startListenPeerUdpData();
		//InfoL << "分配端口:" << srv_port;
		int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
				"CSeq: %d\r\n"
				"Server: %s-%0.2f(build in %s)\r\n"
				"%s"
				"Transport: RTP/AVP/UDP;unicast;"
				"client_port=%s;server_port=%d-%d;ssrc=%s;mode=play\r\n"
				"Session: %s\r\n\r\n",
				m_iCseq, SERVER_NAME,
				RTSP_VERSION, RTSP_BUILDTIME,
				dateHeader().data(), strClientPort.data(),
				pSockRtp->get_local_port(), pSockRtcp->get_local_port(),
				printSSRC(trackRef.ssrc).data(),
				m_strSession.data());
		SocketHelper::send(m_pcBuf, n);
	}
		break;
	case PlayerBase::RTP_MULTICAST: {
		if(!m_pBrdcaster){
			m_pBrdcaster = RtpBroadCaster::get(get_local_ip(),m_mediaInfo.m_vhost, m_mediaInfo.m_app, m_mediaInfo.m_streamid);
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
		int iSrvPort = m_pBrdcaster->getPort(trackRef.type);
		//我们用trackIdx区分rtp和rtcp包
		auto pSockRtcp = UDPServer::Instance().getSock(get_local_ip().data(),2*trackIdx + 1,iSrvPort + 1);
		if (!pSockRtcp) {
			//分配端口失败
			WarnL << "分配rtcp端口失败";
			send_NotAcceptable();
			return false;
		}
		startListenPeerUdpData();
        GET_CONFIG_AND_REGISTER(uint32_t,udpTTL,MultiCast::kUdpTTL);
        int n = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
				"CSeq: %d\r\n"
				"Server: %s-%0.2f(build in %s)\r\n"
				"%s"
				"Transport: RTP/AVP;multicast;destination=%s;"
				"source=%s;port=%d-%d;ttl=%d;ssrc=%s\r\n"
				"Session: %s\r\n\r\n",
				m_iCseq, SERVER_NAME,
				RTSP_VERSION, RTSP_BUILDTIME,
				dateHeader().data(), m_pBrdcaster->getIP().data(),
				get_local_ip().data(), iSrvPort, pSockRtcp->get_local_port(),
				udpTTL,printSSRC(trackRef.ssrc).data(),
				m_strSession.data());
		SocketHelper::send(m_pcBuf, n);
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
	auto strRange = m_parser["Range"];
    auto onRes = [this,strRange](const string &err){
        bool authSuccess = err.empty();
        char response[2 * 1024];
        m_pcBuf = response;
        if(!authSuccess && m_bFirstPlay){
            //第一次play是播放，否则是恢复播放。只对播放鉴权
            int n = sprintf(m_pcBuf,
                            "RTSP/1.0 401 Unauthorized\r\n"
                            "CSeq: %d\r\n"
                            "Server: %s-%0.2f(build in %s)\r\n"
                            "%s"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: %d\r\n\r\n%s",
                            m_iCseq, SERVER_NAME,
                            RTSP_VERSION, RTSP_BUILDTIME,
                            dateHeader().data(),(int)err.size(),err.data());
			SocketHelper::send(m_pcBuf,n);
            shutdown();
            return;
        }
        if(m_pRtpReader){
            weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
            SockUtil::setNoDelay(m_pSender->rawFD(), false);
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
                    strongSelf->sendRtpPacket(pack);
                });

            });
        }

        auto pMediaSrc = m_pMediaSrc.lock();
        uint32_t iStamp = 0;
        if(pMediaSrc){
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
                track.ssrc = pMediaSrc->getSsrc(track.type);
                track.seq = pMediaSrc->getSeqence(track.type);
                track.timeStamp = pMediaSrc->getTimestamp(track.type);
            }
        }
        m_bFirstPlay = false;
        int iLen = sprintf(m_pcBuf, "RTSP/1.0 200 OK\r\n"
                                   "CSeq: %d\r\n"
                                   "Server: %s-%0.2f(build in %s)\r\n"
                                   "%s"
                                   "Session: %s\r\n"
                                   "Range: npt=%.2f-\r\n"
                                   "RTP-Info: ", m_iCseq, SERVER_NAME, RTSP_VERSION, RTSP_BUILDTIME,
                           dateHeader().data(), m_strSession.data(),iStamp/1000.0);

        for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
            auto &track = m_aTrackInfo[i];
            if (track.inited == false) {
                //还有track没有setup
                shutdown();
                return;
            }
            iLen += sprintf(m_pcBuf + iLen, "url=%s/%s;seq=%d;rtptime=%u,",
                            m_strUrl.data(), track.controlSuffix.data(), track.seq,track.timeStamp);
        }
        iLen -= 1;
        (m_pcBuf)[iLen] = '\0';
        iLen += sprintf(m_pcBuf + iLen, "\r\n\r\n");
		SocketHelper::send(m_pcBuf, iLen);

		//提高发送性能
		(*this) << SocketFlags(kSockFlags);
		SockUtil::setNoDelay(m_pSender->rawFD(),false);
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
    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed,m_mediaInfo,invoker,*this);
    if(!flag){
        //该事件无人监听,默认不鉴权
        onRes("");
    }
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
			"Session: %s\r\n\r\n", m_iCseq, SERVER_NAME, RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data(), m_strSession.data());
	SocketHelper::send(m_pcBuf, n);
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
			"Session: %s\r\n\r\n", m_iCseq, SERVER_NAME, RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data(), m_strSession.data());

	SocketHelper::send(m_pcBuf, n);
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
	//InfoL << m_strSessionCookie;
	SocketHelper::send(m_pcBuf, n);
	return true;

}

bool RtspSession::handleReq_Post() {
	lock_guard<recursive_mutex> lock(g_mtxGetter);
	string sessioncookie = m_parser["x-sessioncookie"];
//Poster 找到 Getter
	auto it = g_mapGetter.find(sessioncookie);
	if (it == g_mapGetter.end()) {
		//WarnL << sessioncookie;
		return false;
	}
	m_bBase64need = true;
//Poster 找到Getter的SOCK
	auto strongSession = it->second.lock();
	g_mapGetter.erase(sessioncookie);
	if (!strongSession) {
		send_SessionNotFound();
		//WarnL;
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
			"Session: %s\r\n\r\n", m_iCseq, SERVER_NAME, RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data(), m_strSession.data());
	SocketHelper::send(m_pcBuf, n);
	return true;
}

inline void RtspSession::send_NotAcceptable() {
	int n = sprintf(m_pcBuf, "RTSP/1.0 406 Not Acceptable\r\n"
			"CSeq: %d\r\n"
			"Server: %s-%0.2f(build in %s)\r\n"
			"%s"
			"Connection: Close\r\n\r\n", m_iCseq, SERVER_NAME, RTSP_VERSION, RTSP_BUILDTIME,
			dateHeader().data());
	SocketHelper::send(m_pcBuf, n);

}

inline bool RtspSession::findStream() {
	RtspMediaSource::Ptr pMediaSrc =
    dynamic_pointer_cast<RtspMediaSource>( MediaSource::find(RTSP_SCHEMA,m_mediaInfo.m_vhost, m_mediaInfo.m_app,m_mediaInfo.m_streamid) );
	if (!pMediaSrc) {
		WarnL << "No such stream:" <<  m_mediaInfo.m_vhost << " " <<  m_mediaInfo.m_app << " " << m_mediaInfo.m_streamid;
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
		track.ssrc = pMediaSrc->getSsrc(track.type);
		track.seq = pMediaSrc->getSeqence(track.type);
		track.timeStamp = pMediaSrc->getTimestamp(track.type);
	}

	return true;
}


inline void RtspSession::sendRtpPacket(const RtpPacket::Ptr & pkt) {
	//InfoL<<(int)pkt.Interleaved;
	switch (m_rtpType) {
	case PlayerBase::RTP_TCP: {
        BufferRtp::Ptr buffer(new BufferRtp(pkt));
		send(buffer);
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
		int iTrackIndex = getTrackIndexByTrackType(pkt->type);
		auto pSock = m_apUdpSock[iTrackIndex].lock();
		if (!pSock) {
			shutdown();
			return;
		}
		auto peerAddr = m_apPeerUdpAddr[iTrackIndex];
		if (!peerAddr) {
			return;
		}
        BufferRtp::Ptr buffer(new BufferRtp(pkt,4));
        m_ui64TotalBytes += buffer->size();
        pSock->send(buffer,kSockFlags, peerAddr.get());
	}
		break;
	default:
		break;
	}
}

inline void RtspSession::onRcvPeerUdpData(int iTrackIdx, const Buffer::Ptr &pBuf, const struct sockaddr& addr) {
	if(iTrackIdx % 2 == 0){
		//这是rtp探测包
		if(!m_bGotAllPeerUdp){
			//还没有获取完整的rtp探测包
			if(SockUtil::in_same_lan(get_local_ip().data(),get_peer_ip().data())){
				//在内网中，客户端上报的端口号是真实的，所以我们忽略udp打洞包
				m_bGotAllPeerUdp = true;
				return;
			}
			//设置真实的客户端nat映射端口号
			m_apPeerUdpAddr[iTrackIdx / 2].reset(new struct sockaddr(addr));
			m_abGotPeerUdp[iTrackIdx / 2] = true;
			m_bGotAllPeerUdp = true;//先假设获取到完整的rtp探测包
			for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
				if (!m_abGotPeerUdp[i]) {
					//还有track没获取到rtp探测包
					m_bGotAllPeerUdp = false;
					break;
				}
			}
		}
	}else{
		//这是rtcp心跳包，说明播放器还存活
		m_ticker.resetTime();
		//TraceL << "rtcp:" << (iTrackIdx-1)/2 ;
	}
}


inline void RtspSession::startListenPeerUdpData() {
	m_bListenPeerUdpData = true;
	weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
	UDPServer::Instance().listenPeer(get_peer_ip().data(), this,
			[weakSelf](int iTrackIdx,const Buffer::Ptr &pBuf,struct sockaddr *pPeerAddr)->bool {
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
	m_pSender = session->_sock;
	weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
	session->m_onDestory = [weakSelf]() {
		auto strongSelf=weakSelf.lock();
        if(!strongSelf) {
            return;
        }
        //DebugL;
        strongSelf->m_pSender->setOnErr([weakSelf](const SockException &err) {
			auto strongSelf=weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->safeShutdown();
		});
	};
	session->shutdown_l(false);
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

