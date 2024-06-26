﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtspPlayer.h"
#include "Common/config.h"
#include "Rtcp/Rtcp.h"
#include "Rtcp/RtcpContext.h"
#include "RtspDemuxer.h"
#include "RtspMediaSource.h"
#include "RtspPlayerImp.h"
#include "Util/MD5.h"
#include "Util/base64.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>

using namespace toolkit;
using namespace std;

namespace mediakit {

enum PlayType { type_play = 0, type_pause, type_seek, type_speed };
enum class BeatType : uint32_t { both = 0, rtcp, cmd  };

RtspPlayer::RtspPlayer(const EventPoller::Ptr &poller)
    : TcpClient(poller) {}

RtspPlayer::~RtspPlayer(void) {
    DebugL;
}

void RtspPlayer::sendTeardown() {
    if (alive()) {
        if (!_control_url.empty()) {
            sendRtspRequest("TEARDOWN", _control_url);
        }
        shutdown(SockException(Err_shutdown, "teardown"));
    }
}

void RtspPlayer::teardown() {
    sendTeardown();
    _md5_nonce.clear();
    _realm.clear();
    _sdp_track.clear();
    _session_id.clear();
    _content_base.clear();
    RtpReceiver::clear();
    _rtcp_context.clear();

    CLEAR_ARR(_rtp_sock);
    CLEAR_ARR(_rtcp_sock);

    _play_check_timer.reset();
    _rtp_check_timer.reset();
    _cseq_send = 1;
    _on_response = nullptr;
}

void RtspPlayer::play(const string &strUrl) {
    RtspUrl url;
    try {
        url.parse(strUrl);
    } catch (std::exception &ex) {
        onPlayResult_l(SockException(Err_other, StrPrinter << "illegal rtsp url:" << ex.what()), false);
        return;
    }

    teardown();

    if (url._user.size()) {
        (*this)[Client::kRtspUser] = url._user;
    }
    if (url._passwd.size()) {
        (*this)[Client::kRtspPwd] = url._passwd;
        (*this)[Client::kRtspPwdIsMD5] = false;
    }

    _play_url = url._url;
    _rtp_type = (Rtsp::eRtpType)(int)(*this)[Client::kRtpType];
    _beat_type = (*this)[Client::kRtspBeatType].as<int>();
    _beat_interval_ms = (*this)[Client::kBeatIntervalMS].as<int>();
    _speed = (*this)[Client::kRtspSpeed].as<float>();
    DebugL << url._url << " " << (url._user.size() ? url._user : "null") << " " << (url._passwd.size() ? url._passwd : "null") << " " << _rtp_type;

    weak_ptr<RtspPlayer> weakSelf = static_pointer_cast<RtspPlayer>(shared_from_this());
    float playTimeOutSec = (*this)[Client::kTimeoutMS].as<int>() / 1000.0f;
    _play_check_timer.reset(new Timer(
        playTimeOutSec,
        [weakSelf]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            strongSelf->onPlayResult_l(SockException(Err_timeout, "play rtsp timeout"), false);
            return false;
        },
        getPoller()));

    if (!(*this)[Client::kNetAdapter].empty()) {
        setNetAdapter((*this)[Client::kNetAdapter]);
    }
    startConnect(url._host, url._port, playTimeOutSec);
}

void RtspPlayer::onConnect(const SockException &err) {
    if (err.getErrCode() != Err_success) {
        onPlayResult_l(err, false);
        return;
    }
    sendOptions();
}

void RtspPlayer::onRecv(const Buffer::Ptr &buf) {
    if (_benchmark_mode && !_play_check_timer) {
        // 在性能测试模式下，如果rtsp握手完毕后，不再解析rtp包
        _rtp_recv_ticker.resetTime();
        return;
    }
    try {
        input(buf->data(), buf->size());
    } catch (exception &e) {
        SockException ex(Err_other, e.what());
        onPlayResult_l(ex, !_play_check_timer);
    }
}

void RtspPlayer::onError(const SockException &ex) {
    // 定时器_pPlayTimer为空后表明握手结束了
    onPlayResult_l(ex, !_play_check_timer);
}

