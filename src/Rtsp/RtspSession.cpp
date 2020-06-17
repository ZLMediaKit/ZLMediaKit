/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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

#define RTSP_SERVER_SEND_RTCP 0

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

RtspSession::RtspSession(const Socket::Ptr &pSock) : TcpSession(pSock) {
    DebugP(this);
    GET_CONFIG(uint32_t,keep_alive_sec,Rtsp::kKeepAliveSecond);
    pSock->setSendTimeOutSecond(keep_alive_sec);
    //起始接收buffer缓存设置为4K，节省内存
    pSock->setReadBuffer(std::make_shared<BufferRaw>(4 * 1024));
}

RtspSession::~RtspSession() {
    DebugP(this);
}

void RtspSession::onError(const SockException& err) {
    bool isPlayer = !_pushSrc;
    uint64_t duration = _ticker.createdTime()/1000;
    WarnP(this) << (isPlayer ? "RTSP播放器(" : "RTSP推流器(")
                << _mediaInfo._vhost << "/"
                << _mediaInfo._app << "/"
                << _mediaInfo._streamid
                << ")断开:" << err.what()
                << ",耗时(s):" << duration;

    if (_rtpType == Rtsp::RTP_MULTICAST) {
        //取消UDP端口监听
        UDPServer::Instance().stopListenPeer(get_peer_ip().data(), this);
    }

    if (_http_x_sessioncookie.size() != 0) {
        //移除http getter的弱引用记录
        lock_guard<recursive_mutex> lock(g_mtxGetter);
        g_mapGetter.erase(_http_x_sessioncookie);
    }

    //流量统计事件广播
    GET_CONFIG(uint32_t,iFlowThreshold,General::kFlowThreshold);
    if(_ui64TotalBytes > iFlowThreshold * 1024){
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _mediaInfo, _ui64TotalBytes, duration, isPlayer, static_cast<SockInfo &>(*this));
    }

}

void RtspSession::onManager() {
    GET_CONFIG(uint32_t,handshake_sec,Rtsp::kHandshakeSecond);
    GET_CONFIG(uint32_t,keep_alive_sec,Rtsp::kKeepAliveSecond);

    if (_ticker.createdTime() > handshake_sec * 1000) {
        if (_strSession.size() == 0) {
            shutdown(SockException(Err_timeout,"illegal connection"));
            return;
        }
    }


    if ((_rtpType == Rtsp::RTP_UDP || _pushSrc ) && _ticker.elapsedTime() > keep_alive_sec * 1000) {
        //如果是推流端或者rtp over udp类型的播放端，那么就做超时检测
        shutdown(SockException(Err_timeout,"rtp over udp session timeouted"));
        return;
    }
}

void RtspSession::onRecv(const Buffer::Ptr &pBuf) {
    _ticker.resetTime();
    _ui64TotalBytes += pBuf->size();
    if (_onRecv) {
        //http poster的请求数据转发给http getter处理
        _onRecv(pBuf);
    } else {
//    	TraceP(this) << pBuf->size() << "\r\n" << pBuf->data();
        input(pBuf->data(),pBuf->size());
    }
}

//字符串是否以xx结尾
static inline bool end_of(const string &str, const string &substr){
    auto pos = str.rfind(substr);
    return pos != string::npos && pos == str.size() - substr.size();
};

void RtspSession::onWholeRtspPacket(Parser &parser) {
    string strCmd = parser.Method(); //提取出请求命令字
    _iCseq = atoi(parser["CSeq"].data());
    if(_strContentBase.empty() && strCmd != "GET"){
        _strContentBase = parser.Url();
        _mediaInfo.parse(parser.FullUrl());
        _mediaInfo._schema = RTSP_SCHEMA;
    }

    typedef void (RtspSession::*rtsp_request_handler)(const Parser &parser);
    static unordered_map<string, rtsp_request_handler> s_cmd_functions;
    static onceToken token( []() {
        s_cmd_functions.emplace("OPTIONS",&RtspSession::handleReq_Options);
        s_cmd_functions.emplace("DESCRIBE",&RtspSession::handleReq_Describe);
        s_cmd_functions.emplace("ANNOUNCE",&RtspSession::handleReq_ANNOUNCE);
        s_cmd_functions.emplace("RECORD",&RtspSession::handleReq_RECORD);
        s_cmd_functions.emplace("SETUP",&RtspSession::handleReq_Setup);
        s_cmd_functions.emplace("PLAY",&RtspSession::handleReq_Play);
        s_cmd_functions.emplace("PAUSE",&RtspSession::handleReq_Pause);
        s_cmd_functions.emplace("TEARDOWN",&RtspSession::handleReq_Teardown);
        s_cmd_functions.emplace("GET",&RtspSession::handleReq_Get);
        s_cmd_functions.emplace("POST",&RtspSession::handleReq_Post);
        s_cmd_functions.emplace("SET_PARAMETER",&RtspSession::handleReq_SET_PARAMETER);
        s_cmd_functions.emplace("GET_PARAMETER",&RtspSession::handleReq_SET_PARAMETER);
    }, []() {});

    auto it = s_cmd_functions.find(strCmd);
    if (it == s_cmd_functions.end()) {
        sendRtspResponse("403 Forbidden");
        shutdown(SockException(Err_shutdown,StrPrinter << "403 Forbidden:" << strCmd));
        return;
    }

    auto &fun = it->second;
    try {
        (this->*fun)(parser);
    }catch (SockException &ex){
        if(ex){
            shutdown(ex);
        }
    }catch (exception &ex){
        shutdown(SockException(Err_shutdown,ex.what()));
    }
    parser.Clear();
}

