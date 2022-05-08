/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Util/MD5.h"
#include "Util/base64.h"
#include "RtspPusher.h"
#include "RtspSession.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

RtspPusher::RtspPusher(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src) : TcpClient(poller) {
    _push_src = src;
}

RtspPusher::~RtspPusher() {
    teardown();
    DebugL << endl;
}

void RtspPusher::sendTeardown(){
    if (alive()) {
        if (!_content_base.empty()) {
            sendRtspRequest("TEARDOWN", _content_base);
        }
        shutdown(SockException(Err_shutdown, "teardown"));
    }
}

void RtspPusher::teardown() {
    sendTeardown();
    reset();
    CLEAR_ARR(_rtp_sock);
    CLEAR_ARR(_rtcp_sock);
    _nonce.clear();
    _realm.clear();
    _track_vec.clear();
    _session_id.clear();
    _content_base.clear();
    _cseq = 1;
    _publish_timer.reset();
    _beat_timer.reset();
    _rtsp_reader.reset();
    _track_vec.clear();
    _on_res_func = nullptr;
}

void RtspPusher::publish(const string &url_str) {
    RtspUrl url;
    try {
        url.parse(url_str);
    } catch (std::exception &ex) {
        onPublishResult_l(SockException(Err_other, StrPrinter << "illegal rtsp url:" << ex.what()), false);
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

    _url = url_str;
    _rtp_type = (Rtsp::eRtpType) (int) (*this)[Client::kRtpType];
    DebugL << url._url << " " << (url._user.size() ? url._user : "null") << " "
           << (url._passwd.size() ? url._passwd : "null") << " " << _rtp_type;

    weak_ptr<RtspPusher> weak_self = dynamic_pointer_cast<RtspPusher>(shared_from_this());
    float publish_timeout_sec = (*this)[Client::kTimeoutMS].as<int>() / 1000.0f;
    _publish_timer.reset(new Timer(publish_timeout_sec, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onPublishResult_l(SockException(Err_timeout, "publish rtsp timeout"), false);
        return false;
    }, getPoller()));

    if (!(*this)[Client::kNetAdapter].empty()) {
        setNetAdapter((*this)[Client::kNetAdapter]);
    }

    startConnect(url._host, url._port, publish_timeout_sec);
}

void RtspPusher::onPublishResult_l(const SockException &ex, bool handshake_done) {
    DebugL << ex.what();
    if (ex.getErrCode() == Err_shutdown) {
        //主动shutdown的，不触发回调
        return;
    }
    if (!handshake_done) {
        //播放结果回调
        _publish_timer.reset();
        onPublishResult(ex);
    } else {
        //播放成功后异常断开回调
        onShutdown(ex);
    }

    if (ex) {
        sendTeardown();
    }
}

void RtspPusher::onErr(const SockException &ex) {
    //定时器_pPublishTimer为空后表明握手结束了
    onPublishResult_l(ex, !_publish_timer);
}

void RtspPusher::onConnect(const SockException &err) {
    if (err) {
        onPublishResult_l(err, false);
        return;
    }
    sendAnnounce();
}

void RtspPusher::onRecv(const Buffer::Ptr &buf){
    try {
        input(buf->data(), buf->size());
    } catch (exception &e) {
        SockException ex(Err_other, e.what());
        //定时器_pPublishTimer为空后表明握手结束了
        onPublishResult_l(ex, !_publish_timer);
    }
}

void RtspPusher::onWholeRtspPacket(Parser &parser) {
    decltype(_on_res_func) func;
    _on_res_func.swap(func);
    if (func) {
        func(parser);
    }
    parser.Clear();
}

void RtspPusher::onRtpPacket(const char *data, size_t len) {
    int trackIdx = -1;
    uint8_t interleaved = data[1];
    if (interleaved % 2 != 0) {
        trackIdx = getTrackIndexByInterleaved(interleaved - 1);
        onRtcpPacket(trackIdx, _track_vec[trackIdx], (uint8_t *) data + RtpPacket::kRtpTcpHeaderSize, len - RtpPacket::kRtpTcpHeaderSize);
    }
}

