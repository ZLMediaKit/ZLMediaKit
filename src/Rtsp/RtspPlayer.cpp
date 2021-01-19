/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <set>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include "Common/config.h"
#include "RtspPlayer.h"
#include "Util/MD5.h"
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
}

RtspPlayer::~RtspPlayer(void) {
    DebugL << endl;
}

void RtspPlayer::sendTeardown(){
    if (alive()) {
        if (!_content_base.empty()) {
            sendRtspRequest("TEARDOWN", _content_base);
        }
        shutdown(SockException(Err_shutdown, "teardown"));
    }
}

void RtspPlayer::teardown(){
    sendTeardown();
    _md5_nonce.clear();
    _realm.clear();
    _sdp_track.clear();
    _session_id.clear();
    _content_base.clear();
    RtpReceiver::clear();

    CLEAR_ARR(_rtp_sock);
    CLEAR_ARR(_rtcp_sock);
    CLEAR_ARR(_rtp_seq_start)
    CLEAR_ARR(_rtp_recv_count)
    CLEAR_ARR(_rtp_recv_count)
    CLEAR_ARR(_rtp_seq_now)

    _play_check_timer.reset();
    _rtp_check_timer.reset();
    _cseq_send = 1;
    _on_response = nullptr;
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

    _play_url = url._url;
    _rtp_type = (Rtsp::eRtpType)(int)(*this)[kRtpType];
    DebugL << url._url << " " << (url._user.size() ? url._user : "null") << " " << (url._passwd.size() ? url._passwd : "null") << " " << _rtp_type;

    weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
    float playTimeOutSec = (*this)[kTimeoutMS].as<int>() / 1000.0f;
    _play_check_timer.reset(new Timer(playTimeOutSec, [weakSelf]() {
        auto strongSelf=weakSelf.lock();
        if(!strongSelf) {
            return false;
        }
        strongSelf->onPlayResult_l(SockException(Err_timeout,"play rtsp timeout"),false);
        return false;
    }, getPoller()));

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
    sendOptions();
}

void RtspPlayer::onRecv(const Buffer::Ptr& buf) {
    if(_benchmark_mode && !_play_check_timer){
        //在性能测试模式下，如果rtsp握手完毕后，不再解析rtp包
        _rtp_recv_ticker.resetTime();
        return;
    }
    input(buf->data(), buf->size());
}

void RtspPlayer::onErr(const SockException &ex) {
    //定时器_pPlayTimer为空后表明握手结束了
    onPlayResult_l(ex,!_play_check_timer);
}

// from live555
bool RtspPlayer::handleAuthenticationFailure(const string &paramsStr) {
    if(!_realm.empty()){
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
        _realm = (const char *)realm;
        _md5_nonce = (const char *)nonce;
        return true;
    }
    if (sscanf(paramsStr.data(), "Digest realm=\"%[^\"]\", nonce=\"%[^\"]\"", realm, nonce) == 2) {
        _realm = (const char *)realm;
        _md5_nonce = (const char *)nonce;
        return true;
    }
    if (sscanf(paramsStr.data(), "Basic realm=\"%[^\"]\"", realm) == 1) {
        _realm = (const char *)realm;
        return true;
    }
    return false;
}

bool RtspPlayer::handleResponse(const string &cmd, const Parser &parser){
    string authInfo = parser["WWW-Authenticate"];
    //发送DESCRIBE命令后的回复
    if ((parser.Url() == "401") && handleAuthenticationFailure(authInfo)) {
        sendOptions();
        return false;
    }
    if(parser.Url() == "302" || parser.Url() == "301"){
        auto newUrl = parser["Location"];
        if(newUrl.empty()){
            throw std::runtime_error("未找到Location字段(跳转url)");
        }
        play(newUrl);
        return false;
    }
    if (parser.Url() != "200") {
        throw std::runtime_error(StrPrinter << cmd << ":" << parser.Url() << " " << parser.Tail() << endl);
    }
    return true;
}