void RtspSession::onRtpPacket(const char *data, uint64_t len) {
    if(!_pushSrc){
        return;
    }

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
            onRtcpPacket(trackIdx, _aTrackInfo[trackIdx], (unsigned char *) data + 4, len - 4);
        }
    }
}

void RtspSession::onRtcpPacket(int iTrackidx, SdpTrack::Ptr &track, unsigned char *pucData, unsigned int uiLen){

}
int64_t RtspSession::getContentLength(Parser &parser) {
    if(parser.Method() == "POST"){
        //http post请求的content数据部分是base64编码后的rtsp请求信令包
        return remainDataSize();
    }
    return RtspSplitter::getContentLength(parser);
}


void RtspSession::handleReq_Options(const Parser &parser) {
    //支持这些命令
    sendRtspResponse("200 OK",{"Public" , "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, ANNOUNCE, RECORD, SET_PARAMETER, GET_PARAMETER"});
}

void RtspSession::handleReq_ANNOUNCE(const Parser &parser) {
    auto src = dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTSP_SCHEMA,
                                                                       _mediaInfo._vhost,
                                                                       _mediaInfo._app,
                                                                       _mediaInfo._streamid));
    if(src){
        sendRtspResponse("406 Not Acceptable", {"Content-Type", "text/plain"}, "Already publishing.");
        string err = StrPrinter << "ANNOUNCE:"
                                << "Already publishing:"
                                << _mediaInfo._vhost << " "
                                << _mediaInfo._app << " "
                                << _mediaInfo._streamid << endl;
        throw SockException(Err_shutdown,err);
    }

    auto full_url = parser.FullUrl();
    if(end_of(full_url,".sdp")){
        //去除.sdp后缀，防止EasyDarwin推流器强制添加.sdp后缀
        full_url = full_url.substr(0,full_url.length() - 4);
        _mediaInfo.parse(full_url);
    }

    if(_mediaInfo._app.empty() || _mediaInfo._streamid.empty()){
        //推流rtsp url必须最少两级(rtsp://host/app/stream_id)，不允许莫名其妙的推流url
        sendRtspResponse("403 Forbidden", {"Content-Type", "text/plain"}, "rtsp推流url非法,最少确保两级rtsp url");
        throw SockException(Err_shutdown,StrPrinter << "rtsp推流url非法:" << full_url);
    }

    SdpParser sdpParser(parser.Content());
    _strSession = makeRandStr(12);
    _aTrackInfo = sdpParser.getAvailableTrack();

    _pushSrc = std::make_shared<RtspMediaSourceImp>(_mediaInfo._vhost,_mediaInfo._app,_mediaInfo._streamid);
    _pushSrc->setListener(dynamic_pointer_cast<MediaSourceEvent>(shared_from_this()));
    _pushSrc->setSdp(sdpParser.toString());

    sendRtspResponse("200 OK",{"Content-Base",_strContentBase + "/"});
}

void RtspSession::handleReq_RECORD(const Parser &parser){
    if (_aTrackInfo.empty() || parser["Session"] != _strSession) {
        send_SessionNotFound();
        throw SockException(Err_shutdown,_aTrackInfo.empty() ? "can not find any availabe track when record" : "session not found when record");
    }
    auto onRes = [this](const string &err,bool enableRtxp,bool enableHls,bool enableMP4){
        bool authSuccess = err.empty();
        if(!authSuccess){
            sendRtspResponse("401 Unauthorized", {"Content-Type", "text/plain"}, err);
            shutdown(SockException(Err_shutdown,StrPrinter << "401 Unauthorized:" << err));
            return;
        }

        //设置转协议
        _pushSrc->setProtocolTranslation(enableRtxp,enableHls,enableMP4);

        _StrPrinter rtp_info;
        for(auto &track : _aTrackInfo){
            if (track->_inited == false) {
                //还有track没有setup
                shutdown(SockException(Err_shutdown,"track not setuped"));
                return;
            }
            rtp_info << "url=" << _strContentBase << "/" << track->_control_surffix << ",";
        }

        rtp_info.pop_back();
        sendRtspResponse("200 OK", {"RTP-Info",rtp_info});
        if(_rtpType == Rtsp::RTP_TCP){
            //如果是rtsp推流服务器，并且是TCP推流，那么加大TCP接收缓存，这样能提升接收性能
            _sock->setReadBuffer(std::make_shared<BufferRaw>(256 * 1024));
            setSocketFlags();
        }
    };

    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    Broadcast::PublishAuthInvoker invoker = [weakSelf,onRes](const string &err,bool enableRtxp,bool enableHls,bool enableMP4){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        strongSelf->async([weakSelf,onRes,err,enableRtxp,enableHls,enableMP4](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                return;
            }
            onRes(err,enableRtxp,enableHls,enableMP4);
        });
    };

    //rtsp推流需要鉴权
    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPublish,_mediaInfo,invoker,static_cast<SockInfo &>(*this));
    if(!flag){
        //该事件无人监听,默认不鉴权
        GET_CONFIG(bool,toRtxp,General::kPublishToRtxp);
        GET_CONFIG(bool,toHls,General::kPublishToHls);
        GET_CONFIG(bool,toMP4,General::kPublishToMP4);
        onRes("",toRtxp,toHls,toMP4);
    }
}