void RtspPusher::onRtcpPacket(int track_idx, SdpTrack::Ptr &track, uint8_t *data, size_t len){
    auto rtcp_arr = RtcpHeader::loadFromBytes((char *) data, len);
    for (auto &rtcp : rtcp_arr) {
        _rtcp_context[track_idx]->onRtcp(rtcp);
    }
}

void RtspPusher::sendAnnounce() {
    auto src = _push_src.lock();
    if (!src) {
        throw std::runtime_error("the media source was released");
    }
    //解析sdp
    _sdp_parser.load(src->getSdp());
    _track_vec = _sdp_parser.getAvailableTrack();
    if (_track_vec.empty()) {
        throw std::runtime_error("无有效的Sdp Track");
    }
    _rtcp_context.clear();
    for (auto &track : _track_vec) {
        _rtcp_context.emplace_back(std::make_shared<RtcpContextForSend>());
    }
    _on_res_func = std::bind(&RtspPusher::handleResAnnounce, this, placeholders::_1);
    sendRtspRequest("ANNOUNCE", _url, {}, src->getSdp());
}

void RtspPusher::handleResAnnounce(const Parser &parser) {
    string authInfo = parser["WWW-Authenticate"];
    //发送DESCRIBE命令后的回复
    if ((parser.Url() == "401") && handleAuthenticationFailure(authInfo)) {
        sendAnnounce();
        return;
    }
    if (parser.Url() == "302") {
        auto newUrl = parser["Location"];
        if (newUrl.empty()) {
            throw std::runtime_error("未找到Location字段(跳转url)");
        }
        publish(newUrl);
        return;
    }
    if (parser.Url() != "200") {
        throw std::runtime_error(StrPrinter << "ANNOUNCE:" << parser.Url() << " " << parser.Tail());
    }
    _content_base = parser["Content-Base"];

    if (_content_base.empty()) {
        _content_base = _url;
    }
    if (_content_base.back() == '/') {
        _content_base.pop_back();
    }
    
    _session_id = parser["Session"];

    sendSetup(0);
}

bool RtspPusher::handleAuthenticationFailure(const string &params_str) {
    if (!_realm.empty()) {
        //已经认证过了
        return false;
    }

    char *realm = new char[params_str.size()];
    char *nonce = new char[params_str.size()];
    char *stale = new char[params_str.size()];
    onceToken token(nullptr, [&]() {
        delete[] realm;
        delete[] nonce;
        delete[] stale;
    });

    if (sscanf(params_str.data(), "Digest realm=\"%[^\"]\", nonce=\"%[^\"]\", stale=%[a-zA-Z]", realm, nonce, stale) == 3) {
        _realm = (const char *) realm;
        _nonce = (const char *) nonce;
        return true;
    }
    if (sscanf(params_str.data(), "Digest realm=\"%[^\"]\", nonce=\"%[^\"]\"", realm, nonce) == 2) {
        _realm = (const char *) realm;
        _nonce = (const char *) nonce;
        return true;
    }
    if (sscanf(params_str.data(), "Basic realm=\"%[^\"]\"", realm) == 1) {
        _realm = (const char *) realm;
        return true;
    }
    return false;
}

//有必要的情况下创建udp端口
void RtspPusher::createUdpSockIfNecessary(int track_idx){
    auto &rtpSockRef = _rtp_sock[track_idx];
    auto &rtcpSockRef = _rtcp_sock[track_idx];
    if (!rtpSockRef || !rtcpSockRef) {
        std::pair<Socket::Ptr, Socket::Ptr> pr = std::make_pair(createSocket(), createSocket());
        makeSockPair(pr, get_local_ip());
        rtpSockRef = pr.first;
        rtcpSockRef = pr.second;
    }
}