void RtspPlayer::handleResDESCRIBE(const Parser& parser) {
    if (!handleResponse("DESCRIBE", parser)) {
        return;
    }
    _content_base = parser["Content-Base"];
    if(_content_base.empty()){
        _content_base = _play_url;
    }
    if (_content_base.back() == '/') {
        _content_base.pop_back();
    }

    SdpParser sdpParser(parser.Content());
    //解析sdp
    _sdp_track = sdpParser.getAvailableTrack();
    auto title = sdpParser.getTrack(TrackTitle);

    if (_sdp_track.empty()) {
        throw std::runtime_error("无有效的Sdp Track");
    }
    if (!onCheckSDP(sdpParser.toString())) {
        throw std::runtime_error("onCheckSDP faied");
    }

    sendSetup(0);
}

//有必要的情况下创建udp端口
void RtspPlayer::createUdpSockIfNecessary(int track_idx){
    auto &rtpSockRef = _rtp_sock[track_idx];
    auto &rtcpSockRef = _rtcp_sock[track_idx];
    if (!rtpSockRef || !rtcpSockRef) {
        std::pair<Socket::Ptr, Socket::Ptr> pr = std::make_pair(createSocket(), createSocket());
        makeSockPair(pr, get_local_ip());
        rtpSockRef = pr.first;
        rtcpSockRef = pr.second;
    }
}

//发送SETUP命令
void RtspPlayer::sendSetup(unsigned int track_idx) {
    _on_response = std::bind(&RtspPlayer::handleResSETUP, this, placeholders::_1, track_idx);
    auto &track = _sdp_track[track_idx];
    auto baseUrl = _content_base + "/" + track->_control_surffix;
    if (track->_control.find("://") != string::npos) {
        baseUrl = track->_control;
    }
    switch (_rtp_type) {
        case Rtsp::RTP_TCP: {
            sendRtspRequest("SETUP",baseUrl,{"Transport",StrPrinter << "RTP/AVP/TCP;unicast;interleaved=" << track->_type * 2 << "-" << track->_type * 2 + 1});
        }
            break;
        case Rtsp::RTP_MULTICAST: {
            sendRtspRequest("SETUP",baseUrl,{"Transport","RTP/AVP;multicast"});
        }
            break;
        case Rtsp::RTP_UDP: {
            createUdpSockIfNecessary(track_idx);
            sendRtspRequest("SETUP", baseUrl, {"Transport",
                                               StrPrinter << "RTP/AVP;unicast;client_port="
                                                          << _rtp_sock[track_idx]->get_local_port() << "-"
                                                          << _rtcp_sock[track_idx]->get_local_port()});
        }
            break;
        default:
            break;
    }
}