void RtspSession::emitOnPlay(){
    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    //url鉴权回调
    auto onRes = [weakSelf](const string &err) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        if (!err.empty()) {
            //播放url鉴权失败
            strongSelf->sendRtspResponse("401 Unauthorized", {"Content-Type", "text/plain"}, err);
            strongSelf->shutdown(SockException(Err_shutdown, StrPrinter << "401 Unauthorized:" << err));
            return;
        }
        strongSelf->onAuthSuccess();
    };

    Broadcast::AuthInvoker invoker = [weakSelf, onRes](const string &err) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->async([onRes, err, weakSelf]() {
            onRes(err);
        });
    };

    //广播通用播放url鉴权事件
    auto flag = _emit_on_play ? false : NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed, _mediaInfo, invoker, static_cast<SockInfo &>(*this));
    if (!flag) {
        //该事件无人监听,默认不鉴权
        onRes("");
    }
    //已经鉴权过了
    _emit_on_play = true;
}

void RtspSession::handleReq_Describe(const Parser &parser) {
    //该请求中的认证信息
    auto authorization = parser["Authorization"];
    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    //rtsp专属鉴权是否开启事件回调
    onGetRealm invoker = [weakSelf, authorization](const string &realm) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            //本对象已经销毁
            return;
        }
        //切换到自己的线程然后执行
        strongSelf->async([weakSelf, realm, authorization]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                //本对象已经销毁
                return;
            }
            if (realm.empty()) {
                //无需rtsp专属认证, 那么继续url通用鉴权认证(on_play)
                strongSelf->emitOnPlay();
                return;
            }
            //该流需要rtsp专属认证，开启rtsp专属认证后，将不再触发url通用鉴权认证(on_play)
            strongSelf->_rtsp_realm = realm;
            strongSelf->onAuthUser(realm, authorization);
        });
    };

    if(_rtsp_realm.empty()){
        //广播是否需要rtsp专属认证事件
        if (!NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastOnGetRtspRealm, _mediaInfo, invoker, static_cast<SockInfo &>(*this))) {
            //无人监听此事件，说明无需认证
            invoker("");
        }
    }else{
        invoker(_rtsp_realm);
    }
}
void RtspSession::onAuthSuccess() {
    TraceP(this);
    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    MediaSource::findAsync(_mediaInfo,weakSelf.lock(),[weakSelf](const MediaSource::Ptr &src){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        auto rtsp_src = dynamic_pointer_cast<RtspMediaSource>(src);
        if (!rtsp_src) {
            //未找到相应的MediaSource
            string err = StrPrinter << "no such stream:" <<  strongSelf->_mediaInfo._vhost << " " <<  strongSelf->_mediaInfo._app << " " << strongSelf->_mediaInfo._streamid;
            strongSelf->send_StreamNotFound();
            strongSelf->shutdown(SockException(Err_shutdown,err));
            return;
        }
        //找到了相应的rtsp流
        strongSelf->_aTrackInfo = SdpParser(rtsp_src->getSdp()).getAvailableTrack();
        if (strongSelf->_aTrackInfo.empty()) {
            //该流无效
            DebugL << "无trackInfo，该流无效";
            strongSelf->send_StreamNotFound();
            strongSelf->shutdown(SockException(Err_shutdown,"can not find any available track in sdp"));
            return;
        }
        strongSelf->_strSession = makeRandStr(12);
        strongSelf->_pMediaSrc = rtsp_src;
        for(auto &track : strongSelf->_aTrackInfo){
            track->_ssrc = rtsp_src->getSsrc(track->_type);
            track->_seq = rtsp_src->getSeqence(track->_type);
            track->_time_stamp = rtsp_src->getTimeStamp(track->_type);
        }

        strongSelf->sendRtspResponse("200 OK",
                                     {"Content-Base",strongSelf->_strContentBase + "/",
                                      "x-Accept-Retransmit","our-retransmit",
                                      "x-Accept-Dynamic-Rate","1"
                                     },rtsp_src->getSdp());
    });
}
void RtspSession::onAuthFailed(const string &realm,const string &why,bool close) {
    GET_CONFIG(bool,authBasic,Rtsp::kAuthBasic);
    if (!authBasic) {
        //我们需要客户端优先以md5方式认证
        _strNonce = makeRandStr(32);
        sendRtspResponse("401 Unauthorized",
                         {"WWW-Authenticate",
                          StrPrinter << "Digest realm=\"" << realm << "\",nonce=\"" << _strNonce << "\"" });
    }else {
        //当然我们也支持base64认证,但是我们不建议这样做
        sendRtspResponse("401 Unauthorized",
                         {"WWW-Authenticate",
                          StrPrinter << "Basic realm=\"" << realm << "\"" });
    }
    if(close){
        shutdown(SockException(Err_shutdown,StrPrinter << "401 Unauthorized:" << why));
    }
}

