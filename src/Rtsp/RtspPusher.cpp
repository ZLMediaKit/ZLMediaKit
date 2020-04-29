/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Util/MD5.h"
#include "Util/base64.h"
#include "RtspPusher.h"
#include "RtspSession.h"

using namespace mediakit::Client;

namespace mediakit {

RtspPusher::RtspPusher(const EventPoller::Ptr &poller,const RtspMediaSource::Ptr &src) : TcpClient(poller){
    _pMediaSrc = src;
}

RtspPusher::~RtspPusher() {
    teardown();
    DebugL << endl;
}

void RtspPusher::teardown() {
    if (alive()) {
        sendRtspRequest("TEARDOWN" ,_strContentBase);
        shutdown(SockException(Err_shutdown,"teardown"));
    }

    reset();
    CLEAR_ARR(_apUdpSock);
    _rtspMd5Nonce.clear();
    _rtspRealm.clear();
    _aTrackInfo.clear();
    _strSession.clear();
    _strContentBase.clear();
    _strSession.clear();
    _uiCseq = 1;
    _pPublishTimer.reset();
    _pBeatTimer.reset();
    _pRtspReader.reset();
    _aTrackInfo.clear();
    _onHandshake = nullptr;
}

void RtspPusher::publish(const string &strUrl) {
    RtspUrl url;
    if(!url.parse(strUrl)){
        onPublishResult(SockException(Err_other,StrPrinter << "illegal rtsp url:" << strUrl),false);
        return;
    }

    teardown();

    if (url._user.size()) {
        (*this)[kRtspUser] = url._user;
    }
    if (url._passwd.size()) {
        (*this)[kRtspPwd] = url._passwd;
        (*this)[kRtspPwdIsMD5] = false;
    }

    _strUrl = strUrl;
    _eType = (Rtsp::eRtpType)(int)(*this)[kRtpType];
    DebugL << url._url << " " << (url._user.size() ? url._user : "null") << " " << (url._passwd.size() ? url._passwd : "null") << " " << _eType;

    weak_ptr<RtspPusher> weakSelf = dynamic_pointer_cast<RtspPusher>(shared_from_this());
    float publishTimeOutSec = (*this)[kTimeoutMS].as<int>() / 1000.0;
    _pPublishTimer.reset( new Timer(publishTimeOutSec,  [weakSelf]() {
        auto strongSelf=weakSelf.lock();
        if(!strongSelf) {
            return false;
        }
        strongSelf->onPublishResult(SockException(Err_timeout,"publish rtsp timeout"),false);
        return false;
    },getPoller()));

    if(!(*this)[kNetAdapter].empty()){
        setNetAdapter((*this)[kNetAdapter]);
    }

    startConnect(url._host, url._port, publishTimeOutSec);
}

void RtspPusher::onPublishResult(const SockException &ex, bool handshakeCompleted) {
    if(!handshakeCompleted){
        //播放结果回调
        _pPublishTimer.reset();
        if(_onPublished){
            _onPublished(ex);
        }
    } else {
        //播放成功后异常断开回调
        if(_onShutdown){
            _onShutdown(ex);
        }
    }

    if(ex){
        teardown();
    }
}

void RtspPusher::onErr(const SockException &ex) {
    //定时器_pPublishTimer为空后表明握手结束了
    onPublishResult(ex,!_pPublishTimer);
}

void RtspPusher::onConnect(const SockException &err) {
    if(err) {
        onPublishResult(err,false);
        return;
    }
    //推流器不需要多大的接收缓存，节省内存占用
    _sock->setReadBuffer(std::make_shared<BufferRaw>(1 * 1024));
    sendAnnounce();
}

void RtspPusher::onRecv(const Buffer::Ptr &pBuf){
    try {
        input(pBuf->data(), pBuf->size());
    } catch (exception &e) {
        SockException ex(Err_other, e.what());
        //定时器_pPublishTimer为空后表明握手结束了
        onPublishResult(ex,!_pPublishTimer);
    }
}

void RtspPusher::onWholeRtspPacket(Parser &parser) {
    decltype(_onHandshake) fun;
    _onHandshake.swap(fun);
    if(fun){
        fun(parser);
    }
    parser.Clear();
}


void RtspPusher::sendAnnounce() {
    auto src = _pMediaSrc.lock();
    if (!src) {
        throw std::runtime_error("the media source was released");
    }
    //解析sdp
    _sdpParser.load(src->getSdp());
    _aTrackInfo = _sdpParser.getAvailableTrack();

    if (_aTrackInfo.empty()) {
        throw std::runtime_error("无有效的Sdp Track");
    }

    _onHandshake = std::bind(&RtspPusher::handleResAnnounce,this, placeholders::_1);
    sendRtspRequest("ANNOUNCE",_strUrl,{},src->getSdp());
}

void RtspPusher::handleResAnnounce(const Parser &parser) {
    string authInfo = parser["WWW-Authenticate"];
    //发送DESCRIBE命令后的回复
    if ((parser.Url() == "401") && handleAuthenticationFailure(authInfo)) {
        sendAnnounce();
        return;
    }
    if(parser.Url() == "302"){
        auto newUrl = parser["Location"];
        if(newUrl.empty()){
            throw std::runtime_error("未找到Location字段(跳转url)");
        }
        publish(newUrl);
        return;
    }
    if (parser.Url() != "200") {
        throw std::runtime_error(StrPrinter << "ANNOUNCE:" << parser.Url() << " " << parser.Tail());
    }
    _strContentBase = parser["Content-Base"];

    if(_strContentBase.empty()){
        _strContentBase = _strUrl;
    }
    if (_strContentBase.back() == '/') {
        _strContentBase.pop_back();
    }

    sendSetup(0);
}

bool RtspPusher::handleAuthenticationFailure(const string &paramsStr) {
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

//有必要的情况下创建udp端口
void RtspPusher::createUdpSockIfNecessary(int track_idx){
    auto &rtpSockRef = _apUdpSock[track_idx];
    if(!rtpSockRef){
        rtpSockRef.reset(new Socket(getPoller()));
        //rtp随机端口
        if (!rtpSockRef->bindUdpSock(0, get_local_ip().data())) {
            rtpSockRef.reset();
            throw std::runtime_error("open rtp sock failed");
        }
    }
}

void RtspPusher::sendSetup(unsigned int trackIndex) {
    _onHandshake = std::bind(&RtspPusher::handleResSetup,this, placeholders::_1,trackIndex);
    auto &track = _aTrackInfo[trackIndex];
    auto baseUrl = _strContentBase + "/" + track->_control_surffix;
    switch (_eType) {
        case Rtsp::RTP_TCP: {
            sendRtspRequest("SETUP",baseUrl,{"Transport",StrPrinter << "RTP/AVP/TCP;unicast;interleaved=" << track->_type * 2 << "-" << track->_type * 2 + 1});
        }
            break;
        case Rtsp::RTP_UDP: {
            createUdpSockIfNecessary(trackIndex);
            int port = _apUdpSock[trackIndex]->get_local_port();
            sendRtspRequest("SETUP",baseUrl,{"Transport",StrPrinter << "RTP/AVP;unicast;client_port=" << port << "-" << port + 1});
        }
            break;
        default:
            break;
    }
}


void RtspPusher::handleResSetup(const Parser &parser, unsigned int uiTrackIndex) {
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
    if(strTransport.find("TCP") != string::npos || strTransport.find("interleaved") != string::npos){
        _eType = Rtsp::RTP_TCP;
        string interleaved = FindField( FindField((strTransport + ";").data(), "interleaved=", ";").data(), NULL, "-");
        _aTrackInfo[uiTrackIndex]->_interleaved = atoi(interleaved.data());
    }else if(strTransport.find("multicast") != string::npos){
        throw std::runtime_error("SETUP rtsp pusher can not support multicast!");
    }else{
        _eType = Rtsp::RTP_UDP;
        createUdpSockIfNecessary(uiTrackIndex);
        const char *strPos = "server_port=" ;
        auto port_str = FindField((strTransport + ";").data(), strPos, ";");
        uint16_t port = atoi(FindField(port_str.data(), NULL, "-").data());
        struct sockaddr_in rtpto;
        rtpto.sin_port = ntohs(port);
        rtpto.sin_family = AF_INET;
        rtpto.sin_addr.s_addr = inet_addr(get_peer_ip().data());
        _apUdpSock[uiTrackIndex]->setSendPeerAddr((struct sockaddr *)&(rtpto));
    }

    RtspSplitter::enableRecvRtp(_eType == Rtsp::RTP_TCP);

    if (uiTrackIndex < _aTrackInfo.size() - 1) {
        //需要继续发送SETUP命令
        sendSetup(uiTrackIndex + 1);
        return;
    }

    sendRecord();
}

void RtspPusher::sendOptions() {
    _onHandshake = [this](const Parser& parser){};
    sendRtspRequest("OPTIONS",_strContentBase);
}

inline void RtspPusher::sendRtpPacket(const RtspMediaSource::RingDataType &pkt) {
    //InfoL<<(int)pkt.Interleaved;
    switch (_eType) {
        case Rtsp::RTP_TCP: {
            int i = 0;
            int size = pkt->size();
            setSendFlushFlag(false);
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                if (++i == size) {
                    setSendFlushFlag(true);
                }
                BufferRtp::Ptr buffer(new BufferRtp(rtp));
                send(buffer);
            });
        }
            break;
        case Rtsp::RTP_UDP: {
            int i = 0;
            int size = pkt->size();
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                int iTrackIndex = getTrackIndexByTrackType(rtp->type);
                auto &pSock = _apUdpSock[iTrackIndex];
                if (!pSock) {
                    shutdown(SockException(Err_shutdown,"udp sock not opened yet"));
                    return;
                }

                BufferRtp::Ptr buffer(new BufferRtp(rtp,4));
                pSock->send(buffer, nullptr, 0, ++i == size);
            });
        }
            break;
        default:
            break;
    }
}