void RtspPlayer::handleResSETUP(const Parser &parser, unsigned int track_idx) {
    if (parser.Url() != "200") {
        throw std::runtime_error(StrPrinter << "SETUP:" << parser.Url() << " " << parser.Tail() << endl);
    }
    if (track_idx == 0) {
        _session_id = parser["Session"];
        _session_id.append(";");
        _session_id = FindField(_session_id.data(), nullptr, ";");
    }

    auto strTransport = parser["Transport"];
    if (strTransport.find("TCP") != string::npos || strTransport.find("interleaved") != string::npos) {
        _rtp_type = Rtsp::RTP_TCP;
    } else if (strTransport.find("multicast") != string::npos) {
        _rtp_type = Rtsp::RTP_MULTICAST;
    } else {
        _rtp_type = Rtsp::RTP_UDP;
    }
    auto transport_map = Parser::parseArgs(strTransport, ";", "=");
    RtspSplitter::enableRecvRtp(_rtp_type == Rtsp::RTP_TCP);
    string ssrc = transport_map["ssrc"];
    if(!ssrc.empty()){
        sscanf(ssrc.data(), "%x", &_sdp_track[track_idx]->_ssrc);
    } else{
        _sdp_track[track_idx]->_ssrc = 0;
    }

    if (_rtp_type == Rtsp::RTP_TCP) {
        int interleaved_rtp, interleaved_rtcp;
        sscanf(transport_map["interleaved"].data(), "%d-%d", &interleaved_rtp, &interleaved_rtcp);
        _sdp_track[track_idx]->_interleaved = interleaved_rtp;
    } else {
        auto port_str = transport_map[(_rtp_type == Rtsp::RTP_MULTICAST ? "port" : "server_port")];
        int rtp_port, rtcp_port;
        sscanf(port_str.data(), "%d-%d", &rtp_port, &rtcp_port);
        auto &pRtpSockRef = _rtp_sock[track_idx];
        auto &pRtcpSockRef = _rtcp_sock[track_idx];

        if (_rtp_type == Rtsp::RTP_MULTICAST) {
            //udp组播
            auto multiAddr =  transport_map["destination"];
            pRtpSockRef = createSocket();
            if (!pRtpSockRef->bindUdpSock(rtp_port, multiAddr.data())) {
                pRtpSockRef.reset();
                throw std::runtime_error("open udp sock err");
            }
            auto fd = pRtpSockRef->rawFD();
            if (-1 == SockUtil::joinMultiAddrFilter(fd, multiAddr.data(), get_peer_ip().data(),get_local_ip().data())) {
                SockUtil::joinMultiAddr(fd, multiAddr.data(),get_local_ip().data());
            }
        } else {
            createUdpSockIfNecessary(track_idx);
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
        pRtpSockRef->setOnRead([srcIP, track_idx, weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr , int addr_len) {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            if (((struct sockaddr_in *) addr)->sin_addr.s_addr != srcIP) {
                WarnL << "收到其他地址的rtp数据:" << SockUtil::inet_ntoa(((struct sockaddr_in *) addr)->sin_addr);
                return;
            }
            strongSelf->handleOneRtp(track_idx, strongSelf->_sdp_track[track_idx]->_type,
                                     strongSelf->_sdp_track[track_idx]->_samplerate, (unsigned char *) buf->data(), buf->size());
        });

        if(pRtcpSockRef) {
            //设置rtcp over udp接收回调处理函数
            pRtcpSockRef->setOnRead([srcIP, track_idx, weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr , int addr_len) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                if (((struct sockaddr_in *) addr)->sin_addr.s_addr != srcIP) {
                    WarnL << "收到其他地址的rtcp数据:" << SockUtil::inet_ntoa(((struct sockaddr_in *) addr)->sin_addr);
                    return;
                }
                strongSelf->onRtcpPacket(track_idx, strongSelf->_sdp_track[track_idx], (unsigned char *) buf->data(), buf->size());
            });
        }
    }

    if (track_idx < _sdp_track.size() - 1) {
        //需要继续发送SETUP命令
        sendSetup(track_idx + 1);
        return;
    }
    //所有setup命令发送完毕
    //发送play命令
    sendPause(type_play, 0);
}

void RtspPlayer::sendDescribe() {
    //发送DESCRIBE命令后处理函数:handleResDESCRIBE
    _on_response = std::bind(&RtspPlayer::handleResDESCRIBE, this, placeholders::_1);
    sendRtspRequest("DESCRIBE", _play_url, {"Accept", "application/sdp"});
}

void RtspPlayer::sendOptions(){
    _on_response = [this](const Parser& parser){
        if (!handleResponse("OPTIONS", parser)) {
            return;
        }
        //获取服务器支持的命令
        _supported_cmd.clear();
        auto public_val = split(parser["Public"],",");
        for(auto &cmd : public_val){
            trim(cmd);
            _supported_cmd.emplace(cmd);
        }
        //发送Describe请求，获取sdp
        sendDescribe();
    };
    sendRtspRequest("OPTIONS", _play_url);
}

void RtspPlayer::sendKeepAlive(){
    _on_response = [](const Parser &parser) {};
    if(_supported_cmd.find("GET_PARAMETER") != _supported_cmd.end()){
        //支持GET_PARAMETER，用此命令保活
        sendRtspRequest("GET_PARAMETER", _content_base);
    }else{
        //不支持GET_PARAMETER，用OPTIONS命令保活
        sendRtspRequest("OPTIONS", _play_url);
    }
}

