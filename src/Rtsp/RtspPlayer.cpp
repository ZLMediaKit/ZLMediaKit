/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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

enum PlayType {
    type_play = 0,
    type_pause,
    type_seek
};

RtspPlayer::RtspPlayer(const EventPoller::Ptr &poller) : TcpClient(poller){
    RtpReceiver::setPoolSize(64);
}
RtspPlayer::~RtspPlayer(void) {
    DebugL << endl;
}
void RtspPlayer::teardown(){
    if (alive()) {
        sendRtspRequest("TEARDOWN" ,_strContentBase);
        shutdown(SockException(Err_shutdown,"teardown"));
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

    _pPlayTimer.reset();
    _pRtpTimer.reset();
    _uiCseq = 1;
    _onHandshake = nullptr;
}

void RtspPlayer::play(const string &strUrl){
    RtspUrl url;
    if(!url.parse(strUrl)){
        onPlayResult_l(SockException(Err_other,StrPrinter << "illegal rtsp url:" << strUrl),false);
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

    _strUrl = url._url;
    _eType = (Rtsp::eRtpType)(int)(*this)[kRtpType];
    DebugL << url._url << " " << (url._user.size() ? url._user : "null") << " " << (url._passwd.size() ? url._passwd : "null") << " " << _eType;

    weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
    float playTimeOutSec = (*this)[kTimeoutMS].as<int>() / 1000.0;
    _pPlayTimer.reset( new Timer(playTimeOutSec,  [weakSelf]() {
        auto strongSelf=weakSelf.lock();
        if(!strongSelf) {
            return false;
        }
        strongSelf->onPlayResult_l(SockException(Err_timeout,"play rtsp timeout"),false);
        return false;
    },getPoller()));

    if(!(*this)[kNetAdapter].empty()){
        setNetAdapter((*this)[kNetAdapter]);
    }
    startConnect(url._host, url._port, playTimeOutSec);
}

void RtspPlayer::onConnect(const SockException &err){
    if(err.getErrCode() != Err_success) {
        onPlayResult_l(err,false);
        return;
    }

    sendDescribe();
}

void RtspPlayer::onRecv(const Buffer::Ptr& pBuf) {
    if(_benchmark_mode && !_pPlayTimer){
        //在性能测试模式下，如果rtsp握手完毕后，不再解析rtp包
        _rtpTicker.resetTime();
        return;
    }
    input(pBuf->data(),pBuf->size());
}

void RtspPlayer::onErr(const SockException &ex) {
    //定时器_pPlayTimer为空后表明握手结束了
    onPlayResult_l(ex,!_pPlayTimer);
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
    if(parser.Url() == "302" || parser.Url() == "301"){
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

    SdpParser sdpParser(parser.Content());
    //解析sdp
    _aTrackInfo = sdpParser.getAvailableTrack();
    auto title = sdpParser.getTrack(TrackTitle);
    _is_play_back = false;
    if(title && title->_duration ){
        _is_play_back = true;
    }

    for(auto &stamp : _stamp){
        stamp.setPlayBack(_is_play_back);
        stamp.setRelativeStamp(0);
    }

    if (_aTrackInfo.empty()) {
        throw std::runtime_error("无有效的Sdp Track");
    }
    if (!onCheckSDP(sdpParser.toString())) {
        throw std::runtime_error("onCheckSDP faied");
    }

    sendSetup(0);
}

//有必要的情况下创建udp端口
void RtspPlayer::createUdpSockIfNecessary(int track_idx){
    auto &rtpSockRef = _apRtpSock[track_idx];
    auto &rtcpSockRef = _apRtcpSock[track_idx];
    if(!rtpSockRef){
        rtpSockRef.reset(new Socket(getPoller()));
        //rtp随机端口
        if (!rtpSockRef->bindUdpSock(0, get_local_ip().data())) {
            rtpSockRef.reset();
            throw std::runtime_error("open rtp sock failed");
        }
    }

    if(!rtcpSockRef){
        rtcpSockRef.reset(new Socket(getPoller()));
        //rtcp端口为rtp端口+1，目的是为了兼容某些服务器，其实更推荐随机端口
        if (!rtcpSockRef->bindUdpSock(rtpSockRef->get_local_port() + 1, get_local_ip().data())) {
            rtcpSockRef.reset();
            throw std::runtime_error("open rtcp sock failed");
        }
    }

    if(rtpSockRef->get_local_port() % 2 != 0){
        //如果rtp端口不是偶数，那么与rtcp端口互换，目的是兼容一些要求严格的服务器
        Socket::Ptr tmp = rtpSockRef;
        rtpSockRef = rtcpSockRef;
        rtcpSockRef = tmp;
    }
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
            createUdpSockIfNecessary(trackIndex);
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
    if(strTransport.find("TCP") != string::npos || strTransport.find("interleaved") != string::npos){
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
            pRtpSockRef.reset(new Socket(getPoller()));
            if (!pRtpSockRef->bindUdpSock(rtp_port, multiAddr.data())) {
                pRtpSockRef.reset();
                throw std::runtime_error("open udp sock err");
            }
            auto fd = pRtpSockRef->rawFD();
            if (-1 == SockUtil::joinMultiAddrFilter(fd, multiAddr.data(), get_peer_ip().data(),get_local_ip().data())) {
                SockUtil::joinMultiAddr(fd, multiAddr.data(),get_local_ip().data());
            }
        } else {
            createUdpSockIfNecessary(uiTrackIndex);
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
        pRtpSockRef->setOnRead([srcIP, uiTrackIndex, weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr , int addr_len) {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            if (((struct sockaddr_in *) addr)->sin_addr.s_addr != srcIP) {
                WarnL << "收到其他地址的rtp数据:" << SockUtil::inet_ntoa(((struct sockaddr_in *) addr)->sin_addr);
                return;
            }
            strongSelf->handleOneRtp(uiTrackIndex, strongSelf->_aTrackInfo[uiTrackIndex], (unsigned char *) buf->data(), buf->size());
        });

        if(pRtcpSockRef) {
            //设置rtcp over udp接收回调处理函数
            pRtcpSockRef->setOnRead([srcIP, uiTrackIndex, weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr , int addr_len) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                if (((struct sockaddr_in *) addr)->sin_addr.s_addr != srcIP) {
                    WarnL << "收到其他地址的rtcp数据:" << SockUtil::inet_ntoa(((struct sockaddr_in *) addr)->sin_addr);
                    return;
                }
                strongSelf->onRtcpPacket(uiTrackIndex, strongSelf->_aTrackInfo[uiTrackIndex], (unsigned char *) buf->data(), buf->size());
            });
        }
    }

    if (uiTrackIndex < _aTrackInfo.size() - 1) {
        //需要继续发送SETUP命令
        sendSetup(uiTrackIndex + 1);
        return;
    }
    //所有setup命令发送完毕
    //发送play命令
    sendPause(type_play, 0);
}

void RtspPlayer::sendDescribe() {
    //发送DESCRIBE命令后处理函数:handleResDESCRIBE
    _onHandshake = std::bind(&RtspPlayer::handleResDESCRIBE,this, placeholders::_1);
    sendRtspRequest("DESCRIBE",_strUrl,{"Accept","application/sdp"});
}

void RtspPlayer::sendPause(int type , uint32_t seekMS){
    _onHandshake = std::bind(&RtspPlayer::handleResPAUSE,this, placeholders::_1,type);
    //开启或暂停rtsp
    switch (type){
        case type_pause:
            sendRtspRequest("PAUSE", _strContentBase);
            break;
        case type_play:
            sendRtspRequest("PLAY", _strContentBase);
            break;
        case type_seek:
            sendRtspRequest("PLAY", _strContentBase, {"Range",StrPrinter << "npt=" << setiosflags(ios::fixed) << setprecision(2) << seekMS / 1000.0 << "-"});
            break;
        default:
            WarnL << "unknown type : " << type;
            _onHandshake = nullptr;
            break;
    }
}

void RtspPlayer::pause(bool bPause) {
    sendPause(bPause ? type_pause : type_seek, getProgressMilliSecond());
}

void RtspPlayer::handleResPAUSE(const Parser& parser,int type) {
    if (parser.Url() != "200") {
        switch (type) {
            case type_pause:
                WarnL << "Pause failed:" << parser.Url() << " " << parser.Tail() << endl;
                break;
            case type_play:
                WarnL << "Play failed:" << parser.Url() << " " << parser.Tail() << endl;
                break;
            case type_seek:
                WarnL << "Seek failed:" << parser.Url() << " " << parser.Tail() << endl;
                break;
        }
        return;
    }

    if (type == type_pause) {
        //暂停成功！
        _pRtpTimer.reset();
        return;
    }

    //play或seek成功
    uint32_t iSeekTo = 0;
    //修正时间轴
    auto strRange = parser["Range"];
    if (strRange.size()) {
        auto strStart = FindField(strRange.data(), "npt=", "-");
        if (strStart == "now") {
            strStart = "0";
        }
        iSeekTo = 1000 * atof(strStart.data());
        DebugL << "seekTo(ms):" << iSeekTo;
    }
    //设置相对时间戳
    _stamp[0].setRelativeStamp(iSeekTo);
    _stamp[1].setRelativeStamp(iSeekTo);
    onPlayResult_l(SockException(Err_success, type == type_seek ? "resum rtsp success" : "rtsp play success"), type == type_seek);
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
        //定时器_pPlayTimer为空后表明握手结束了
        onPlayResult_l(SockException(Err_other, err.what()),!_pPlayTimer);
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
            onRtcpPacket(trackIdx, _aTrackInfo[trackIdx], (unsigned char *) data + 4, len - 4);
        }
    }
}