inline int RtspPusher::getTrackIndexByTrackType(TrackType type) {
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

void RtspPusher::sendRecord() {
    _onHandshake = [this](const Parser& parser){
        auto src = _pMediaSrc.lock();
        if (!src) {
            throw std::runtime_error("the media source was released");
        }

        _pRtspReader = src->getRing()->attach(getPoller());
        weak_ptr<RtspPusher> weakSelf = dynamic_pointer_cast<RtspPusher>(shared_from_this());
        _pRtspReader->setReadCB([weakSelf](const RtspMediaSource::RingDataType &pkt){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf) {
                return;
            }
            strongSelf->sendRtpPacket(pkt);
        });
        _pRtspReader->setDetachCB([weakSelf](){
            auto strongSelf = weakSelf.lock();
            if(strongSelf){
                strongSelf->onPublishResult(SockException(Err_other,"媒体源被释放"), !strongSelf->_pPublishTimer);
            }
        });
        if(_eType != Rtsp::RTP_TCP){
            /////////////////////////心跳/////////////////////////////////
            weak_ptr<RtspPusher> weakSelf = dynamic_pointer_cast<RtspPusher>(shared_from_this());
            _pBeatTimer.reset(new Timer((*this)[kBeatIntervalMS].as<int>() / 1000.0, [weakSelf](){
                auto strongSelf = weakSelf.lock();
                if (!strongSelf){
                    return false;
                }
                strongSelf->sendOptions();
                return true;
            },getPoller()));
        }
        onPublishResult(SockException(Err_success,"success"), false);
        //提升发送性能
        setSocketFlags();
    };
    sendRtspRequest("RECORD",_strContentBase,{"Range","npt=0.000-"});
}