void RtspSession::onAuthBasic(const string &realm,const string &strBase64){
    //base64认证
    char user_pwd_buf[512];
    av_base64_decode((uint8_t *)user_pwd_buf,strBase64.data(),strBase64.size());
    auto user_pwd_vec = split(user_pwd_buf,":");
    if(user_pwd_vec.size() < 2){
        //认证信息格式不合法，回复401 Unauthorized
        onAuthFailed(realm,"can not find user and passwd when basic64 auth");
        return;
    }
    auto user = user_pwd_vec[0];
    auto pwd = user_pwd_vec[1];
    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    onAuth invoker = [pwd,realm,weakSelf](bool encrypted,const string &good_pwd){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            //本对象已经销毁
            return;
        }
        //切换到自己的线程执行
        strongSelf->async([weakSelf,good_pwd,pwd,realm](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                //本对象已经销毁
                return;
            }
            //base64忽略encrypted参数，上层必须传入明文密码
            if(pwd == good_pwd){
                //提供的密码且匹配正确
                strongSelf->onAuthSuccess();
                return;
            }
            //密码错误
            strongSelf->onAuthFailed(realm,StrPrinter << "password mismatch when base64 auth:" << pwd << " != " << good_pwd);
        });
    };

    //此时必须提供明文密码
    if(!NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastOnRtspAuth,_mediaInfo,realm,user, true,invoker,static_cast<SockInfo &>(*this))){
        //表明该流需要认证却没监听请求密码事件，这一般是大意的程序所为，警告之
        WarnP(this) << "请监听kBroadcastOnRtspAuth事件！";
        //但是我们还是忽略认证以便完成播放
        //我们输入的密码是明文
        invoker(false,pwd);
    }
}

void RtspSession::onAuthDigest(const string &realm,const string &strMd5){
    DebugP(this) << strMd5;
    auto mapTmp = Parser::parseArgs(strMd5,",","=");
    decltype(mapTmp) map;
    for(auto &pr : mapTmp){
        map[trim(string(pr.first)," \"")] = trim(pr.second," \"");
    }
    //check realm
    if(realm != map["realm"]){
        onAuthFailed(realm,StrPrinter << "realm not mached:" << realm << " != " << map["realm"]);
        return ;
    }
    //check nonce
    auto nonce = map["nonce"];
    if(_strNonce != nonce){
        onAuthFailed(realm,StrPrinter << "nonce not mached:" << nonce << " != " << _strNonce);
        return ;
    }
    //check username and uri
    auto username = map["username"];
    auto uri = map["uri"];
    auto response = map["response"];
    if(username.empty() || uri.empty() || response.empty()){
        onAuthFailed(realm,StrPrinter << "username/uri/response empty:" << username << "," << uri << "," << response);
        return ;
    }

    auto realInvoker = [this,realm,nonce,uri,username,response](bool ignoreAuth,bool encrypted,const string &good_pwd){
        if(ignoreAuth){
            //忽略认证
            TraceP(this) << "auth ignored";
            onAuthSuccess();
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
            onAuthSuccess();
        }else{
            //认证失败！
            onAuthFailed(realm, StrPrinter << "password mismatch when md5 auth:" << good_response << " != " << response );
        }
    };

    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    onAuth invoker = [realInvoker,weakSelf](bool encrypted,const string &good_pwd){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        //切换到自己的线程确保realInvoker执行时，this指针有效
        strongSelf->async([realInvoker,weakSelf,encrypted,good_pwd](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                return;
            }
            realInvoker(false,encrypted,good_pwd);
        });
    };

    //此时可以提供明文或md5加密的密码
    if(!NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastOnRtspAuth,_mediaInfo,realm,username, false,invoker,static_cast<SockInfo &>(*this))){
        //表明该流需要认证却没监听请求密码事件，这一般是大意的程序所为，警告之
        WarnP(this) << "请监听kBroadcastOnRtspAuth事件！";
        //但是我们还是忽略认证以便完成播放
        realInvoker(true,true,"");
    }
}

