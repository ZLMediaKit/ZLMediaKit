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
    CLEAR_ARR(_udp_socks);
    _nonce.clear();
    _realm.clear();
    _track_vec.clear();
    _session_id.clear();
    _content_base.clear();
    _session_id.clear();
    _cseq = 1;
    _publish_timer.reset();
    _beat_timer.reset();
    _rtsp_reader.reset();
    _track_vec.clear();
    _on_res_func = nullptr;
}

void RtspPusher::publish(const string &url_str) {
    RtspUrl url;
    if (!url.parse(url_str)) {
        onPublishResult(SockException(Err_other, StrPrinter << "illegal rtsp url:" << url_str), false);
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

    _url = url_str;
    _rtp_type = (Rtsp::eRtpType) (int) (*this)[kRtpType];
    DebugL << url._url << " " << (url._user.size() ? url._user : "null") << " "
           << (url._passwd.size() ? url._passwd : "null") << " " << _rtp_type;

    weak_ptr<RtspPusher> weak_self = dynamic_pointer_cast<RtspPusher>(shared_from_this());
    float publish_timeout_sec = (*this)[kTimeoutMS].as<int>() / 1000.0;
    _publish_timer.reset(new Timer(publish_timeout_sec, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onPublishResult(SockException(Err_timeout, "publish rtsp timeout"), false);
        return false;
    }, getPoller()));

    if (!(*this)[kNetAdapter].empty()) {
        setNetAdapter((*this)[kNetAdapter]);
    }

    startConnect(url._host, url._port, publish_timeout_sec);
}

void RtspPusher::onPublishResult(const SockException &ex, bool handshake_done) {
    if (ex.getErrCode() == Err_shutdown) {
        //主动shutdown的，不触发回调
        return;
    }
    if (!handshake_done) {
        //播放结果回调
        _publish_timer.reset();
        if (_on_published) {
            _on_published(ex);
        }
    } else {
        //播放成功后异常断开回调
        if (_on_shutdown) {
            _on_shutdown(ex);
        }
    }

    if (ex) {
        sendTeardown();
    }
}

void RtspPusher::onErr(const SockException &ex) {
    //定时器_pPublishTimer为空后表明握手结束了
    onPublishResult(ex, !_publish_timer);
}

void RtspPusher::onConnect(const SockException &err) {
    if (err) {
        onPublishResult(err, false);
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
        onPublishResult(ex, !_publish_timer);
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
    auto &rtp_sock = _udp_socks[track_idx];
    if (!rtp_sock) {
        rtp_sock = createSocket();
        //rtp随机端口
        if (!rtp_sock->bindUdpSock(0, get_local_ip().data())) {
            rtp_sock.reset();
            throw std::runtime_error("open rtp sock failed");
        }
    }
}

void RtspPusher::sendSetup(unsigned int track_idx) {
    _on_res_func = std::bind(&RtspPusher::handleResSetup, this, placeholders::_1, track_idx);
    auto &track = _track_vec[track_idx];
    auto base_url = _content_base + "/" + track->_control_surffix;
    if (track->_control.find("://") != string::npos) {
        base_url = track->_control;
    }
    switch (_rtp_type) {
        case Rtsp::RTP_TCP: {
            sendRtspRequest("SETUP", base_url, {"Transport",
                                                StrPrinter << "RTP/AVP/TCP;unicast;interleaved=" << track->_type * 2
                                                           << "-" << track->_type * 2 + 1});
        }
            break;
        case Rtsp::RTP_UDP: {
            createUdpSockIfNecessary(track_idx);
            int port = _udp_socks[track_idx]->get_local_port();
            sendRtspRequest("SETUP", base_url,
                            {"Transport", StrPrinter << "RTP/AVP;unicast;client_port=" << port << "-" << port + 1});
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
        uint16_t port = atoi(FindField(port_str.data(), NULL, "-").data());
        struct sockaddr_in rtpto;
        rtpto.sin_port = ntohs(port);
        rtpto.sin_family = AF_INET;
        rtpto.sin_addr.s_addr = inet_addr(get_peer_ip().data());
        _udp_socks[track_idx]->setSendPeerAddr((struct sockaddr *) &(rtpto));
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
    _on_res_func = [this](const Parser &parser) {};
    sendRtspRequest("OPTIONS", _content_base);
}

inline void RtspPusher::sendRtpPacket(const RtspMediaSource::RingDataType &pkt) {
    switch (_rtp_type) {
        case Rtsp::RTP_TCP: {
            int i = 0;
            int size = pkt->size();
            setSendFlushFlag(false);
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                if (++i == size) {
                    setSendFlushFlag(true);
                }
                BufferRtp::Ptr buffer(new BufferRtp(rtp));
                send(std::move(buffer));
            });
            break;
        }

        case Rtsp::RTP_UDP: {
            int i = 0;
            int size = pkt->size();
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                int iTrackIndex = getTrackIndexByTrackType(rtp->type);
                auto &pSock = _udp_socks[iTrackIndex];
                if (!pSock) {
                    shutdown(SockException(Err_shutdown, "udp sock not opened yet"));
                    return;
                }

                BufferRtp::Ptr buffer(new BufferRtp(rtp, 4));
                pSock->send(std::move(buffer), nullptr, 0, ++i == size);
            });
            break;
        }
        default : break;
    }
}