//此处预留rtcp处理函数
void RtspPlayer::onRtcpPacket(int iTrackidx, SdpTrack::Ptr &track, unsigned char *pucData, unsigned int uiLen){}

#if 0
//改代码提取自FFmpeg，参考之
// Receiver Report
    avio_w8(pb, (RTP_VERSION << 6) + 1); /* 1 report block */
    avio_w8(pb, RTCP_RR);
    avio_wb16(pb, 7); /* length in words - 1 */
    // our own SSRC: we use the server's SSRC + 1 to avoid conflicts
    avio_wb32(pb, s->ssrc + 1);
    avio_wb32(pb, s->ssrc); // server SSRC
    // some placeholders we should really fill...
    // RFC 1889/p64
    extended_max          = stats->cycles + stats->max_seq;
    expected              = extended_max - stats->base_seq;
    lost                  = expected - stats->received;
    lost                  = FFMIN(lost, 0xffffff); // clamp it since it's only 24 bits...
    expected_interval     = expected - stats->expected_prior;
    stats->expected_prior = expected;
    received_interval     = stats->received - stats->received_prior;
    stats->received_prior = stats->received;
    lost_interval         = expected_interval - received_interval;
    if (expected_interval == 0 || lost_interval <= 0)
        fraction = 0;
    else
        fraction = (lost_interval << 8) / expected_interval;

    fraction = (fraction << 24) | lost;

    avio_wb32(pb, fraction); /* 8 bits of fraction, 24 bits of total packets lost */
    avio_wb32(pb, extended_max); /* max sequence received */
    avio_wb32(pb, stats->jitter >> 4); /* jitter */

    if (s->last_rtcp_ntp_time == AV_NOPTS_VALUE) {
        avio_wb32(pb, 0); /* last SR timestamp */
        avio_wb32(pb, 0); /* delay since last SR */
    } else {
        uint32_t middle_32_bits   = s->last_rtcp_ntp_time >> 16; // this is valid, right? do we need to handle 64 bit values special?
        uint32_t delay_since_last = av_rescale(av_gettime_relative() - s->last_rtcp_reception_time,
                                               65536, AV_TIME_BASE);

        avio_wb32(pb, middle_32_bits); /* last SR timestamp */
        avio_wb32(pb, delay_since_last); /* delay since last SR */
    }

    // CNAME
    avio_w8(pb, (RTP_VERSION << 6) + 1); /* 1 report block */
    avio_w8(pb, RTCP_SDES);
    len = strlen(s->hostname);
    avio_wb16(pb, (7 + len + 3) / 4); /* length in words - 1 */
    avio_wb32(pb, s->ssrc + 1);
    avio_w8(pb, 0x01);
    avio_w8(pb, len);
    avio_write(pb, s->hostname, len);
    avio_w8(pb, 0); /* END */
    // padding
    for (len = (7 + len) % 4; len % 4; len++)
        avio_w8(pb, 0);