void RtspPusher::sendSetup(unsigned int track_idx) {
    _on_res_func = std::bind(&RtspPusher::handleResSetup, this, placeholders::_1, track_idx);
    auto &track = _track_vec[track_idx];
    auto control_url = track->getControlUrl(_content_base);
    switch (_rtp_type) {
        case Rtsp::RTP_TCP: {
            sendRtspRequest("SETUP", control_url, {"Transport",
                                                   StrPrinter << "RTP/AVP/TCP;unicast;interleaved=" << track->_type * 2
                                                           << "-" << track->_type * 2 + 1 << ";mode=record"});
        }
            break;
        case Rtsp::RTP_UDP: {
            createUdpSockIfNecessary(track_idx);
            int port = _rtp_sock[track_idx]->get_local_port();
            sendRtspRequest("SETUP", control_url,
                            {"Transport", StrPrinter << "RTP/AVP;unicast;client_port=" << port << "-" << port + 1 << ";mode=record"});
        }
            break;
        default:
            break;
    }
}

void RtspPusher::handleResSetup(const Parser &parser, unsigned int track_idx) {
    if (parser.Url() != "200") {
        throw std::runtime_error(StrPrinter << "SETUP:" << parser.Url() << " " << parser.Tail() << endl);
    }
    if (track_idx == 0) {
        _session_id = parser["Session"];
        _session_id.append(";");
        _session_id = FindField(_session_id.data(), nullptr, ";");
    }

    auto transport = parser["Transport"];
    if (transport.find("TCP") != string::npos || transport.find("interleaved") != string::npos) {
        _rtp_type = Rtsp::RTP_TCP;
        string interleaved = FindField(FindField((transport + ";").data(), "interleaved=", ";").data(), NULL, "-");
        _track_vec[track_idx]->_interleaved = atoi(interleaved.data());
    } else if (transport.find("multicast") != string::npos) {
        throw std::runtime_error("SETUP rtsp pusher can not support multicast!");
    } else {
        _rtp_type = Rtsp::RTP_UDP;
        createUdpSockIfNecessary(track_idx);
        const char *strPos = "server_port=";
        auto port_str = FindField((transport + ";").data(), strPos, ";");
        uint16_t rtp_port = atoi(FindField(port_str.data(), NULL, "-").data());
        uint16_t rtcp_port = atoi(FindField(port_str.data(), "-", NULL).data());
        auto &rtp_sock = _rtp_sock[track_idx];
        auto &rtcp_sock = _rtcp_sock[track_idx];

        auto rtpto = SockUtil::make_sockaddr(get_peer_ip().data(), rtp_port);
        //设置rtp发送目标，为后续发送rtp做准备
        rtp_sock->bindPeerAddr((struct sockaddr *) &(rtpto));

        //设置rtcp发送目标，为后续发送rtcp做准备
        auto rtcpto = SockUtil::make_sockaddr(get_peer_ip().data(), rtcp_port);
        rtcp_sock->bindPeerAddr((struct sockaddr *)&(rtcpto));

        auto peer_ip = get_peer_ip();
        weak_ptr<RtspPusher> weakSelf = dynamic_pointer_cast<RtspPusher>(shared_from_this());
        if(rtcp_sock) {
            //设置rtcp over udp接收回调处理函数
            rtcp_sock->setOnRead([peer_ip, track_idx, weakSelf](const Buffer::Ptr &buf, struct sockaddr *addr , int addr_len) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                if (SockUtil::inet_ntoa(addr) != peer_ip) {
                    WarnL << "收到其他地址的rtcp数据:" << SockUtil::inet_ntoa(addr);
                    return;
                }
                strongSelf->onRtcpPacket(track_idx, strongSelf->_track_vec[track_idx], (uint8_t *) buf->data(), buf->size());
            });
        }
    }

    RtspSplitter::enableRecvRtp(_rtp_type == Rtsp::RTP_TCP);

    if (track_idx < _track_vec.size() - 1) {
        //需要继续发送SETUP命令
        sendSetup(track_idx + 1);
        return;
    }

    sendRecord();
}