inline int RtspPusher::getTrackIndexByTrackType(TrackType type) {
    for (unsigned int i = 0; i < _track_vec.size(); i++) {
        if (type == _track_vec[i]->_type) {
            return i;
        }
    }
    if (_track_vec.size() == 1) {
        return 0;
    }
    throw SockException(Err_shutdown, StrPrinter << "no such track with type:" << (int) type);
}

void RtspPusher::sendRecord() {
    _on_res_func = [this](const Parser &parser) {
        auto src = _push_src.lock();
        if (!src) {
            throw std::runtime_error("the media source was released");
        }

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
                strong_self->onPublishResult(SockException(Err_other, "媒体源被释放"), !strong_self->_publish_timer);
            }
        });
        if (_rtp_type != Rtsp::RTP_TCP) {
            /////////////////////////心跳/////////////////////////////////
            _beat_timer.reset(new Timer((*this)[kBeatIntervalMS].as<int>() / 1000.0, [weak_self]() {
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    return false;
                }
                strong_self->sendOptions();
                return true;
            }, getPoller()));
        }
        onPublishResult(SockException(Err_success, "success"), false);
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
    header.emplace("User-Agent", SERVER_NAME);

    if (!_session_id.empty()) {
        header.emplace("Session", _session_id);
    }

    if (!_realm.empty() && !(*this)[kRtspUser].empty()) {
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
            string encrypted_pwd = (*this)[kRtspPwd];
            if (!(*this)[kRtspPwdIsMD5].as<bool>()) {
                encrypted_pwd = MD5((*this)[kRtspUser] + ":" + _realm + ":" + encrypted_pwd).hexdigest();
            }
            auto response = MD5(encrypted_pwd + ":" + _nonce + ":" + MD5(cmd + ":" + url).hexdigest()).hexdigest();
            _StrPrinter printer;
            printer << "Digest ";
            printer << "username=\"" << (*this)[kRtspUser] << "\", ";
            printer << "realm=\"" << _realm << "\", ";
            printer << "nonce=\"" << _nonce << "\", ";
            printer << "uri=\"" << url << "\", ";
            printer << "response=\"" << response << "\"";
            header.emplace("Authorization", printer);
        } else if (!(*this)[kRtspPwdIsMD5].as<bool>()) {
            //base64认证
            string authStr = StrPrinter << (*this)[kRtspUser] << ":" << (*this)[kRtspPwd];
            char authStrBase64[1024] = {0};
            av_base64_encode(authStrBase64, sizeof(authStrBase64), (uint8_t *) authStr.data(), authStr.size());
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