void RtspPlayer::sendPause(int type , uint32_t seekMS){
    _on_response = std::bind(&RtspPlayer::handleResPAUSE, this, placeholders::_1, type);
    //开启或暂停rtsp
    switch (type){
        case type_pause:
            sendRtspRequest("PAUSE", _content_base);
            break;
        case type_play:
            sendRtspRequest("PLAY", _content_base);
            break;
        case type_seek:
            sendRtspRequest("PLAY", _content_base, {"Range",StrPrinter << "npt=" << setiosflags(ios::fixed) << setprecision(2) << seekMS / 1000.0 << "-"});
            break;
        default:
            WarnL << "unknown type : " << type;
            _on_response = nullptr;
            break;
    }
}

void RtspPlayer::pause(bool pause_flag) {
    sendPause(pause_flag ? type_pause : type_seek, getProgressMilliSecond());
}

void RtspPlayer::handleResPAUSE(const Parser& parser,int type) {
    if (parser.Url() != "200") {
        switch (type) {
            case type_pause:
                WarnL << "Pause failed:" << parser.Url() << " " << parser.Tail() << endl;
                break;
            case type_play:
                WarnL << "Play failed:" << parser.Url() << " " << parser.Tail() << endl;
                onPlayResult_l(SockException(Err_shutdown, StrPrinter << "rtsp play failed:" << parser.Url() << " " << parser.Tail() ), !_play_check_timer);
                break;
            case type_seek:
                WarnL << "Seek failed:" << parser.Url() << " " << parser.Tail() << endl;
                break;
        }
        return;
    }

    if (type == type_pause) {
        //暂停成功！
        _rtp_check_timer.reset();
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
        iSeekTo = (uint32_t)(1000 * atof(strStart.data()));
        DebugL << "seekTo(ms):" << iSeekTo;
    }

    onPlayResult_l(SockException(Err_success, type == type_seek ? "resum rtsp success" : "rtsp play success"), !_play_check_timer);
}

void RtspPlayer::onWholeRtspPacket(Parser &parser) {
    try {
        decltype(_on_response) func;
        _on_response.swap(func);
        if(func){
            func(parser);
        }
        parser.Clear();
    } catch (std::exception &err) {
        //定时器_pPlayTimer为空后表明握手结束了
        onPlayResult_l(SockException(Err_other, err.what()),!_play_check_timer);
    }
}

void RtspPlayer::onRtpPacket(const char *data, size_t len) {
    int trackIdx = -1;
    uint8_t interleaved = data[1];
    if(interleaved %2 == 0){
        trackIdx = getTrackIndexByInterleaved(interleaved);
        handleOneRtp(trackIdx, _sdp_track[trackIdx]->_type, _sdp_track[trackIdx]->_samplerate, (unsigned char *)data + 4, len - 4);
    }else{
        trackIdx = getTrackIndexByInterleaved(interleaved - 1);
        onRtcpPacket(trackIdx, _sdp_track[trackIdx], (unsigned char *) data + 4, len - 4);
    }
}

//此处预留rtcp处理函数
void RtspPlayer::onRtcpPacket(int track_idx, SdpTrack::Ptr &track, unsigned char *data, size_t len){}

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