// from live555
bool RtspPlayer::handleAuthenticationFailure(const string &paramsStr) {
    if (!_realm.empty()) {
        // 已经认证过了
        return false;
    }

    char *realm = new char[paramsStr.size()];
    char *nonce = new char[paramsStr.size()];
    char *stale = new char[paramsStr.size()];
    onceToken token(nullptr, [&]() {
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

bool RtspPlayer::handleResponse(const string &cmd, const Parser &parser) {
    string authInfo = parser["WWW-Authenticate"];
    // 发送DESCRIBE命令后的回复
    if ((parser.status() == "401") && handleAuthenticationFailure(authInfo)) {
        sendOptions();
        return false;
    }
    if (parser.status() == "302" || parser.status() == "301") {
        auto newUrl = parser["Location"];
        if (newUrl.empty()) {
            throw std::runtime_error("未找到Location字段(跳转url)");
        }
        play(newUrl);
        return false;
    }
    if (parser.status() != "200") {
        throw std::runtime_error(StrPrinter << cmd << ":" << parser.status() << " " << parser.statusStr() << endl);
    }
    return true;
}

void RtspPlayer::handleResDESCRIBE(const Parser &parser) {
    if (!handleResponse("DESCRIBE", parser)) {
        return;
    }
    _content_base = parser["Content-Base"];
    if (_content_base.empty()) {
        _content_base = _play_url;
    }
    if (_content_base.back() == '/') {
        _content_base.pop_back();
    }

    // 解析sdp
    SdpParser sdpParser(parser.content());

    _control_url = sdpParser.getControlUrl(_content_base);

    string sdp;
    auto play_track = (TrackType)((int)(*this)[Client::kPlayTrack] - 1);
    if (play_track != TrackInvalid) {
        auto track = sdpParser.getTrack(play_track);
        _sdp_track.emplace_back(track);
        auto title_track = sdpParser.getTrack(TrackTitle);
        sdp = (title_track ? title_track->toString() : "") + track->toString();
    } else {
        _sdp_track = sdpParser.getAvailableTrack();
        sdp = sdpParser.toString();
    }

    if (_sdp_track.empty()) {
        throw std::runtime_error("无有效的Sdp Track");
    }
    if (!onCheckSDP(sdp)) {
        throw std::runtime_error("onCheckSDP faied");
    }
    _rtcp_context.clear();
    for (auto &track : _sdp_track) {
        if (track->_pt != 0xff) {
            setPayloadType(_rtcp_context.size(), track->_pt);
        }
        _rtcp_context.emplace_back(std::make_shared<RtcpContextForRecv>());
    }
    sendSetup(0);
}

// 有必要的情况下创建udp端口
void RtspPlayer::createUdpSockIfNecessary(int track_idx) {
    auto &rtpSockRef = _rtp_sock[track_idx];
    auto &rtcpSockRef = _rtcp_sock[track_idx];
    if (!rtpSockRef || !rtcpSockRef) {
        std::pair<Socket::Ptr, Socket::Ptr> pr = std::make_pair(createSocket(), createSocket());
        makeSockPair(pr, get_local_ip());
        rtpSockRef = pr.first;
        rtcpSockRef = pr.second;
    }
}

// 发送SETUP命令
void RtspPlayer::sendSetup(unsigned int track_idx) {
    _on_response = std::bind(&RtspPlayer::handleResSETUP, this, placeholders::_1, track_idx);
    auto &track = _sdp_track[track_idx];
    auto control_url = track->getControlUrl(_content_base);
    switch (_rtp_type) {
        case Rtsp::RTP_TCP: {
            sendRtspRequest(
                "SETUP", control_url, { "Transport", StrPrinter << "RTP/AVP/TCP;unicast;interleaved=" << track->_type * 2 << "-" << track->_type * 2 + 1 });
        } break;
        case Rtsp::RTP_MULTICAST: {
            sendRtspRequest("SETUP", control_url, { "Transport", "RTP/AVP;multicast" });
        } break;
        case Rtsp::RTP_UDP: {
            createUdpSockIfNecessary(track_idx);
            sendRtspRequest(
                "SETUP", control_url,
                { "Transport",
                  StrPrinter << "RTP/AVP;unicast;client_port=" << _rtp_sock[track_idx]->get_local_port() << "-" << _rtcp_sock[track_idx]->get_local_port() });
        } break;
        default: break;
    }
}

void RtspPlayer::handleResSETUP(const Parser &parser, unsigned int track_idx) {
    if (parser.status() != "200") {
        throw std::runtime_error(StrPrinter << "SETUP:" << parser.status() << " " << parser.statusStr() << endl);
    }
    if (track_idx == 0) {
        _session_id = parser["Session"];
        _session_id.append(";");
        _session_id = findSubString(_session_id.data(), nullptr, ";");
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
    if (!ssrc.empty()) {
        sscanf(ssrc.data(), "%x", &_sdp_track[track_idx]->_ssrc);
    } else {
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
            // udp组播
            auto multiAddr = transport_map["destination"];
            pRtpSockRef = createSocket();
            // 目前组播仅支持ipv4
            if (!pRtpSockRef->bindUdpSock(rtp_port, "0.0.0.0")) {
                pRtpSockRef.reset();
                throw std::runtime_error("open udp sock err");
            }
            auto fd = pRtpSockRef->rawFD();
            if (-1 == SockUtil::joinMultiAddrFilter(fd, multiAddr.data(), get_peer_ip().data(), get_local_ip().data())) {
                SockUtil::joinMultiAddr(fd, multiAddr.data(), get_local_ip().data());
            }

            // 设置rtcp发送端口
            pRtcpSockRef = createSocket();
            // 目前组播仅支持ipv4
            if (!pRtcpSockRef->bindUdpSock(0, "0.0.0.0")) {
                // 分配端口失败
                throw runtime_error("open udp socket failed");
            }

            // 设置发送地址和发送端口
            auto dst = SockUtil::make_sockaddr(get_peer_ip().data(), rtcp_port);
            pRtcpSockRef->bindPeerAddr((struct sockaddr *)&(dst));
        } else {
            createUdpSockIfNecessary(track_idx);
            // udp单播
            auto dst = SockUtil::make_sockaddr(get_peer_ip().data(), rtp_port);
            pRtpSockRef->bindPeerAddr((struct sockaddr *)&(dst));
            // 发送rtp打洞包
            pRtpSockRef->send("\xce\xfa\xed\xfe", 4);

            dst = SockUtil::make_sockaddr(get_peer_ip().data(), rtcp_port);
            // 设置rtcp发送目标，为后续发送rtcp做准备
            pRtcpSockRef->bindPeerAddr((struct sockaddr *)&(dst));
        }

        auto peer_ip = get_peer_ip();
        weak_ptr<RtspPlayer> weakSelf = static_pointer_cast<RtspPlayer>(shared_from_this());
        // 设置rtp over udp接收回调处理函数
        pRtpSockRef->setOnRead([peer_ip, track_idx, weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            if (SockUtil::inet_ntoa(addr) != peer_ip) {
                WarnL << "收到其他地址的rtp数据:" << SockUtil::inet_ntoa(addr);
                return;
            }
            strongSelf->handleOneRtp(
                track_idx, strongSelf->_sdp_track[track_idx]->_type, strongSelf->_sdp_track[track_idx]->_samplerate, (uint8_t *)buf->data(), buf->size());
        });

        if (pRtcpSockRef) {
            // 设置rtcp over udp接收回调处理函数
            pRtcpSockRef->setOnRead([peer_ip, track_idx, weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                if (SockUtil::inet_ntoa(addr) != peer_ip) {
                    WarnL << "收到其他地址的rtcp数据:" << SockUtil::inet_ntoa(addr);
                    return;
                }
                strongSelf->onRtcpPacket(track_idx, strongSelf->_sdp_track[track_idx], (uint8_t *)buf->data(), buf->size());
            });
        }
    }

    if (track_idx < _sdp_track.size() - 1) {
        // 需要继续发送SETUP命令
        sendSetup(track_idx + 1);
        return;
    }
    // 所有setup命令发送完毕
    // 发送play命令
    if (_speed==0.0f) {
        sendPause(type_play, 0);
    } else {
        sendPause(type_speed, 0);
    }
   
}

void RtspPlayer::sendDescribe() {
    // 发送DESCRIBE命令后处理函数:handleResDESCRIBE
    _on_response = std::bind(&RtspPlayer::handleResDESCRIBE, this, placeholders::_1);
    sendRtspRequest("DESCRIBE", _play_url, { "Accept", "application/sdp" });
}

void RtspPlayer::sendOptions() {
    _on_response = [this](const Parser &parser) {
        if (!handleResponse("OPTIONS", parser)) {
            return;
        }
        // 获取服务器支持的命令
        _supported_cmd.clear();
        auto public_val = split(parser["Public"], ",");
        for (auto &cmd : public_val) {
            trim(cmd);
            _supported_cmd.emplace(cmd);
        }
        // 发送Describe请求，获取sdp
        sendDescribe();
    };
    sendRtspRequest("OPTIONS", _play_url);
}

void RtspPlayer::sendKeepAlive() {
    _on_response = [](const Parser &parser) {};
    if (_supported_cmd.find("GET_PARAMETER") != _supported_cmd.end()) {
        // 支持GET_PARAMETER，用此命令保活
        sendRtspRequest("GET_PARAMETER", _control_url);
    } else {
        // 不支持GET_PARAMETER，用OPTIONS命令保活
        sendRtspRequest("OPTIONS", _play_url);
    }
}

void RtspPlayer::sendPause(int type, uint32_t seekMS) {
    _on_response = std::bind(&RtspPlayer::handleResPAUSE, this, placeholders::_1, type);
    // 开启或暂停rtsp
    switch (type) {
        case type_pause: sendRtspRequest("PAUSE", _control_url, {}); break;
        case type_play:
            // sendRtspRequest("PLAY", _content_base);
            // break;
        case type_seek:
            sendRtspRequest("PLAY", _control_url, { "Range", StrPrinter << "npt=" << setiosflags(ios::fixed) << setprecision(2) << seekMS / 1000.0 << "-" });
            break;
        case type_speed:
            speed(_speed);
            break;
        default:
            WarnL << "unknown type : " << type;
            _on_response = nullptr;
            break;
    }
}

void RtspPlayer::pause(bool bPause) {
    sendPause(bPause ? type_pause : type_seek, getProgressMilliSecond());
}

void RtspPlayer::speed(float speed) {
    sendRtspRequest("PLAY", _control_url, { "Scale", StrPrinter << speed });
}

void RtspPlayer::handleResPAUSE(const Parser &parser, int type) {
    if (parser.status() != "200") {
        switch (type) {
            case type_pause: WarnL << "Pause failed:" << parser.status() << " " << parser.statusStr(); break;
            case type_play:
                WarnL << "Play failed:" << parser.status() << " " << parser.statusStr();
                onPlayResult_l(SockException(Err_other, StrPrinter << "rtsp play failed:" << parser.status() << " " << parser.statusStr()), !_play_check_timer);
                break;
            case type_seek: WarnL << "Seek failed:" << parser.status() << " " << parser.statusStr(); break;
        }
        return;
    }

    if (type == type_pause) {
        // 暂停成功！
        _rtp_check_timer.reset();
        return;
    }

    // play或seek成功
    uint32_t iSeekTo = 0;
    // 修正时间轴
    auto strRange = parser["Range"];
    if (strRange.size()) {
        auto strStart = findSubString(strRange.data(), "npt=", "-");
        if (strStart == "now") {
            strStart = "0";
        }
        iSeekTo = (uint32_t)(1000 * atof(strStart.data()));
        DebugL << "seekTo(ms):" << iSeekTo;
    }

    onPlayResult_l(SockException(Err_success, type == type_seek ? "resume rtsp success" : "rtsp play success"), !_play_check_timer);
}

void RtspPlayer::onWholeRtspPacket(Parser &parser) {
    if (!start_with(parser.method(), "RTSP")) {
        // 不是rtsp回复，忽略
        WarnL << "Not rtsp response: " << parser.method();
        return;
    }
    try {
        decltype(_on_response) func;
        _on_response.swap(func);
        if (func) {
            func(parser);
        }
        parser.clear();
    } catch (std::exception &err) {
        // 定时器_pPlayTimer为空后表明握手结束了
        onPlayResult_l(SockException(Err_other, err.what()), !_play_check_timer);
    }
}

void RtspPlayer::onRtpPacket(const char *data, size_t len) {
    int trackIdx = -1;
    uint8_t interleaved = data[1];
    if (interleaved % 2 == 0) {
        trackIdx = getTrackIndexByInterleaved(interleaved);
        if (trackIdx == -1) {
            return;
        }
        handleOneRtp(
            trackIdx, _sdp_track[trackIdx]->_type, _sdp_track[trackIdx]->_samplerate, (uint8_t *)data + RtpPacket::kRtpTcpHeaderSize,
            len - RtpPacket::kRtpTcpHeaderSize);
    } else {
        trackIdx = getTrackIndexByInterleaved(interleaved - 1);
        if (trackIdx == -1) {
            return;
        }
        onRtcpPacket(trackIdx, _sdp_track[trackIdx], (uint8_t *)data + RtpPacket::kRtpTcpHeaderSize, len - RtpPacket::kRtpTcpHeaderSize);
    }
}

// 此处预留rtcp处理函数
void RtspPlayer::onRtcpPacket(int track_idx, SdpTrack::Ptr &track, uint8_t *data, size_t len) {
    auto rtcp_arr = RtcpHeader::loadFromBytes((char *)data, len);
    for (auto &rtcp : rtcp_arr) {
        _rtcp_context[track_idx]->onRtcp(rtcp);
        if ((RtcpType)rtcp->pt == RtcpType::RTCP_SR) {
            auto sr = (RtcpSR *)(rtcp);
            // 设置rtp时间戳与ntp时间戳的对应关系
            setNtpStamp(track_idx, sr->rtpts, sr->getNtpUnixStampMS());
        }
    }
}

void RtspPlayer::onRtpSorted(RtpPacket::Ptr rtppt, int trackidx) {
    _stamp[trackidx] = rtppt->getStampMS();
    _rtp_recv_ticker.resetTime();
    onRecvRTP(std::move(rtppt), _sdp_track[trackidx]);
}

float RtspPlayer::getPacketLossRate(TrackType type) const {
    size_t lost = 0, expected = 0;
    try {
        auto track_idx = getTrackIndexByTrackType(type);
        if (_rtcp_context.empty()) {
            return 0;
        }
        auto ctx = _rtcp_context[track_idx];
        lost = ctx->getLost();
        expected = ctx->getExpectedPackets();
    } catch (...) {
        for (auto &ctx : _rtcp_context) {
            lost += ctx->getLost();
            expected += ctx->getExpectedPackets();
        }
    }
    if (!expected) {
        return 0;
    }
    return (float)(double(lost) / double(expected));
}

uint32_t RtspPlayer::getProgressMilliSecond() const {
    return MAX(_stamp[0], _stamp[1]);
}

void RtspPlayer::seekToMilliSecond(uint32_t ms) {
    sendPause(type_seek, ms);
}

void RtspPlayer::sendRtspRequest(const string &cmd, const string &url, const std::initializer_list<string> &header) {
    string key;
    StrCaseMap header_map;
    int i = 0;
    for (auto &val : header) {
        if (++i % 2 == 0) {
            header_map.emplace(key, val);
        } else {
            key = val;
        }
    }

    sendRtspRequest(cmd, url, header_map);
}

void RtspPlayer::sendRtspRequest(const string &cmd, const string &url, const StrCaseMap &header_const) {
    auto header = header_const;
    header.emplace("CSeq", StrPrinter << _cseq_send++);
    header.emplace("User-Agent", kServerName);

    if (!_session_id.empty()) {
        header.emplace("Session", _session_id);
    }

    if (!_realm.empty() && !(*this)[Client::kRtspUser].empty()) {
        if (!_md5_nonce.empty()) {
            // MD5认证
            /*
            response计算方法如下：
            RTSP客户端应该使用username + password并计算response如下:
            (1)当password为MD5编码,则
                response = md5( password:nonce:md5(public_method:url)  );
            (2)当password为ANSI字符串,则
                response= md5( md5(username:realm:password):nonce:md5(public_method:url) );
             */
            string encrypted_pwd = (*this)[Client::kRtspPwd];
            if (!(*this)[Client::kRtspPwdIsMD5].as<bool>()) {
                encrypted_pwd = MD5((*this)[Client::kRtspUser] + ":" + _realm + ":" + encrypted_pwd).hexdigest();
            }
            auto response = MD5(encrypted_pwd + ":" + _md5_nonce + ":" + MD5(cmd + ":" + url).hexdigest()).hexdigest();
            _StrPrinter printer;
            printer << "Digest ";
            printer << "username=\"" << (*this)[Client::kRtspUser] << "\", ";
            printer << "realm=\"" << _realm << "\", ";
            printer << "nonce=\"" << _md5_nonce << "\", ";
            printer << "uri=\"" << url << "\", ";
            printer << "response=\"" << response << "\"";
            header.emplace("Authorization", printer);
        } else if (!(*this)[Client::kRtspPwdIsMD5].as<bool>()) {
            // base64认证
            auto authStrBase64 = encodeBase64((*this)[Client::kRtspUser] + ":" + (*this)[Client::kRtspPwd]);
            header.emplace("Authorization", StrPrinter << "Basic " << authStrBase64);
        }
    }

    _StrPrinter printer;
    printer << cmd << " " << url << " RTSP/1.0\r\n";

    TraceL << cmd << " "<< url;
    for (auto &pr : header) {
        printer << pr.first << ": " << pr.second << "\r\n";
    }
    printer << "\r\n";
    SockSender::send(std::move(printer));
}

void RtspPlayer::onBeforeRtpSorted(const RtpPacket::Ptr &rtp, int track_idx) {
    auto &rtcp_ctx = _rtcp_context[track_idx];
    rtcp_ctx->onRtp(rtp->getSeq(), rtp->getStamp(), rtp->ntp_stamp, rtp->sample_rate, rtp->size() - RtpPacket::kRtpTcpHeaderSize);

    auto &ticker = _rtcp_send_ticker[track_idx];
    if (ticker.elapsedTime() < _beat_interval_ms) {
        // 心跳时间未到
        return;
    }

    // 有些rtsp服务器需要rtcp保活，有些需要发送信令保活; rtcp与rtsp信令轮流心跳，该特性用于兼容issue:#642
    auto &rtcp_flag = _send_rtcp[track_idx];
    ticker.resetTime();

    switch ((BeatType)_beat_type) {
        case BeatType::cmd: rtcp_flag = false; break;
        case BeatType::rtcp: rtcp_flag = true; break;
        case BeatType::both:
        default: rtcp_flag = !rtcp_flag; break;
    }

    // 发送信令保活
    if (!rtcp_flag) {
        if (track_idx == 0) {
            // 两个track无需同时触发发送信令保活
            sendKeepAlive();
        }
        return;
    }

    // 发送rtcp
    static auto send_rtcp = [](RtspPlayer *thiz, int index, Buffer::Ptr ptr) {
        if (thiz->_rtp_type == Rtsp::RTP_TCP) {
            auto &track = thiz->_sdp_track[index];
            thiz->send(makeRtpOverTcpPrefix((uint16_t)(ptr->size()), track->_interleaved + 1));
            thiz->send(std::move(ptr));
        } else {
            thiz->_rtcp_sock[index]->send(std::move(ptr));
        }
    };

    auto ssrc = rtp->getSSRC();
    auto rtcp = rtcp_ctx->createRtcpRR(ssrc + 1, ssrc);
    auto rtcp_sdes = RtcpSdes::create({ kServerName });
    rtcp_sdes->chunks.type = (uint8_t)SdesType::RTCP_SDES_CNAME;
    rtcp_sdes->chunks.ssrc = htonl(ssrc);
    send_rtcp(this, track_idx, std::move(rtcp));
    send_rtcp(this, track_idx, RtcpHeader::toBuffer(rtcp_sdes));
}

void RtspPlayer::onPlayResult_l(const SockException &ex, bool handshake_done) {
    if (ex.getErrCode() == Err_shutdown) {
        // 主动shutdown的，不触发回调
        return;
    }

    WarnL << ex.getErrCode() << " " << ex.what();
    if (!handshake_done) {
        // 开始播放阶段
        _play_check_timer.reset();
        onPlayResult(ex);
        // 是否为性能测试模式
        _benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();
    } else if (ex) {
        // 播放成功后异常断开回调
        onShutdown(ex);
    } else {
        // 恢复播放
        onResume();
    }

    if (!ex) {
        // 播放成功，恢复rtp接收超时定时器
        _rtp_recv_ticker.resetTime();
        auto timeoutMS = (*this)[Client::kMediaTimeoutMS].as<uint64_t>();
        weak_ptr<RtspPlayer> weakSelf = static_pointer_cast<RtspPlayer>(shared_from_this());
        auto lam = [weakSelf, timeoutMS]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            if (strongSelf->_rtp_recv_ticker.elapsedTime() > timeoutMS) {
                // 接收rtp媒体数据包超时
                strongSelf->onPlayResult_l(SockException(Err_timeout, "receive rtp timeout"), true);
                return false;
            }
            return true;
        };
        // 创建rtp数据接收超时检测定时器
        _rtp_check_timer = std::make_shared<Timer>(timeoutMS / 2000.0f, lam, getPoller());
    } else {
        sendTeardown();
    }
}

int RtspPlayer::getTrackIndexByInterleaved(int interleaved) const {
    for (size_t i = 0; i < _sdp_track.size(); ++i) {
        if (_sdp_track[i]->_interleaved == interleaved) {
            return i;
        }
    }
    if (_sdp_track.size() == 1) {
        return 0;
    }
    WarnL << "no such track with interleaved:" << interleaved;
    return -1;
}

int RtspPlayer::getTrackIndexByTrackType(TrackType track_type) const {
    for (size_t i = 0; i < _sdp_track.size(); ++i) {
        if (_sdp_track[i]->_type == track_type) {
            return i;
        }
    }
    if (_sdp_track.size() == 1) {
        return 0;
    }
    throw SockException(Err_other, StrPrinter << "no such track with type:" << getTrackString(track_type));
}

///////////////////////////////////////////////////
// RtspPlayerImp
float RtspPlayerImp::getDuration() const {
    return _demuxer ? _demuxer->getDuration() : 0;
}

void RtspPlayerImp::onPlayResult(const toolkit::SockException &ex) {
    if (!(*this)[Client::kWaitTrackReady].as<bool>() || ex) {
        Super::onPlayResult(ex);
        return;
    }
}

void RtspPlayerImp::addTrackCompleted() {
    if ((*this)[Client::kWaitTrackReady].as<bool>()) {
        Super::onPlayResult(toolkit::SockException(toolkit::Err_success, "play success"));
    }
}

std::vector<Track::Ptr> RtspPlayerImp::getTracks(bool ready /*= true*/) const {
    return _demuxer ? _demuxer->getTracks(ready) : Super::getTracks(ready);
}

bool RtspPlayerImp::onCheckSDP(const std::string &sdp) {
    _rtsp_media_src = std::dynamic_pointer_cast<RtspMediaSource>(_media_src);
    if (_rtsp_media_src) {
        _rtsp_media_src->setSdp(sdp);
    }
    _demuxer = std::make_shared<RtspDemuxer>();
    _demuxer->setTrackListener(this, (*this)[Client::kWaitTrackReady].as<bool>());
    _demuxer->loadSdp(sdp);
    return true;
}

void RtspPlayerImp::onRecvRTP(RtpPacket::Ptr rtp, const SdpTrack::Ptr &track) {
    // rtp解复用时可以判断是否为关键帧起始位置
    auto key_pos = _demuxer->inputRtp(rtp);
    if (_rtsp_media_src) {
        _rtsp_media_src->onWrite(std::move(rtp), key_pos);
    }
}

} /* namespace mediakit */