#endif

void RtspPlayer::sendReceiverReport(bool overTcp,int iTrackIndex){
    static const char s_cname[] = "ZLMediaKitRtsp";
    uint8_t aui8Rtcp[4 + 32 + 10 + sizeof(s_cname) + 1] = {0};
    uint8_t *pui8Rtcp_RR = aui8Rtcp + 4, *pui8Rtcp_SDES = pui8Rtcp_RR + 32;
    auto &track = _aTrackInfo[iTrackIndex];
    auto &counter = _aRtcpCnt[iTrackIndex];

    aui8Rtcp[0] = '$';
    aui8Rtcp[1] = track->_interleaved + 1;
    aui8Rtcp[2] = (sizeof(aui8Rtcp) - 4) >>  8;
    aui8Rtcp[3] = (sizeof(aui8Rtcp) - 4) & 0xFF;

    pui8Rtcp_RR[0] = 0x81;/* 1 report block */
    pui8Rtcp_RR[1] = 0xC9;//RTCP_RR
    pui8Rtcp_RR[2] = 0x00;
    pui8Rtcp_RR[3] = 0x07;/* length in words - 1 */

    uint32_t ssrc=htonl(track->_ssrc + 1);
    // our own SSRC: we use the server's SSRC + 1 to avoid conflicts
    memcpy(&pui8Rtcp_RR[4], &ssrc, 4);
    ssrc=htonl(track->_ssrc);
    // server SSRC
    memcpy(&pui8Rtcp_RR[8], &ssrc, 4);

    //FIXME: 8 bits of fraction, 24 bits of total packets lost
    pui8Rtcp_RR[12] = 0x00;
    pui8Rtcp_RR[13] = 0x00;
    pui8Rtcp_RR[14] = 0x00;
    pui8Rtcp_RR[15] = 0x00;

    //FIXME: max sequence received
    int cycleCount = getCycleCount(iTrackIndex);
    pui8Rtcp_RR[16] = cycleCount >> 8;
    pui8Rtcp_RR[17] = cycleCount & 0xFF;
    pui8Rtcp_RR[18] = counter.pktCnt >> 8;
    pui8Rtcp_RR[19] = counter.pktCnt & 0xFF;

    uint32_t  jitter = htonl(getJitterSize(iTrackIndex));
    //FIXME: jitter
    memcpy(pui8Rtcp_RR + 20, &jitter , 4);
    /* last SR timestamp */
    memcpy(pui8Rtcp_RR + 24, &counter.lastTimeStamp, 4);
    uint32_t msInc = htonl(ntohl(counter.timeStamp) - ntohl(counter.lastTimeStamp));
    /* delay since last SR */
    memcpy(pui8Rtcp_RR + 28, &msInc, 4);

    // CNAME
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
    }else if(_apRtcpSock[iTrackIndex]) {
        _apRtcpSock[iTrackIndex]->send((char *) aui8Rtcp + 4, sizeof(aui8Rtcp) - 4);
    }
}

