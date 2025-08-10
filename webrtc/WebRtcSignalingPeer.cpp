/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcSignalingPeer.h"
#include "WebRtcSignalingMsg.h"
#include "Util/util.h"
#include "Common/config.h"
#include "json/value.h"

using namespace std;

namespace mediakit {
using namespace Rtc;

//注册到的信令服务器列表
//不允许注册到同一个服务器地址
static ServiceController<WebRtcSignalingPeer> s_room_keepers;

static inline string getRoomKeepersKey(const string &host, uint16_t &port) {
    return host + ":" + std::to_string(port);
}

void addWebrtcRoomKeeper(const string &host, uint16_t port, const std::string& room_id,
                         const function<void(const SockException &ex, const string &key)> &cb) {
    DebugL;
    auto key = getRoomKeepersKey(host, port);
    if (s_room_keepers.find(key)) {
        //已经发起注册了
        cb(SockException(Err_success), key);
        TraceL;
        return;
    }

    auto peer = s_room_keepers.make(key, host, port, room_id);
    peer->setOnShutdown([key] (const SockException &ex) {
        InfoL << "webrtc peer shutdown, key: " << key << ", " << ex.what();
        s_room_keepers.erase(key);
    });

    peer->setOnConnect([peer, cb] (const SockException &ex) {
        peer->regist(cb);
    });
    peer->connect();
    return;
};

void delWebrtcRoomKeeper(const std::string &key, const std::function<void(const SockException &ex)> &cb) {
    auto peer = s_room_keepers.find(key);
    if (!peer) {
        return cb(SockException(Err_other, "room_key not exist"));
    }
    peer->unregist(cb);
    s_room_keepers.erase(key);
    return;
}

void listWebrtcRoomKeepers(const std::function<void(const std::string& key, const WebRtcSignalingPeer::Ptr& p)> &cb) {
    s_room_keepers.for_each(cb);
    return;
}

Json::Value ToJson(const WebRtcSignalingPeer::Ptr& p) {
    return p->makeInfoJson();
}

WebRtcSignalingPeer::Ptr getWebrtcRoomKeeper(const string &host, uint16_t port) {
    return s_room_keepers.find(getRoomKeepersKey(host, port));
}

////////////  WebRtcSignalingPeer //////////////////////////

WebRtcSignalingPeer::WebRtcSignalingPeer(const std::string &host, uint16_t port, const std::string& room_id, const EventPoller::Ptr &poller) 
: WebSocketClient<TcpClient>(poller), _room_id(room_id) {
    TraceL;
    //TODO: not support wss now
    _ws_url = StrPrinter << "ws://" + host << ":" << port << "/signaling";
    _room_key = getRoomKeepersKey(host, port);
}

WebRtcSignalingPeer::~WebRtcSignalingPeer() {
    DebugL << "room_id: " << _room_id;
}

void WebRtcSignalingPeer::connect() {
    DebugL;
    startWebSocket(_ws_url);
}

void WebRtcSignalingPeer::regist(const function<void(const SockException &ex, const string &key)>& cb) {
    DebugL;

    getPoller()->async([=] () {
        sendRegisterRequest(cb);
    });
    return;
}

void WebRtcSignalingPeer::unregist(const function<void(const SockException &ex)> cb) {
    DebugL;
    getPoller()->async([=] () {
        auto trggier = ([cb](const SockException &ex, std::string msg) {
            cb(ex);
        });
        sendUnregisterRequest(trggier);
    });
    return;
}

void WebRtcSignalingPeer::checkIn(const std::string& peer_room_id, const MediaTuple &tuple, const std::string& identifier,
                                  const std::string& offer, bool is_play, 
                                  const function<void(const SockException &ex, const std::string& answer)> cb, float timeout_sec) {
    DebugL;
    getPoller()->async([=] () {
        TraceL;
        auto guest_id = _room_id + "_" + makeRandStr(16);
        _tours.emplace(peer_room_id, std::make_pair(guest_id, identifier));
        auto trigger = ([this, cb, peer_room_id] (const SockException &ex, const std::string& msg) { 
            if (ex) {
                this->_tours.erase(peer_room_id);
            }
            return cb(ex, msg);
        });
        sendCallRequest(peer_room_id, guest_id, tuple, offer, is_play, cb);
    });
}

void WebRtcSignalingPeer::checkOut(const std::string& peer_room_id) {
    DebugL;
    getPoller()->async([=] () {
        TraceL;
        auto it = _tours.find(peer_room_id);
        if (it == _tours.end()) {
            return;
        }
        auto guest_id = it->first;

        sendByeIndication(peer_room_id, guest_id);
        _tours.erase(peer_room_id);
    });
}


void WebRtcSignalingPeer::candidate(const std::string& transport_identifier, const std::string& candidate, const std::string& ice_ufrag, const std::string& ice_pwd) {
    getPoller()->async([=] () {
        sendCandidateIndication(transport_identifier, candidate, ice_ufrag, ice_pwd);
    });
}

void WebRtcSignalingPeer::processOffer(SIGNALING_MSG_ARGS, WebRtcInterface &transport) {
    try {
        auto sdp = transport.getAnswerSdp((const std::string )allArgs[SDP_KEY]);
        auto tuple = MediaTuple(allArgs[CALL_VHOST_KEY], allArgs[CALL_APP_KEY], allArgs[CALL_STREAM_KEY]);
        answer(allArgs[GUEST_ID_KEY], tuple, transport.getIdentifier(), sdp, allArgs[TYPE_KEY] == TYPE_VALUE_PLAY, allArgs[TRANSACTION_ID_KEY]);

        std::weak_ptr<WebRtcSignalingPeer> weak_self = std::static_pointer_cast<WebRtcSignalingPeer>(shared_from_this());
        transport.gatheringCandidate(_ice_server, [weak_self](const std::string& transport_identifier, const std::string& candidate,
            const std::string& ufrag, const std::string pwd) 
        {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->candidate(transport_identifier, candidate, ufrag, pwd);
            return;
        });
    } catch (std::exception &ex) {
        Json::Value body;
        body[METHOD_KEY]   = allArgs[METHOD_KEY];
        body[ROOM_ID_KEY]  = allArgs[ROOM_ID_KEY];
        body[GUEST_ID_KEY] = allArgs[GUEST_ID_KEY];
        body[CALL_VHOST_KEY] = allArgs[CALL_VHOST_KEY];
        body[CALL_APP_KEY] = allArgs[CALL_APP_KEY];
        body[CALL_STREAM_KEY] = allArgs[CALL_STREAM_KEY];
        body[TYPE_KEY] = allArgs[TYPE_KEY];
        sendRefusesResponse(body, allArgs[TRANSACTION_ID_KEY], ex.what());
    }
    return;
}

void WebRtcSignalingPeer::answer(const std::string& guest_id, const MediaTuple &tuple, const std::string& identifier, const std::string& sdp, bool is_play, const std::string& transaction_id) {
    _peer_guests.emplace(guest_id, identifier);
    sendCallAccept(guest_id, tuple, sdp, is_play, transaction_id);
}

void WebRtcSignalingPeer::setOnConnect(function<void(const SockException &ex)> cb) {
    _on_connect = cb ? std::move(cb) : [](const SockException &) {};
}

void WebRtcSignalingPeer::onConnect(const SockException &ex) {
    TraceL;
    if (_on_connect) {
        return _on_connect(ex);
    }
    return;
}

void WebRtcSignalingPeer::setOnShutdown(function<void(const SockException &ex)> cb) {
    _on_shutdown = cb ? std::move(cb) : [](const SockException &) {};
}

void WebRtcSignalingPeer::onShutdown(const SockException &ex) {
    TraceL;
    if (_on_shutdown) {
        return _on_shutdown(ex);
    }
    return;
};

void WebRtcSignalingPeer::onRecv(const Buffer::Ptr &buffer) {
    TraceL << "recv msg:\r\n" << buffer->data();

    Json::Value args;
    Json::Reader reader;
    reader.parse(buffer->data(), args);
    Parser parser;
    HttpAllArgs<decltype(args)> allArgs(parser, args);

    CHECK_ARGS(METHOD_KEY, TRANSACTION_ID_KEY);

    using MsgHandler = void (WebRtcSignalingPeer::*)(SIGNALING_MSG_ARGS);
    static std::unordered_map<std::pair<std::string /*class*/, std::string /*method*/>, MsgHandler, ClassMethodHash> s_msg_handlers;

    static onceToken token([]() {
        s_msg_handlers.emplace(std::make_pair(CLASS_VALUE_ACCEPT, METHOD_VALUE_REGISTER), &WebRtcSignalingPeer::handleRegisterAccept);
        s_msg_handlers.emplace(std::make_pair(CLASS_VALUE_REJECT, METHOD_VALUE_REGISTER), &WebRtcSignalingPeer::handleRegisterReject);
        s_msg_handlers.emplace(std::make_pair(CLASS_VALUE_ACCEPT, METHOD_VALUE_UNREGISTER), &WebRtcSignalingPeer::handleUnregisterAccept);
        s_msg_handlers.emplace(std::make_pair(CLASS_VALUE_REJECT, METHOD_VALUE_UNREGISTER), &WebRtcSignalingPeer::handleUnregisterReject);

        s_msg_handlers.emplace(std::make_pair(CLASS_VALUE_REQUEST, METHOD_VALUE_CALL), &WebRtcSignalingPeer::handleCallRequest);
        s_msg_handlers.emplace(std::make_pair(CLASS_VALUE_ACCEPT, METHOD_VALUE_CALL), &WebRtcSignalingPeer::handleCallAccept);
        s_msg_handlers.emplace(std::make_pair(CLASS_VALUE_REJECT, METHOD_VALUE_CALL), &WebRtcSignalingPeer::handleCallReject);

        s_msg_handlers.emplace(std::make_pair(CLASS_VALUE_INDICATION, METHOD_VALUE_CANDIDATE), &WebRtcSignalingPeer::handleCandidateIndication);
        s_msg_handlers.emplace(std::make_pair(CLASS_VALUE_INDICATION, METHOD_VALUE_BYE), &WebRtcSignalingPeer::handleByeIndication);
    });

    auto it = s_msg_handlers.find(std::make_pair(allArgs[CLASS_KEY], allArgs[METHOD_KEY]));
    if (it == s_msg_handlers.end()) {
        WarnL << " not support class: "<< allArgs[CLASS_KEY] << ", method: " << allArgs[METHOD_KEY] << ", ignore";
        return;
    }
    return (this->*(it->second))(allArgs);

    WarnL << " not process msg, method: " << allArgs[METHOD_KEY] << ", transcation_id: " << allArgs[TRANSACTION_ID_KEY];
    return;
}

void WebRtcSignalingPeer::onError(const SockException &err) {
    WarnL << "room_id: " << _room_id;
    s_room_keepers.erase(_room_key);
    //除非对端显式的发送了注销执行,否则因为网络异常导致的会话中断，不影响已经进行通信的webrtc会话,仅作移除
}

bool WebRtcSignalingPeer::responseFilter(SIGNALING_MSG_ARGS, ResponseTrigger& trigger) {
    if (allArgs[CLASS_KEY] != CLASS_VALUE_ACCEPT && allArgs[CLASS_KEY] != CLASS_VALUE_REJECT) {
        return false;
    }

    for (auto it : _response_list) {
        auto transcation_id = it.first;

        //mismatch transcation_id
        if (transcation_id != allArgs[TRANSACTION_ID_KEY] && transcation_id != TRANSACTION_ID_ANY) {
            continue;
        }
        auto handle = it.second;

        auto method = std::get<1>(handle);
        if (allArgs[METHOD_KEY] != method) {
            WarnL << "recv response method: " << allArgs[METHOD_KEY] << " mismatch request method: " << method;
            return false;
        }

        trigger = std::get<2>(handle);
        _response_list.erase(transcation_id);
        return true;
    }
    return  false;
}

void WebRtcSignalingPeer::sendRegisterRequest(ResponseTrigger trigger) {
    TraceL;
    Json::Value body;
    body[CLASS_KEY]   = CLASS_VALUE_REQUEST;
    body[METHOD_KEY]  = METHOD_VALUE_REGISTER;
    body[ROOM_ID_KEY] = getRoomId();
    sendRequest(body, trigger);
    return;
}

void WebRtcSignalingPeer::handleRegisterAccept(SIGNALING_MSG_ARGS) {
    TraceL;
    ResponseTrigger trigger;
    if (!responseFilter(allArgs, trigger)) {
        return;
    }

    auto jsonArgs = allArgs.getArgs();
    auto ice_servers = jsonArgs[ICE_SERVERS_KEY];
    if (ice_servers.type() != Json::ValueType::arrayValue) {
        _StrPrinter msg;
        msg << "illegal \"" << ICE_SERVERS_KEY << "\" point";
        WarnL << msg;
        trigger(SockException(Err_other, msg), getRoomKey());
        return;
    }

    if (ice_servers.empty()) {
        _StrPrinter msg;
        msg << "no ice server found in \"" << ICE_SERVERS_KEY << "\" point";
        WarnL << msg;
        trigger(SockException(Err_other, msg), getRoomKey());
        return;
    }

    for (auto ice_server: ice_servers) {
        //only support 1 ice_server now
        auto url = ice_server[URL_KEY].asString();
        _ice_server = std::make_shared<RTC::IceServerInfo>(url);
        _ice_server->_ufrag = ice_server[UFRAG_KEY].asString();
        _ice_server->_pwd = ice_server[PWD_KEY].asString();
    }

    trigger(SockException(Err_success), getRoomKey());
    return;
}

void WebRtcSignalingPeer::handleRegisterReject(SIGNALING_MSG_ARGS) {
    TraceL;
    ResponseTrigger trigger;
    if (!responseFilter(allArgs, trigger)) {
        return;
    }

    auto ex = SockException(Err_other, StrPrinter << "register refuses by server, reason: " << allArgs[REASON_KEY]);
    trigger(ex, getRoomKey());
    onShutdown(ex);
    return;
}

void WebRtcSignalingPeer::sendUnregisterRequest(ResponseTrigger trigger) {
    TraceL;
    Json::Value body;
    body[CLASS_KEY]   = CLASS_VALUE_REQUEST;
    body[METHOD_KEY]  = METHOD_VALUE_UNREGISTER;
    body[ROOM_ID_KEY] = _room_id;
    sendRequest(body, trigger);
    return;
}

void WebRtcSignalingPeer::handleUnregisterAccept(SIGNALING_MSG_ARGS) {
    ResponseTrigger trigger;
    if (!responseFilter(allArgs, trigger)) {
        return;
    }

    trigger(SockException(Err_success), getRoomKey());
    return;
}

void WebRtcSignalingPeer::handleUnregisterReject(SIGNALING_MSG_ARGS) {
    ResponseTrigger trigger;
    if (!responseFilter(allArgs, trigger)) {
        return;
    }

    auto ex = SockException(Err_other, StrPrinter << "unregister refuses by server, reason: " << allArgs[REASON_KEY]);
    trigger(ex, getRoomKey());
    return;
}

void WebRtcSignalingPeer::sendCallRequest(const std::string& peer_room_id, const std::string& guest_id, const MediaTuple &tuple, const std::string& sdp, bool is_play, ResponseTrigger trigger) {
    DebugL;
    Json::Value body;
    body[CLASS_KEY]       = CLASS_VALUE_REQUEST;
    body[METHOD_KEY]      = METHOD_VALUE_CALL;
    body[TYPE_KEY]        = is_play? TYPE_VALUE_PLAY : TYPE_VALUE_PUSH;
    body[GUEST_ID_KEY]    = guest_id; //our guest id
    body[ROOM_ID_KEY]     = peer_room_id;
    body[CALL_VHOST_KEY]  = tuple.vhost;
    body[CALL_APP_KEY]    = tuple.app;
    body[CALL_STREAM_KEY] = tuple.stream;
    body[SDP_KEY]         = sdp;
    sendRequest(body, trigger);
    return;
}

void WebRtcSignalingPeer::sendCallAccept(const std::string& peer_guest_id, const MediaTuple &tuple, const std::string& sdp, bool is_play, const std::string& transaction_id) {
    DebugL;
    Json::Value body;
    body[CLASS_KEY]          = CLASS_VALUE_ACCEPT;
    body[METHOD_KEY]         = METHOD_VALUE_CALL;
    body[TRANSACTION_ID_KEY] = transaction_id;
    body[TYPE_KEY]           = is_play? TYPE_VALUE_PLAY : TYPE_VALUE_PUSH;
    body[GUEST_ID_KEY]       = peer_guest_id;
    body[ROOM_ID_KEY]        = _room_id;       //our room id
    body[CALL_VHOST_KEY]     = tuple.vhost;
    body[CALL_APP_KEY]       = tuple.app;
    body[CALL_STREAM_KEY]    = tuple.stream;
    body[SDP_KEY]            = sdp;
    sendPacket(body);
    return; }

void WebRtcSignalingPeer::handleCallRequest(SIGNALING_MSG_ARGS) {
    DebugL;
    CHECK_ARGS(GUEST_ID_KEY, ROOM_ID_KEY, CALL_VHOST_KEY, CALL_APP_KEY, CALL_STREAM_KEY, TYPE_KEY);

    if (allArgs[ROOM_ID_KEY] != getRoomId()) {
        WarnL << "target room_id: " << allArgs[ROOM_ID_KEY] << "mismatch our room_id: " << getRoomId();
        return;
    }

    auto args = std::make_shared<WebRtcArgsImp<Json::Value>>(allArgs, allArgs[GUEST_ID_KEY]);
    std::weak_ptr<WebRtcSignalingPeer> weak_self = std::static_pointer_cast<WebRtcSignalingPeer>(shared_from_this());
    WebRtcPluginManager::Instance().negotiateSdp(*shared_from_this(), allArgs[TYPE_KEY], *args, 
        [allArgs, weak_self](const WebRtcInterface &exchanger) mutable {
            auto strong_self =  weak_self.lock();
            if (!strong_self) {
                return;
            }

            return strong_self->processOffer(allArgs, const_cast<WebRtcInterface&>(exchanger));
        });

    return;
};

void WebRtcSignalingPeer::handleCallAccept(SIGNALING_MSG_ARGS) {
    DebugL;
    ResponseTrigger trigger;
    if (!responseFilter(allArgs, trigger)) {
        return;
    }

    CHECK_ARGS(GUEST_ID_KEY, ROOM_ID_KEY, CALL_VHOST_KEY, CALL_APP_KEY, CALL_STREAM_KEY, TYPE_KEY);

    auto room_id = allArgs[ROOM_ID_KEY];
    auto it = _tours.find(room_id);
    if (it == _tours.end()) {
        WarnL << "not found room_id: " << room_id << " in tours";
        return;
    }

    auto guest_id = it->second.first;
    if (allArgs[GUEST_ID_KEY] != guest_id) {
        WarnL << "guest_id: " << allArgs[GUEST_ID_KEY] << "mismatch our guest_id: " << guest_id;
        return;
    }

    trigger(SockException(Err_success), allArgs[SDP_KEY]);
    return;
};

void WebRtcSignalingPeer::handleCallReject(SIGNALING_MSG_ARGS) {
    DebugL;
    ResponseTrigger trigger;
    if (!responseFilter(allArgs, trigger)) {
        return;
    }

    CHECK_ARGS(GUEST_ID_KEY, ROOM_ID_KEY, CALL_VHOST_KEY, CALL_APP_KEY, CALL_STREAM_KEY, TYPE_KEY);
    DebugL;

    auto room_id = allArgs[ROOM_ID_KEY];
    auto it = _tours.find(room_id);
    if (it == _tours.end()) {
        WarnL << "not found room_id: " << room_id << " in tours";
        return;
    }

    auto guest_id = it->second.first;
    if (allArgs[GUEST_ID_KEY] != guest_id) {
        WarnL << "guest_id: " << allArgs[GUEST_ID_KEY] << "mismatch our guest_id: " << guest_id;
        return;
    }

    _tours.erase(room_id);
    InfoL;
    trigger(SockException(Err_other, StrPrinter << "call refuses by server, reason: " << allArgs[REASON_KEY]), "");
    return;
}

void WebRtcSignalingPeer::handleCandidateIndication(SIGNALING_MSG_ARGS) {
    DebugL;
    CHECK_ARGS(GUEST_ID_KEY, ROOM_ID_KEY, CANDIDATE_KEY, UFRAG_KEY, PWD_KEY);

    std::string identifier;
    //作为被叫
    if (allArgs[ROOM_ID_KEY] == getRoomId()) {
        auto it = _peer_guests.find(allArgs[GUEST_ID_KEY]);
        if (it == _peer_guests.end()) {
            WarnL << "not found guest_id: " << allArgs[GUEST_ID_KEY];
            return;
        }

        identifier = it->second;

    } else {
        //作为主叫
        for (auto it : _tours) {
            if (allArgs[ROOM_ID_KEY] != it.first) {
                continue;
            }

            auto info = it.second;
            if (allArgs[GUEST_ID_KEY] != info.first) {
                break;
            }
            identifier = info.second;
        }
    }

    TraceL << "recv remote candidate: " << allArgs[CANDIDATE_KEY];

    if (identifier.empty()) {
        WarnL << "target room_id: " << allArgs[ROOM_ID_KEY] << " not match our room_id: " << getRoomId()
            << ", and target guest_id: " << allArgs[GUEST_ID_KEY] << " not match";
        return;
    }

    auto transport = WebRtcTransportManager::Instance().getItem(identifier);
    if (!transport) {
        WarnL << "not found identifier transport: " << identifier;
        return;
    }

    SdpAttrCandidate candidate_attr;
    candidate_attr.parse(allArgs[CANDIDATE_KEY]);
    transport->connectivityCheck(candidate_attr, allArgs[UFRAG_KEY], allArgs[PWD_KEY]);
    return;
};

void WebRtcSignalingPeer::handleByeIndication(SIGNALING_MSG_ARGS) {
    DebugL;
    CHECK_ARGS(GUEST_ID_KEY, ROOM_ID_KEY);

    TraceL;

    if (allArgs[ROOM_ID_KEY] != getRoomId()) {
        WarnL << "target room_id: " << allArgs[ROOM_ID_KEY] << "not match our room_id: " << getRoomId();
        return;
    }
    auto it = _peer_guests.find(allArgs[GUEST_ID_KEY]);
    if (it == _peer_guests.end()) {
        WarnL << "not found guest_id: " << allArgs[GUEST_ID_KEY];
        return;
    }

    auto identifier = it->second;
    _peer_guests.erase(allArgs[GUEST_ID_KEY]);
    auto obj = WebRtcTransportManager::Instance().getItem(identifier);
    if (!obj) {
        WarnL << "not found identifier transport: " << identifier;
        return;
    }
    obj->safeShutdown(SockException(Err_shutdown, "deleted by websocket signaling server"));
    return;
};

void WebRtcSignalingPeer::sendByeIndication(const std::string& peer_room_id, const std::string &guest_id) {
    DebugL;
    Json::Value body;
    body[CLASS_KEY]    = CLASS_VALUE_INDICATION;
    body[METHOD_KEY]   = METHOD_VALUE_BYE;
    body[GUEST_ID_KEY] = guest_id; //our guest id
    body[ROOM_ID_KEY]  = peer_room_id;
    sendIndication(body);
    return;
}

void WebRtcSignalingPeer::sendCandidateIndication(const std::string& transport_identifier, const std::string& candidate, const std::string& ice_ufrag, const std::string& ice_pwd) {
    TraceL;
    Json::Value body;
    body[CLASS_KEY]  = CLASS_VALUE_INDICATION;
    body[METHOD_KEY] = METHOD_VALUE_CANDIDATE;
    body[CANDIDATE_KEY] = candidate;
    body[UFRAG_KEY] = ice_ufrag;
    body[PWD_KEY] = ice_pwd;

    //作为被叫
    for (auto it : _peer_guests) {
        if (it.second == transport_identifier) {
            body[ROOM_ID_KEY] = _room_id;
            body[GUEST_ID_KEY] = it.first; //peer_guest_id
            return sendIndication(body);
        }
    }

    //作为主叫
    for (auto it : _tours) {
        auto info = it.second;
        if (info.second == transport_identifier) {
            body[ROOM_ID_KEY] = it.first;           //peer room id
            body[GUEST_ID_KEY] = info.first; //our_guest_id
            return sendIndication(body);
        }
    }

    return;
}

void WebRtcSignalingPeer::sendRefusesResponse(Json::Value &body, const std::string& transaction_id, const std::string& reason) {
    body[CLASS_KEY]    = CLASS_VALUE_REJECT;
    body[REASON_KEY]   = reason;
    sendResponse(body, transaction_id);
}

void WebRtcSignalingPeer::sendRequest(Json::Value& body, ResponseTrigger trigger, uint32_t seconds) {

    auto transaction_id = makeRandStr(32);
    body[TRANSACTION_ID_KEY] = transaction_id;

    auto overtime = std::chrono::seconds(seconds);
    auto tuple = std::make_tuple(SteadyClock::now() + overtime, body[METHOD_KEY].asString(), trigger);
    _response_list.emplace(transaction_id, tuple);
    sendPacket(body);
    return;
}

void WebRtcSignalingPeer::sendIndication(Json::Value &body) {
    auto transaction_id = makeRandStr(32);
    body[TRANSACTION_ID_KEY] = transaction_id;
    sendPacket(body);
}

void WebRtcSignalingPeer::sendResponse(Json::Value &body, const std::string& transaction_id) {
    body[TRANSACTION_ID_KEY] = transaction_id;
    sendPacket(body);
}

void WebRtcSignalingPeer::sendPacket(Json::Value& body) {
    auto msg = body.toStyledString();
    DebugL << "send msg: " << msg;
    SockSender::send(msg);
    return;
}

Json::Value WebRtcSignalingPeer::makeInfoJson() {
    Json::Value item;
    item["room_id"] = getRoomId();
    item["room_key"] = getRoomKey();

    Json::Value peer_guests_obj(Json::arrayValue);
    auto peer_guests = _peer_guests;
    for(auto &guest : peer_guests) {
        Json::Value obj;
        obj["guest_id"] = guest.first;
        obj["transport_identifier"] = guest.second;
        peer_guests_obj.append(obj);
    }
    item["guests"] = peer_guests_obj;

    Json::Value tours_obj(Json::arrayValue);
    auto tours = _tours;
    for(auto &tour : tours){
        Json::Value obj;
        obj["room_id"] = tour.first;
        obj["guest_id"] = tour.second.first;
        obj["transport_identifier"] = tour.second.second;
        tours_obj.append(obj);
    }
    item["tours"] = tours_obj;
    return item;
}

void WebRtcSignalingPeer::createResponseExpireTimer() {
    std::weak_ptr<WebRtcSignalingPeer> weak_self = std::static_pointer_cast<WebRtcSignalingPeer>(shared_from_this());
    _expire_timer = std::make_shared<Timer>(0.2,
                                            [weak_self]() {
                                            auto strong_self = weak_self.lock();
                                            if (!strong_self) {
                                            return false;
                                            }

                                            strong_self->checkResponseExpire();
                                            return true;
                                            }, getPoller());

    return;
}

void WebRtcSignalingPeer::checkResponseExpire() {
    //FIXME: 移动到专门的超时timer中处理
#if 0
    // 设置计时器以检测 offer 响应超时
    _offer_timeout_timer = std::make_shared<Timer>(
        timeout_sec, 
        [this, cb, peer_room_id]() {
            _tours.erase(peer_room_id);
            return false; // 停止计时器
        }, 
        getPoller()
    );
#endif

    for (auto it : _response_list) {
        auto tuple = it.second;

        //over time
        auto now = SteadyClock::now();
        auto expire_time = std::get<0>(tuple);
        if (expire_time < now) {
            auto transcation_id = it.first;
            auto method = std::get<1>(tuple);
            WarnL << "transcation_id: " << transcation_id << ", method: " << method << " recv response over time";
            auto trigger = std::get<2>(tuple);
            trigger(SockException(Err_timeout, "recv response timeout"), "");
            _response_list.erase(transcation_id);
            return;
        }
    }
}

}// namespace mediakit