void RtspPusher::setSocketFlags(){
    GET_CONFIG(int, mergeWriteMS, General::kMergeWriteMS);
    if(mergeWriteMS > 0) {
        //提高发送性能
        setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
        SockUtil::setNoDelay(_sock->rawFD(), false);
    }
}

void RtspPusher::sendRtspRequest(const string &cmd, const string &url, const std::initializer_list<string> &header,const string &sdp ) {
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
    sendRtspRequest(cmd,url,header_map,sdp);
}
void RtspPusher::sendRtspRequest(const string &cmd, const string &url,const StrCaseMap &header_const,const string &sdp ) {
    auto header = header_const;
    header.emplace("CSeq",StrPrinter << _uiCseq++);
    header.emplace("User-Agent",SERVER_NAME);

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

    if(!sdp.empty()){
        header.emplace("Content-Length",StrPrinter << sdp.size());
        header.emplace("Content-Type","application/sdp");
    }

    _StrPrinter printer;
    printer << cmd << " " << url << " RTSP/1.0\r\n";
    for (auto &pr : header){
        printer << pr.first << ": " << pr.second << "\r\n";
    }

    printer << "\r\n";

    if(!sdp.empty()){
        printer << sdp;
    }
    SockSender::send(printer);
}


} /* namespace mediakit */