void RtspPlayer::onRtpSorted(const RtpPacket::Ptr &rtppt, int trackidx){
    //统计丢包率
    if (_aui16FirstSeq[trackidx] == 0 || rtppt->sequence < _aui16FirstSeq[trackidx]) {
        _aui16FirstSeq[trackidx] = rtppt->sequence;
        _aui64RtpRecv[trackidx] = 0;
    }
    _aui64RtpRecv[trackidx] ++;
    _aui16NowSeq[trackidx] = rtppt->sequence;

    //计算相对时间戳
    int64_t dts_out;
    _stamp[trackidx].revise(rtppt->timeStamp,rtppt->timeStamp,dts_out,dts_out);
    rtppt->timeStamp = dts_out;
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
    return MAX(_stamp[0].getRelativeStamp(),_stamp[1].getRelativeStamp());
}

void RtspPlayer::seekToMilliSecond(uint32_t ms) {
    sendPause(type_seek,ms);
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

    _StrPrinter printer;
    printer << cmd << " " << url << " RTSP/1.0\r\n";
    for (auto &pr : header){
        printer << pr.first << ": " << pr.second << "\r\n";
    }
    SockSender::send(printer << "\r\n");
}

void RtspPlayer::onRecvRTP_l(const RtpPacket::Ptr &pkt, const SdpTrack::Ptr &track) {
    _rtpTicker.resetTime();
    onRecvRTP(pkt,track);

    int iTrackIndex = getTrackIndexByInterleaved(pkt->interleaved);
    if(iTrackIndex == -1){
        return;
    }
    RtcpCounter &counter = _aRtcpCnt[iTrackIndex];
    counter.pktCnt = pkt->sequence;
    auto &ticker = _aRtcpTicker[iTrackIndex];
    if (ticker.elapsedTime() > 5 * 1000) {
        //send rtcp every 5 second
        counter.lastTimeStamp = counter.timeStamp;
        //直接保存网络字节序
        memcpy(&counter.timeStamp, pkt->data() + 8 , 4);
        if(counter.lastTimeStamp != 0){
            sendReceiverReport(_eType == Rtsp::RTP_TCP,iTrackIndex);
            ticker.resetTime();
        }
    }
}