void RtspPusher::sendOptions() {
    _on_res_func = [](const Parser &parser) {};
    sendRtspRequest("OPTIONS", _content_base);
}

void RtspPusher::updateRtcpContext(const RtpPacket::Ptr &rtp){
    int track_index = getTrackIndexByTrackType(rtp->type);
    auto &ticker = _rtcp_send_ticker[track_index];
    auto &rtcp_ctx = _rtcp_context[track_index];
    rtcp_ctx->onRtp(rtp->getSeq(), rtp->getStamp(), rtp->ntp_stamp, rtp->sample_rate, rtp->size() - RtpPacket::kRtpTcpHeaderSize);

    //send rtcp every 5 second
    if (ticker.elapsedTime() > 5 * 1000) {
        ticker.resetTime();
        static auto send_rtcp = [](RtspPusher *thiz, int index, Buffer::Ptr ptr) {
            if (thiz->_rtp_type == Rtsp::RTP_TCP) {
                auto &track = thiz->_track_vec[index];
                thiz->send(makeRtpOverTcpPrefix((uint16_t) (ptr->size()), track->_interleaved + 1));
                thiz->send(std::move(ptr));
            } else {
                thiz->_rtcp_sock[index]->send(std::move(ptr));
            }
        };

        auto ssrc = rtp->getSSRC();
        auto rtcp = rtcp_ctx->createRtcpSR(ssrc + 1);
        auto rtcp_sdes = RtcpSdes::create({kServerName});
        rtcp_sdes->chunks.type = (uint8_t) SdesType::RTCP_SDES_CNAME;
        rtcp_sdes->chunks.ssrc = htonl(ssrc);
        send_rtcp(this, track_index, std::move(rtcp));
        send_rtcp(this, track_index, RtcpHeader::toBuffer(rtcp_sdes));
    }
}

void RtspPusher::sendRtpPacket(const RtspMediaSource::RingDataType &pkt) {
    switch (_rtp_type) {
        case Rtsp::RTP_TCP: {
            size_t i = 0;
            auto size = pkt->size();
            setSendFlushFlag(false);
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                updateRtcpContext(rtp);
                if (++i == size) {
                    setSendFlushFlag(true);
                }
                send(rtp);
            });
            break;
        }

        case Rtsp::RTP_UDP: {
            size_t i = 0;
            auto size = pkt->size();
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                updateRtcpContext(rtp);
                int iTrackIndex = getTrackIndexByTrackType(rtp->type);
                auto &pSock = _rtp_sock[iTrackIndex];
                if (!pSock) {
                    shutdown(SockException(Err_shutdown, "udp sock not opened yet"));
                    return;
                }

                pSock->send(std::make_shared<BufferRtp>(rtp, RtpPacket::kRtpTcpHeaderSize), nullptr, 0, ++i == size);
            });
            break;
        }
        default : break;
    }
}

int RtspPusher::getTrackIndexByInterleaved(int interleaved) const {
    for (size_t i = 0; i < _track_vec.size(); ++i) {
        if (_track_vec[i]->_interleaved == interleaved) {
            return i;
        }
    }
    if (_track_vec.size() == 1) {
        return 0;
    }
    throw SockException(Err_shutdown, StrPrinter << "no such track with interleaved:" << interleaved);
}

int RtspPusher::getTrackIndexByTrackType(TrackType type) const {
    for (size_t i = 0; i < _track_vec.size(); ++i) {
        if (type == _track_vec[i]->_type) {
            return i;
        }
    }
    if (_track_vec.size() == 1) {
        return 0;
    }
    throw SockException(Err_shutdown, StrPrinter << "no such track with type:" << getTrackString(type));
}

