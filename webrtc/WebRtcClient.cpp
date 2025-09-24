/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Network/TcpClient.h"
#include "Common/config.h"
#include "Common/Parser.h"
#include "WebRtcClient.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

// # WebRTCUrl format
// ## whep/whip over http sfu: webrtc://server_host:server_port/{{app}}/{{streamid}}
// ## whep/whip over https sfu: webrtcs://server_host:server_port/{{app}}/{{streamid}}
// ## websocket p2p: webrtc://{{signaling_server_host}}:{{signaling_server_port}}/{{app}}/{{streamid}}?room_id={{peer_room_id}}
// ## websockets p2p: webrtcs://{{signaling_server_host}}:{{signaling_server_port}}/{{app}}/{{streamid}}?room_id={{peer_room_id}}
void WebRTCUrl::parse(const string &strUrl, bool isPlayer) {
    DebugL << "url: " << strUrl;
    _full_url = strUrl;
    auto url = strUrl;
    auto pos = url.find("?");
    if (pos != string::npos) {
        _params = url.substr(pos + 1);
        url.erase(pos);
    }

    auto schema_pos = url.find("://");
    if (schema_pos != string::npos) {
        auto schema = url.substr(0, schema_pos);
        _is_ssl = strcasecmp(schema.data(), "webrtcs") == 0;
    } else {
        schema_pos = -3;
    }
    // set default port
    _port = _is_ssl ? 443 : 80;
    auto split_vec = split(url.substr(schema_pos + 3), "/");
    if (split_vec.size() > 0) {
        splitUrl(split_vec[0], _host, _port);
        _vhost = _host;
        if (_vhost == "localhost" || isIP(_vhost.data())) {
            // 如果访问的是localhost或ip，那么则为默认虚拟主机
            _vhost = DEFAULT_VHOST;
        }
    }
    if (split_vec.size() > 1) {
        _app = split_vec[1];
    }
    if (split_vec.size() > 2) {
        string stream_id;
        for (size_t i = 2; i < split_vec.size(); ++i) {
            stream_id.append(split_vec[i] + "/");
        }
        if (stream_id.back() == '/') {
            stream_id.pop_back();
        }
        _stream = stream_id;
    }

    // for vhost
    auto kv = Parser::parseArgs(_params);
    auto it = kv.find(VHOST_KEY);
    if (it != kv.end()) {
        _vhost = it->second;
    }

    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    if (!enableVhost || _vhost.empty()) {
        // 如果关闭虚拟主机或者虚拟主机为空，则设置虚拟主机为默认
        _vhost = DEFAULT_VHOST;
    }

    // for peer_room_id
    it = kv.find("peer_room_id");
    if (it != kv.end()) {
        _peer_room_id = it->second;
    }

    it = kv.find("signaling_protocols");
    if (it != kv.end()) {
        _signaling_protocols = (WebRtcTransport::SignalingProtocols)(stoi(it->second));
    }

    auto suffix = _host + ":" + to_string(_port);
    suffix += (isPlayer ? "/index/api/whep" : "/index/api/whip");
    suffix += "?app=" + _app + "&stream=" + _stream;
    if (!_params.empty()) {
        suffix += "&" + _params;
    }
    if (_is_ssl) {
        _negotiate_url = StrPrinter << "https://" << suffix << endl;
    } else {
        _negotiate_url = StrPrinter << "http://" << suffix << endl;
    }
}

////////////  WebRtcClient //////////////////////////

WebRtcClient::WebRtcClient(toolkit::EventPoller::Ptr poller) {
    DebugL;
    _poller = poller ? std::move(poller) : EventPollerPool::Instance().getPoller();
}

WebRtcClient::~WebRtcClient() {
    doBye();
    DebugL;
}

void WebRtcClient::startConnect() {
    DebugL;
    doNegotiate();
}

void WebRtcClient::connectivityCheck() {
    DebugL;
    return _transport->connectivityCheckForSFU();
}

void WebRtcClient::onNegotiateFinish() {
    DebugL;
    _is_negotiate_finished = true;
    if (WebRtcTransport::SignalingProtocols::WEBSOCKET == _url._signaling_protocols) {
        // P2P模式需要gathering candidates
        gatheringCandidate(_peer->getIceServer());
    } else if (WebRtcTransport::SignalingProtocols::WHEP_WHIP == _url._signaling_protocols) {
        // SFU模式不会存在IP不通的情况， answer中就携带了candidates, 直接进行connectivityCheck
        connectivityCheck();
    }
}

void WebRtcClient::doNegotiate() {
    DebugL;
    switch (_url._signaling_protocols) {
        case WebRtcTransport::SignalingProtocols::WHEP_WHIP: return doNegotiateWhepOrWhip();
        case WebRtcTransport::SignalingProtocols::WEBSOCKET: return doNegotiateWebsocket();
        default: throw std::invalid_argument(StrPrinter << "not support signaling_protocols: " << (int)_url._signaling_protocols);
    }
}