void RtspSession::onAuthUser(const string &realm,const string &authorization){
    if(authorization.empty()){
        onAuthFailed(realm,"", false);
        return;
    }
    //请求中包含认证信息
    auto authType = FindField(authorization.data(),NULL," ");
    auto authStr = FindField(authorization.data()," ",NULL);
    if(authType.empty() || authStr.empty()){
        //认证信息格式不合法，回复401 Unauthorized
        onAuthFailed(realm,"can not find auth type or auth string");
        return;
    }
    if(authType == "Basic"){
        //base64认证，需要明文密码
        onAuthBasic(realm,authStr);
    }else if(authType == "Digest"){
        //md5认证
        onAuthDigest(realm,authStr);
    }else{
        //其他认证方式？不支持！
        onAuthFailed(realm,StrPrinter << "unsupported auth type:" << authType);
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

void RtspSession::handleReq_Setup(const Parser &parser) {
//处理setup命令，该函数可能进入多次
    auto controlSuffix = split(parser.FullUrl(),"/").back();// parser.FullUrl().substr(_strContentBase.size());
    if(controlSuffix.front() == '/'){
        controlSuffix = controlSuffix.substr(1);
    }
    int trackIdx = getTrackIndexByControlSuffix(controlSuffix);
    if (trackIdx == -1) {
        //未找到相应track
        throw SockException(Err_shutdown, StrPrinter << "can not find any track by control suffix:" << controlSuffix);
    }
    SdpTrack::Ptr &trackRef = _aTrackInfo[trackIdx];
    if (trackRef->_inited) {
        //已经初始化过该Track
        throw SockException(Err_shutdown, "can not setup one track twice");
    }
    trackRef->_inited = true; //现在初始化

    if(_rtpType == Rtsp::RTP_Invalid){
        auto &strTransport = parser["Transport"];
        if(strTransport.find("TCP") != string::npos){
            _rtpType = Rtsp::RTP_TCP;
        }else if(strTransport.find("multicast") != string::npos){
            _rtpType = Rtsp::RTP_MULTICAST;
        }else{
            _rtpType = Rtsp::RTP_UDP;
        }
    }

    //允许接收rtp、rtcp包
    RtspSplitter::enableRecvRtp(_rtpType == Rtsp::RTP_TCP);

    switch (_rtpType) {
    case Rtsp::RTP_TCP: {
        if(_pushSrc){
            //rtsp推流时，interleaved由推流者决定
            auto key_values =  Parser::parseArgs(parser["Transport"],";","=");
            int interleaved_rtp = -1 , interleaved_rtcp = -1;
            if(2 == sscanf(key_values["interleaved"].data(),"%d-%d",&interleaved_rtp,&interleaved_rtcp)){
                trackRef->_interleaved = interleaved_rtp;
            }else{
                throw SockException(Err_shutdown, "can not find interleaved when setup of rtp over tcp");
            }
        }else{
            //rtsp播放时，由于数据共享分发，所以interleaved必须由服务器决定
            trackRef->_interleaved = 2 * trackRef->_type;
        }
        sendRtspResponse("200 OK",
                         {"Transport",StrPrinter << "RTP/AVP/TCP;unicast;"
                                                 << "interleaved=" << (int)trackRef->_interleaved << "-" << (int)trackRef->_interleaved + 1 << ";"
                                                 << "ssrc=" << printSSRC(trackRef->_ssrc),
                          "x-Transport-Options" , "late-tolerance=1.400000",
                          "x-Dynamic-Rate" , "1"
                         });
    }
        break;
    case Rtsp::RTP_UDP: {
        std::pair<Socket::Ptr, Socket::Ptr> pr;
        try{
            pr = makeSockPair(_sock->getPoller(), get_local_ip());
        }catch(std::exception &ex) {
            //分配端口失败
            send_NotAcceptable();
            throw SockException(Err_shutdown, ex.what());
        }

        _apRtpSock[trackIdx] = pr.first;
        _apRtcpSock[trackIdx] = pr.second;

        //设置客户端内网端口信息
        string strClientPort = FindField(parser["Transport"].data(), "client_port=", NULL);
        uint16_t ui16RtpPort = atoi( FindField(strClientPort.data(), NULL, "-").data());
        uint16_t ui16RtcpPort = atoi( FindField(strClientPort.data(), "-" , NULL).data());

        struct sockaddr_in peerAddr;
        //设置rtp发送目标地址
        peerAddr.sin_family = AF_INET;
        peerAddr.sin_port = htons(ui16RtpPort);
        peerAddr.sin_addr.s_addr = inet_addr(get_peer_ip().data());
        bzero(&(peerAddr.sin_zero), sizeof peerAddr.sin_zero);
        pr.first->setSendPeerAddr((struct sockaddr *)(&peerAddr));

        //设置rtcp发送目标地址
        peerAddr.sin_family = AF_INET;
        peerAddr.sin_port = htons(ui16RtcpPort);
        peerAddr.sin_addr.s_addr = inet_addr(get_peer_ip().data());
        bzero(&(peerAddr.sin_zero), sizeof peerAddr.sin_zero);
        pr.second->setSendPeerAddr((struct sockaddr *)(&peerAddr));

        //尝试获取客户端nat映射地址
        startListenPeerUdpData(trackIdx);
        //InfoP(this) << "分配端口:" << srv_port;

        sendRtspResponse("200 OK",
                         {"Transport", StrPrinter << "RTP/AVP/UDP;unicast;"
                                                  << "client_port=" << strClientPort << ";"
                                                  << "server_port=" << pr.first->get_local_port() << "-" << pr.second->get_local_port() << ";"
                                                  << "ssrc=" << printSSRC(trackRef->_ssrc)
                         });
    }
        break;
    case Rtsp::RTP_MULTICAST: {
        if(!_multicaster){
            _multicaster = RtpMultiCaster::get(getPoller(),get_local_ip(),_mediaInfo._vhost, _mediaInfo._app, _mediaInfo._streamid);
            if (!_multicaster) {
                send_NotAcceptable();
                throw SockException(Err_shutdown, "can not get a available udp multicast socket");
            }
            weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
            _multicaster->setDetachCB(this, [weakSelf]() {
                auto strongSelf = weakSelf.lock();
                if(!strongSelf) {
                    return;
                }
                strongSelf->safeShutdown(SockException(Err_shutdown,"ring buffer detached"));
            });
        }
        int iSrvPort = _multicaster->getPort(trackRef->_type);
        //我们用trackIdx区分rtp和rtcp包
        //由于组播udp端口是共享的，而rtcp端口为组播udp端口+1，所以rtcp端口需要改成共享端口
        auto pSockRtcp = UDPServer::Instance().getSock(getPoller(),get_local_ip().data(),2*trackIdx + 1,iSrvPort + 1);
        if (!pSockRtcp) {
            //分配端口失败
            send_NotAcceptable();
            throw SockException(Err_shutdown, "open shared rtcp socket failed");
        }
        startListenPeerUdpData(trackIdx);
        GET_CONFIG(uint32_t,udpTTL,MultiCast::kUdpTTL);

        sendRtspResponse("200 OK",
                         {"Transport",StrPrinter << "RTP/AVP;multicast;"
                                                 << "destination=" << _multicaster->getIP() << ";"
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
}

void RtspSession::handleReq_Play(const Parser &parser) {
    if (_aTrackInfo.empty() || parser["Session"] != _strSession) {
        send_SessionNotFound();
        throw SockException(Err_shutdown,_aTrackInfo.empty() ? "can not find any available track when play" : "session not found when play");
    }
    auto pMediaSrc = _pMediaSrc.lock();
    if(!pMediaSrc){
        send_StreamNotFound();
        shutdown(SockException(Err_shutdown,"rtsp stream released"));
        return;
    }

    bool useBuf = true;
    _enableSendRtp = false;
    float iStartTime = 0;
    auto strRange = parser["Range"];
    if (strRange.size()) {
        //这个是seek操作
        auto strStart = FindField(strRange.data(), "npt=", "-");
        if (strStart == "now") {
            strStart = "0";
        }
        iStartTime = 1000 * atof(strStart.data());
        InfoP(this) << "rtsp seekTo(ms):" << iStartTime;
        useBuf = !pMediaSrc->seekTo(iStartTime);
    } else if (pMediaSrc->totalReaderCount() == 0) {
        //第一个消费者
        pMediaSrc->seekTo(0);
    }

    _StrPrinter rtp_info;
    for (auto &track : _aTrackInfo) {
        if (track->_inited == false) {
            //还有track没有setup
            shutdown(SockException(Err_shutdown, "track not setuped"));
            return;
        }
        track->_ssrc = pMediaSrc->getSsrc(track->_type);
        track->_seq = pMediaSrc->getSeqence(track->_type);
        track->_time_stamp = pMediaSrc->getTimeStamp(track->_type);

        rtp_info << "url=" << _strContentBase << "/" << track->_control_surffix << ";"
                 << "seq=" << track->_seq << ";"
                 << "rtptime=" << (int) (track->_time_stamp * (track->_samplerate / 1000)) << ",";
    }

    rtp_info.pop_back();
    sendRtspResponse("200 OK",
                     {"Range", StrPrinter << "npt=" << setiosflags(ios::fixed) << setprecision(2) << (useBuf? pMediaSrc->getTimeStamp(TrackInvalid) / 1000.0 : iStartTime / 1000),
                      "RTP-Info",rtp_info
                     });

    _enableSendRtp = true;
    setSocketFlags();

    if (!_pRtpReader && _rtpType != Rtsp::RTP_MULTICAST) {
        weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
        _pRtpReader = pMediaSrc->getRing()->attach(getPoller(), useBuf);
        _pRtpReader->setDetachCB([weakSelf]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            strongSelf->shutdown(SockException(Err_shutdown, "rtsp ring buffer detached"));
        });
        _pRtpReader->setReadCB([weakSelf](const RtspMediaSource::RingDataType &pack) {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            if (strongSelf->_enableSendRtp) {
                strongSelf->sendRtpPacket(pack);
            }
        });
    }
}

void RtspSession::handleReq_Pause(const Parser &parser) {
    if (parser["Session"] != _strSession) {
        send_SessionNotFound();
        throw SockException(Err_shutdown,"session not found when pause");
    }

    sendRtspResponse("200 OK");
    _enableSendRtp = false;
}

void RtspSession::handleReq_Teardown(const Parser &parser) {
    sendRtspResponse("200 OK");
    throw SockException(Err_shutdown,"rtsp player send teardown request");
}

void RtspSession::handleReq_Get(const Parser &parser) {
    _http_x_sessioncookie = parser["x-sessioncookie"];
    sendRtspResponse("200 OK",
                     {"Cache-Control","no-store",
                      "Pragma","no-store",
                      "Content-Type","application/x-rtsp-tunnelled",
                     },"","HTTP/1.0");

    //注册http getter，以便http poster绑定
    lock_guard<recursive_mutex> lock(g_mtxGetter);
    g_mapGetter[_http_x_sessioncookie] = dynamic_pointer_cast<RtspSession>(shared_from_this());
}

void RtspSession::handleReq_Post(const Parser &parser) {
    lock_guard<recursive_mutex> lock(g_mtxGetter);
    string sessioncookie = parser["x-sessioncookie"];
    //Poster 找到 Getter
    auto it = g_mapGetter.find(sessioncookie);
    if (it == g_mapGetter.end()) {
        throw SockException(Err_shutdown,"can not find http getter by x-sessioncookie");
    }

    //Poster 找到Getter的SOCK
    auto httpGetterWeak = it->second;
    //移除http getter的弱引用记录
    g_mapGetter.erase(sessioncookie);

    //http poster收到请求后转发给http getter处理
    _onRecv = [this,httpGetterWeak](const Buffer::Ptr &pBuf){
        auto httpGetterStrong = httpGetterWeak.lock();
        if(!httpGetterStrong){
            shutdown(SockException(Err_shutdown,"http getter released"));
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

    sendRtspResponse("200 OK",
                     {"Cache-Control","no-store",
                      "Pragma","no-store",
                      "Content-Type","application/x-rtsp-tunnelled",
                     },"","HTTP/1.0");
}

void RtspSession::handleReq_SET_PARAMETER(const Parser &parser) {
    //TraceP(this) <<endl;
    sendRtspResponse("200 OK");
}

inline void RtspSession::send_NotAcceptable() {
    sendRtspResponse("406 Not Acceptable",{"Connection","Close"});
}


void RtspSession::onRtpSorted(const RtpPacket::Ptr &rtppt, int trackidx) {
    _pushSrc->onWrite(rtppt, false);
}
inline void RtspSession::onRcvPeerUdpData(int intervaled, const Buffer::Ptr &pBuf, const struct sockaddr& addr) {
    //这是rtcp心跳包，说明播放器还存活
    _ticker.resetTime();

    if(intervaled % 2 == 0){
        if(_pushSrc){
            //这是rtsp推流上来的rtp包
            handleOneRtp(intervaled / 2,_aTrackInfo[intervaled / 2],( unsigned char *)pBuf->data(),pBuf->size());
        }else if(!_udpSockConnected.count(intervaled)){
            //这是rtsp播放器的rtp打洞包
            _udpSockConnected.emplace(intervaled);
            _apRtpSock[intervaled / 2]->setSendPeerAddr(&addr);
        }
    }else{
        //rtcp包
        if(!_udpSockConnected.count(intervaled)){
            _udpSockConnected.emplace(intervaled);
            _apRtcpSock[(intervaled - 1) / 2]->setSendPeerAddr(&addr);
        }
        onRtcpPacket((intervaled - 1) / 2, _aTrackInfo[(intervaled - 1) / 2], (unsigned char *) pBuf->data(),pBuf->size());
    }
}


inline void RtspSession::startListenPeerUdpData(int trackIdx) {
    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    auto srcIP = inet_addr(get_peer_ip().data());
    auto onUdpData = [weakSelf,srcIP](const Buffer::Ptr &pBuf, struct sockaddr *pPeerAddr,int intervaled){
        auto strongSelf=weakSelf.lock();
        if(!strongSelf) {
            return false;
        }

        if (((struct sockaddr_in *) pPeerAddr)->sin_addr.s_addr != srcIP) {
            WarnP(strongSelf.get()) << ((intervaled % 2 == 0) ? "收到其他地址的rtp数据:" : "收到其他地址的rtcp数据:")
                                    << SockUtil::inet_ntoa(((struct sockaddr_in *) pPeerAddr)->sin_addr);
            return true;
        }

        struct sockaddr addr=*pPeerAddr;
        strongSelf->async([weakSelf,pBuf,addr,intervaled]() {
            auto strongSelf=weakSelf.lock();
            if(!strongSelf) {
                return;
            }
            strongSelf->onRcvPeerUdpData(intervaled,pBuf,addr);
        });
        return true;
    };

    switch (_rtpType){
        case Rtsp::RTP_MULTICAST:{
            //组播使用的共享rtcp端口
            UDPServer::Instance().listenPeer(get_peer_ip().data(), this, [onUdpData](
                    int intervaled, const Buffer::Ptr &pBuf, struct sockaddr *pPeerAddr) {
                return onUdpData(pBuf,pPeerAddr,intervaled);
            });
        }
            break;
        case Rtsp::RTP_UDP:{
            auto setEvent = [&](Socket::Ptr &sock,int intervaled){
                if(!sock){
                    WarnP(this) << "udp端口为空:" << intervaled;
                    return;
                }
                sock->setOnRead([onUdpData,intervaled](const Buffer::Ptr &pBuf, struct sockaddr *pPeerAddr , int addr_len){
                    onUdpData(pBuf,pPeerAddr,intervaled);
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

    header.emplace("Server",SERVER_NAME);
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
//	DebugP(this) << printer;
    return send(std::make_shared<BufferString>(printer)) > 0 ;
}

int RtspSession::send(const Buffer::Ptr &pkt){
//	if(!_enableSendRtp){
//		DebugP(this) << pkt->data();
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
    if(_aTrackInfo.size() == 1){
        return 0;
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
    if(_aTrackInfo.size() == 1){
        return 0;
    }
    return -1;
}

bool RtspSession::close(MediaSource &sender,bool force) {
    //此回调在其他线程触发
    if(!_pushSrc || (!force && _pushSrc->totalReaderCount())){
        return false;
    }
    string err = StrPrinter << "close media:" << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    safeShutdown(SockException(Err_shutdown,err));
    return true;
}

int RtspSession::totalReaderCount(MediaSource &sender) {
    return _pushSrc ? _pushSrc->totalReaderCount() : sender.readerCount();
}

void RtspSession::sendRtpPacket(const RtspMediaSource::RingDataType &pkt) {
    //InfoP(this) <<(int)pkt.Interleaved;
    switch (_rtpType) {
        case Rtsp::RTP_TCP: {
            int i = 0;
            int size = pkt->size();
            setSendFlushFlag(false);
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                if (++i == size) {
                    setSendFlushFlag(true);
                }
                send(rtp);
            });
        }
            break;
        case Rtsp::RTP_UDP: {
            int i = 0;
            int size = pkt->size();
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                int iTrackIndex = getTrackIndexByTrackType(rtp->type);
                auto &pSock = _apRtpSock[iTrackIndex];
                if (!pSock) {
                    shutdown(SockException(Err_shutdown, "udp sock not opened yet"));
                    return;
                }

                BufferRtp::Ptr buffer(new BufferRtp(rtp, 4));
                _ui64TotalBytes += buffer->size();
                pSock->send(buffer, nullptr, 0, ++i == size);
            });
        }
            break;
        default:
            break;
    }

#if RTSP_SERVER_SEND_RTCP
    int iTrackIndex = getTrackIndexByTrackType(pkt->type);
    if(iTrackIndex == -1){
        return;
    }
    RtcpCounter &counter = _aRtcpCnt[iTrackIndex];
    counter.pktCnt += 1;
    counter.octCount += (pkt->length - pkt->offset);
    auto &ticker = _aRtcpTicker[iTrackIndex];
    if (ticker.elapsedTime() > 5 * 1000) {
        //send rtcp every 5 second
        ticker.resetTime();
        //直接保存网络字节序
        memcpy(&counter.timeStamp, pkt->payload + 8 , 4);
        sendSenderReport(_rtpType == Rtsp::RTP_TCP,iTrackIndex);
    }
#endif
}

void RtspSession::sendSenderReport(bool overTcp,int iTrackIndex) {
    static const char s_cname[] = "ZLMediaKitRtsp";
    uint8_t aui8Rtcp[4 + 28 + 10 + sizeof(s_cname) + 1] = {0};
    uint8_t *pui8Rtcp_SR = aui8Rtcp + 4, *pui8Rtcp_SDES = pui8Rtcp_SR + 28;
    auto &track = _aTrackInfo[iTrackIndex];
    auto &counter = _aRtcpCnt[iTrackIndex];

    aui8Rtcp[0] = '$';
    aui8Rtcp[1] = track->_interleaved + 1;
    aui8Rtcp[2] = (sizeof(aui8Rtcp) - 4) >> 8;
    aui8Rtcp[3] = (sizeof(aui8Rtcp) - 4) & 0xFF;

    pui8Rtcp_SR[0] = 0x80;
    pui8Rtcp_SR[1] = 0xC8;
    pui8Rtcp_SR[2] = 0x00;
    pui8Rtcp_SR[3] = 0x06;

    uint32_t ssrc=htonl(track->_ssrc);
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
    //直接使用网络字节序
    memcpy(&pui8Rtcp_SR[16], &counter.timeStamp, 4);

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
    memcpy(&pui8Rtcp_SDES[10], s_cname, sizeof(s_cname));
    pui8Rtcp_SDES[10 + sizeof(s_cname)] = 0x00;

    if(overTcp){
        send(obtainBuffer((char *) aui8Rtcp, sizeof(aui8Rtcp)));
    }else {
        _apRtcpSock[iTrackIndex]->send((char *) aui8Rtcp + 4, sizeof(aui8Rtcp) - 4);
    }
}

void RtspSession::setSocketFlags(){
    GET_CONFIG(int, mergeWriteMS, General::kMergeWriteMS);
    if(mergeWriteMS > 0) {
        //推流模式下，关闭TCP_NODELAY会增加推流端的延时，但是服务器性能将提高
        SockUtil::setNoDelay(_sock->rawFD(), false);
        //播放模式下，开启MSG_MORE会增加延时，但是能提高发送性能
        setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
    }
}

}
/* namespace mediakit */