void RtspPusher::sendRecord() {
    _on_res_func = [this](const Parser &parser) {
        auto src = _push_src.lock();
        if (!src) {
            throw std::runtime_error("the media source was released");
        }

        src->pause(false);
        _rtsp_reader = src->getRing()->attach(getPoller());
        weak_ptr<RtspPusher> weak_self = dynamic_pointer_cast<RtspPusher>(shared_from_this());
        _rtsp_reader->setReadCB([weak_self](const RtspMediaSource::RingDataType &pkt) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->sendRtpPacket(pkt);
        });
        _rtsp_reader->setDetachCB([weak_self]() {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                strong_self->onPublishResult_l(SockException(Err_other, "媒体源被释放"), !strong_self->_publish_timer);
            }
        });
        if (_rtp_type != Rtsp::RTP_TCP) {
            /////////////////////////心跳/////////////////////////////////
            _beat_timer.reset(new Timer((*this)[Client::kBeatIntervalMS].as<int>() / 1000.0f, [weak_self]() {
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    return false;
                }
                strong_self->sendOptions();
                return true;
            }, getPoller()));
        }
        onPublishResult_l(SockException(Err_success, "success"), false);
        //提升发送性能
        setSocketFlags();
    };
    sendRtspRequest("RECORD", _content_base, {"Range", "npt=0.000-"});
}

void RtspPusher::setSocketFlags(){
    GET_CONFIG(int, merge_write_ms, General::kMergeWriteMS);
    if (merge_write_ms > 0) {
        //提高发送性能
        setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
        SockUtil::setNoDelay(getSock()->rawFD(), false);
    }
}

void RtspPusher::sendRtspRequest(const string &cmd, const string &url, const std::initializer_list<string> &header,const string &sdp ) {
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
    sendRtspRequest(cmd, url, header_map, sdp);
}
void RtspPusher::sendRtspRequest(const string &cmd, const string &url,const StrCaseMap &header_const,const string &sdp ) {
    auto header = header_const;
    header.emplace("CSeq", StrPrinter << _cseq++);
    header.emplace("User-Agent", kServerName);

    if (!_session_id.empty()) {
        header.emplace("Session", _session_id);
    }

    if (!_realm.empty() && !(*this)[Client::kRtspUser].empty()) {
        if (!_nonce.empty()) {
            //MD5认证
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
            auto response = MD5(encrypted_pwd + ":" + _nonce + ":" + MD5(cmd + ":" + url).hexdigest()).hexdigest();
            _StrPrinter printer;
            printer << "Digest ";
            printer << "username=\"" << (*this)[Client::kRtspUser] << "\", ";
            printer << "realm=\"" << _realm << "\", ";
            printer << "nonce=\"" << _nonce << "\", ";
            printer << "uri=\"" << url << "\", ";
            printer << "response=\"" << response << "\"";
            header.emplace("Authorization", printer);
        } else if (!(*this)[Client::kRtspPwdIsMD5].as<bool>()) {
            //base64认证
            string authStr = StrPrinter << (*this)[Client::kRtspUser] << ":" << (*this)[Client::kRtspPwd];
            char authStrBase64[1024] = {0};
            av_base64_encode(authStrBase64, sizeof(authStrBase64), (uint8_t *) authStr.data(), (int)authStr.size());
            header.emplace("Authorization", StrPrinter << "Basic " << authStrBase64);
        }
    }

    if (!sdp.empty()) {
        header.emplace("Content-Length", StrPrinter << sdp.size());
        header.emplace("Content-Type", "application/sdp");
    }

    _StrPrinter printer;
    printer << cmd << " " << url << " RTSP/1.0\r\n";
    for (auto &pr : header) {
        printer << pr.first << ": " << pr.second << "\r\n";
    }

    printer << "\r\n";

    if (!sdp.empty()) {
        printer << sdp;
    }
    SockSender::send(std::move(printer));
}


} /* namespace mediakit */