void WebRtcClient::doNegotiateWhepOrWhip() {
    DebugL << _url._negotiate_url;

    weak_ptr<WebRtcClient> weak_self = static_pointer_cast<WebRtcClient>(shared_from_this());
    auto offer_sdp = _transport->createOfferSdp();
    DebugL << "send offer:\n" << offer_sdp;

    _negotiate = make_shared<HttpRequester>();
    _negotiate->setMethod("POST");
    _negotiate->addHeader("Content-Type", "application/sdp");
    _negotiate->setBody(std::move(offer_sdp));
    _negotiate->startRequester(_url._negotiate_url, [weak_self](const toolkit::SockException &ex, const Parser &response) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        if (ex) {
            WarnL << "network err:" << ex;
            strong_self->onResult(ex);
            return;
        }

        DebugL << "status:" << response.status() << "\r\n"
               << "Location:\r\n"
               << response.getHeader()["Location"] << "\r\nrecv answer:\n"
               << response.content();
        strong_self->_url._delete_url = response.getHeader()["Location"];
        if ("201" != response.status()) {
            strong_self->onResult(SockException(Err_other, response.content()));
            return;
        }
        strong_self->_transport->setAnswerSdp(response.content());
        strong_self->onNegotiateFinish();
    }, getTimeOutSec());
}

void WebRtcClient::doNegotiateWebsocket() {
    DebugL;
#if 0
    //TODO: 当前暂将每一路呼叫都使用一个独立的peer_connection,不复用
    _peer = getWebrtcRoomKeeper(_url._host, _url._port);
    if (_peer) {
        checkIn();
        return;
    }
#endif

    // 未注册的,先增加注册流程，并在此次播放结束后注销
    InfoL << (StrPrinter << "register to signaling server " << _url._host << "::" << _url._port << " first");
    auto room_id = "ringing_" + makeRandStr(16);
    _peer = make_shared<WebRtcSignalingPeer>(_url._host, _url._port, _url._is_ssl, room_id);
    weak_ptr<WebRtcClient> weak_self = static_pointer_cast<WebRtcClient>(shared_from_this());
    _peer->setOnConnect([weak_self](const SockException &ex) {
        if (auto strong_self = weak_self.lock()) {
            if (ex) {
                strong_self->onResult(ex);
                return;
            }

            auto cb = [weak_self](const SockException &ex, const string &key) {
                if (auto strong_self = weak_self.lock()) {
                    strong_self->checkIn();
                }
            };
            strong_self->_peer->regist(cb);
        }
    });
    _peer->connect();
}

void WebRtcClient::checkIn() {
    DebugL;
    weak_ptr<WebRtcClient> weak_self = static_pointer_cast<WebRtcClient>(shared_from_this());
    auto tuple = MediaTuple(_url._vhost, _url._app, _url._stream, _url._params);
    _peer->checkIn(_url._peer_room_id, tuple, _transport->getIdentifier(), _transport->createOfferSdp(), isPlayer(),
                   [weak_self](const SockException &ex, const std::string &answer) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        if (ex) {
            WarnL << "network err:" << ex;
            strong_self->onResult(ex);
            return;
        }

        strong_self->_transport->setAnswerSdp(answer);
        strong_self->onNegotiateFinish();
    }, getTimeOutSec());
}

void WebRtcClient::checkOut() {
    DebugL;
    auto tuple = MediaTuple(_url._vhost, _url._app, _url._stream);
    if (_peer) {
        _peer->checkOut(_url._peer_room_id);
        _peer->unregist([](const SockException &ex) {});
    }
}

void WebRtcClient::candidate(const std::string &candidate, const std::string &ufrag, const std::string &pwd) {
    _peer->candidate(_transport->getIdentifier(), candidate, ufrag, pwd);
}

void WebRtcClient::gatheringCandidate(IceServerInfo::Ptr ice_server) {
    DebugL;
    std::weak_ptr<WebRtcClient> weak_self = std::static_pointer_cast<WebRtcClient>(shared_from_this());
    _transport->gatheringCandidate(ice_server, [weak_self](const std::string& transport_identifier, const std::string& candidate,
        const std::string& ufrag, const std::string& pwd) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->candidate(candidate, ufrag, pwd);
    });
}

void WebRtcClient::doBye() {
    DebugL;
    if (!_is_negotiate_finished) {
        return;
    }

    switch (_url._signaling_protocols) {
        case WebRtcTransport::SignalingProtocols::WHEP_WHIP: return doByeWhepOrWhip();
        case WebRtcTransport::SignalingProtocols::WEBSOCKET: return checkOut();
        default: throw std::invalid_argument(StrPrinter << "not support signaling_protocols: " << (int)_url._signaling_protocols);
    }
    _is_negotiate_finished = false;
}

void WebRtcClient::doByeWhepOrWhip() {
    DebugL;
    if (!_negotiate) {
        return;
    }
    _negotiate->setMethod("DELETE");
    _negotiate->setBody("");
    _negotiate->startRequester(_url._delete_url, [](const toolkit::SockException &ex, const Parser &response) {
        if (ex) {
            WarnL << "network err:" << ex;
            return;
        }
        DebugL << "status:" << response.status();
    }, getTimeOutSec());
}

float WebRtcClient::getTimeOutSec() {
    GET_CONFIG(uint32_t, timeout, Rtc::kTimeOutSec);
    if (timeout <= 0) {
        WarnL << "config rtc. " << Rtc::kTimeOutSec << ": " << timeout << " not vaild";
        return 5.0;
    }
    return (float)timeout;
}

} /* namespace mediakit */