void RtspPlayer::sendReceiverReport(bool over_tcp, int track_idx){
    static const char s_cname[] = "ZLMediaKitRtsp";
    uint8_t aui8Rtcp[4 + 32 + 10 + sizeof(s_cname) + 1] = {0};
    uint8_t *pui8Rtcp_RR = aui8Rtcp + 4, *pui8Rtcp_SDES = pui8Rtcp_RR + 32;
    auto &track = _sdp_track[track_idx];
    auto &counter = _rtcp_counter[track_idx];

    aui8Rtcp[0] = '$';
    aui8Rtcp[1] = track->_interleaved + 1;
    aui8Rtcp[2] = (sizeof(aui8Rtcp) - 4) >> 8;
    aui8Rtcp[3] = (sizeof(aui8Rtcp) - 4) & 0xFF;

    pui8Rtcp_RR[0] = 0x81;/* 1 report block */
    pui8Rtcp_RR[1] = 0xC9;//RTCP_RR
    pui8Rtcp_RR[2] = 0x00;
    pui8Rtcp_RR[3] = 0x07;/* length in words - 1 */

    auto track_ssrc = track->_ssrc ? track->_ssrc : getSSRC(track_idx);
    // our own SSRC: we use the server's SSRC + 1 to avoid conflicts
    uint32_t ssrc = htonl(track_ssrc + 1);
    memcpy(&pui8Rtcp_RR[4], &ssrc, 4);

    // server SSRC
    ssrc = htonl(track_ssrc);
    memcpy(&pui8Rtcp_RR[8], &ssrc, 4);

    //FIXME: 8 bits of fraction, 24 bits of total packets lost
    pui8Rtcp_RR[12] = 0x00;
    pui8Rtcp_RR[13] = 0x00;
    pui8Rtcp_RR[14] = 0x00;
    pui8Rtcp_RR[15] = 0x00;

    //FIXME: max sequence received
    uint16_t cycleCount = (uint16_t)getCycleCount(track_idx);
    pui8Rtcp_RR[16] = (cycleCount >> 8) & 0xFF;
    pui8Rtcp_RR[17] = cycleCount & 0xFF;
    pui8Rtcp_RR[18] = counter.pktCnt >> 8;
    pui8Rtcp_RR[19] = counter.pktCnt & 0xFF;

    uint32_t jitter = htonl((uint32_t)getJitterSize(track_idx));
    //FIXME: jitter
    memcpy(pui8Rtcp_RR + 20, &jitter, 4);
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

    if (over_tcp) {
        send(obtainBuffer((char *) aui8Rtcp, sizeof(aui8Rtcp)));
    } else if (_rtcp_sock[track_idx]) {
        _rtcp_sock[track_idx]->send((char *) aui8Rtcp + 4, sizeof(aui8Rtcp) - 4);
    }
}

void RtspPlayer::onRtpSorted(const RtpPacket::Ptr &rtppt, int trackidx){
    //统计丢包率
    if (_rtp_seq_start[trackidx] == 0 || rtppt->sequence < _rtp_seq_start[trackidx]) {
        _rtp_seq_start[trackidx] = rtppt->sequence;
        _rtp_recv_count[trackidx] = 0;
    }
    _rtp_recv_count[trackidx] ++;
    _rtp_seq_now[trackidx] = rtppt->sequence;
    _stamp[trackidx] = rtppt->timeStamp;
    //计算相对时间戳
    onRecvRTP_l(rtppt, _sdp_track[trackidx]);
}

float RtspPlayer::getPacketLossRate(TrackType type) const{
    try {
        auto track_idx = getTrackIndexByTrackType(type);
        if (_rtp_seq_now[track_idx] - _rtp_seq_start[track_idx] + 1 == 0) {
            return 0;
        }
        return (float)(1.0f - (double) _rtp_recv_count[track_idx] / (_rtp_seq_now[track_idx] - _rtp_seq_start[track_idx] + 1));
    } catch (...) {
        uint64_t totalRecv = 0;
        uint64_t totalSend = 0;
        for (unsigned int i = 0; i < _sdp_track.size(); i++) {
            totalRecv += _rtp_recv_count[i];
            totalSend += (_rtp_seq_now[i] - _rtp_seq_start[i] + 1);
        }
        if (totalSend == 0) {
            return 0;
        }
        return (float)(1.0f - (double) totalRecv / totalSend);
    }
}

uint32_t RtspPlayer::getProgressMilliSecond() const{
    return MAX(_stamp[0],_stamp[1]);
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
    header.emplace("CSeq",StrPrinter << _cseq_send++);
    header.emplace("User-Agent",SERVER_NAME);

    if(!_session_id.empty()){
        header.emplace("Session", _session_id);
    }

    if(!_realm.empty() && !(*this)[kRtspUser].empty()){
        if(!_md5_nonce.empty()){
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
                encrypted_pwd = MD5((*this)[kRtspUser] + ":" + _realm + ":" + encrypted_pwd).hexdigest();
            }
            auto response = MD5(encrypted_pwd + ":" + _md5_nonce + ":" + MD5(cmd + ":" + url).hexdigest()).hexdigest();
            _StrPrinter printer;
            printer << "Digest ";
            printer << "username=\"" << (*this)[kRtspUser] << "\", ";
            printer << "realm=\"" << _realm << "\", ";
            printer << "nonce=\"" << _md5_nonce << "\", ";
            printer << "uri=\"" << url << "\", ";
            printer << "response=\"" << response << "\"";
            header.emplace("Authorization",printer);
        }else if(!(*this)[kRtspPwdIsMD5].as<bool>()){
            //base64认证
            string authStr = StrPrinter << (*this)[kRtspUser] << ":" << (*this)[kRtspPwd];
            char authStrBase64[1024] = {0};
            av_base64_encode(authStrBase64, sizeof(authStrBase64), (uint8_t *) authStr.data(), (int) authStr.size());
            header.emplace("Authorization",StrPrinter << "Basic " << authStrBase64 );
        }
    }

    _StrPrinter printer;
    printer << cmd << " " << url << " RTSP/1.0\r\n";
    for (auto &pr : header){
        printer << pr.first << ": " << pr.second << "\r\n";
    }
    printer << "\r\n";
    SockSender::send(std::move(printer));
}