void RtspPlayer::onPlayResult_l(const SockException &ex , bool handshakeCompleted) {
    WarnL << ex.getErrCode() << " " << ex.what();

    if(!ex){
        //播放成功，恢复rtp接收超时定时器
        _rtpTicker.resetTime();
        weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
        int timeoutMS = (*this)[kMediaTimeoutMS].as<int>();
        //创建rtp数据接收超时检测定时器
        _pRtpTimer.reset( new Timer(timeoutMS / 2000.0, [weakSelf,timeoutMS]() {
            auto strongSelf=weakSelf.lock();
            if(!strongSelf) {
                return false;
            }
            if(strongSelf->_rtpTicker.elapsedTime()> timeoutMS) {
                //接收rtp媒体数据包超时
                strongSelf->onPlayResult_l(SockException(Err_timeout,"receive rtp timeout"), true);
                return false;
            }
            return true;
        },getPoller()));
    }

    if (!handshakeCompleted) {
        //开始播放阶段
        _pPlayTimer.reset();
        onPlayResult(ex);
        //是否为性能测试模式
        _benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();
    } else if (ex) {
        //播放成功后异常断开回调
        onShutdown(ex);
    } else {
        //恢复播放
        onResume();
    }

    if(ex){
        teardown();
    }
}

int RtspPlayer::getTrackIndexByInterleaved(int interleaved) const{
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

int RtspPlayer::getTrackIndexByTrackType(TrackType trackType) const {
    for (unsigned int i = 0; i < _aTrackInfo.size(); i++) {
        if (_aTrackInfo[i]->_type == trackType) {
            return i;
        }
    }
    if(_aTrackInfo.size() == 1){
        return 0;
    }
    return -1;
}

} /* namespace mediakit */