void RtspPlayer::onRecvRTP_l(const RtpPacket::Ptr &rtp, const SdpTrack::Ptr &track) {
    _rtp_recv_ticker.resetTime();
    onRecvRTP(rtp, track);

    int track_idx = getTrackIndexByTrackType(rtp->type);
    RtcpCounter &counter = _rtcp_counter[track_idx];
    counter.pktCnt = rtp->sequence;
    auto &ticker = _rtcp_send_ticker[track_idx];
    if (ticker.elapsedTime() > 3 * 1000) {
        //每3秒发送一次心跳，rtcp与rtsp信令轮流心跳，该特性用于兼容issue:642
        if (_send_rtcp) {
            counter.lastTimeStamp = counter.timeStamp;
            //直接保存网络字节序
            memcpy(&counter.timeStamp, rtp->data() + 8, 4);
            if (counter.lastTimeStamp != 0) {
                sendReceiverReport(_rtp_type == Rtsp::RTP_TCP, track_idx);
                ticker.resetTime();
                _send_rtcp = false;
            }
        } else {
            //有些rtsp服务器需要rtcp保活，有些需要发送信令保活
            if (track_idx == 0) {
                //只需要发送一次心跳信令包
                sendKeepAlive();
                ticker.resetTime();
                _send_rtcp = true;
            }
        }
    }
}

void RtspPlayer::onPlayResult_l(const SockException &ex , bool handshake_done) {
    if (ex.getErrCode() == Err_shutdown) {
        //主动shutdown的，不触发回调
        return;
    }

    WarnL << ex.getErrCode() << " " << ex.what();
    if (!handshake_done) {
        //开始播放阶段
        _play_check_timer.reset();
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

    if (!ex) {
        //播放成功，恢复rtp接收超时定时器
        _rtp_recv_ticker.resetTime();
        auto timeoutMS = (*this)[kMediaTimeoutMS].as<uint64_t>();
        weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
        auto lam = [weakSelf, timeoutMS]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            if (strongSelf->_rtp_recv_ticker.elapsedTime() > timeoutMS) {
                //接收rtp媒体数据包超时
                strongSelf->onPlayResult_l(SockException(Err_timeout, "receive rtp timeout"), true);
                return false;
            }
            return true;
        };
        //创建rtp数据接收超时检测定时器
        _rtp_check_timer = std::make_shared<Timer>(timeoutMS / 2000.0f, lam, getPoller());
    } else {
        sendTeardown();
    }
}

int RtspPlayer::getTrackIndexByInterleaved(int interleaved) const {
    for (unsigned int i = 0; i < _sdp_track.size(); i++) {
        if (_sdp_track[i]->_interleaved == interleaved) {
            return i;
        }
    }
    if (_sdp_track.size() == 1) {
        return 0;
    }
    throw SockException(Err_shutdown, StrPrinter << "no such track with interleaved:" << interleaved);
}

int RtspPlayer::getTrackIndexByTrackType(TrackType track_type) const {
    for (unsigned int i = 0; i < _sdp_track.size(); i++) {
        if (_sdp_track[i]->_type == track_type) {
            return i;
        }
    }
    if (_sdp_track.size() == 1) {
        return 0;
    }
    throw SockException(Err_shutdown, StrPrinter << "no such track with type:" << (int) track_type);
}

} /* namespace mediakit */
