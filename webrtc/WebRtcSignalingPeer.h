/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */


#ifndef ZLMEDIAKIT_WEBRTC_SIGNALING_PEER_H
#define ZLMEDIAKIT_WEBRTC_SIGNALING_PEER_H

#include <chrono>
#include "Poller/Timer.h"
#include "Network/Session.h"
#include "Http/WebSocketClient.h"
#include "webrtc/WebRtcSignalingMsg.h"
#include "webrtc/WebRtcTransport.h"

namespace mediakit {

class WebRtcSignalingPeer : public WebSocketClient<toolkit::TcpClient>  {
public:
    struct ClassMethodHash {
        bool operator()(std::pair<std::string /*class*/, std::string /*method*/> key) const {
            std::size_t h = 0;
            h ^= std::hash<std::string>()(key.first) << 0;
            h ^= std::hash<std::string>()(key.second) << 1;
            return h;
        }
    };
    using Ptr = std::shared_ptr<WebRtcSignalingPeer>;
    WebRtcSignalingPeer(const std::string &host, uint16_t port, bool ssl, const std::string &room_id, const toolkit::EventPoller::Ptr &poller = nullptr);
    virtual ~WebRtcSignalingPeer();

    void connect();
    void regist(const std::function<void(const toolkit::SockException &ex, const std::string &key)> &cb);
    void unregist(const std::function<void(const toolkit::SockException &ex)> &cb);
    void checkIn(const std::string& peer_room_id, const MediaTuple &tuple, const std::string& identifier,
                 const std::string& offer, bool is_play, const std::function<void(const toolkit::SockException &ex, const std::string& answer)> &cb, float timeout_sec);
    void checkOut(const std::string& peer_room_id);
    void candidate(const std::string& transport_identifier, const std::string& candidate, const std::string& ice_ufrag, const std::string& ice_pwd);

    void processOffer(SIGNALING_MSG_ARGS, WebRtcInterface &transport);
    void answer(const std::string& guest_id, const MediaTuple &tuple, const std::string& identifier, const std::string& sdp, bool is_play, const std::string& transaction_id);

    const std::string& getRoomKey() const {
        return _room_key;
    }

    const std::string& getRoomId() const {
        return _room_id;
    }

    const RTC::IceServerInfo::Ptr& getIceServer() const {
        return _ice_server;
    }

    //// TcpClient override////
    void setOnConnect(std::function<void(const toolkit::SockException &ex)> cb);
    void onConnect(const toolkit::SockException &ex) override;
    void setOnShutdown(std::function<void(const toolkit::SockException &ex)> cb);
    void onShutdown(const toolkit::SockException &ex);
    void onRecv(const toolkit::Buffer::Ptr &) override;
    void onError(const toolkit::SockException &err) override;

    Json::Value makeInfoJson();

protected:
    void checkResponseExpired();
    void createResponseExpiredTimer();

    using ResponseTrigger = std::function<void(const toolkit::SockException &ex, std::string /*msg*/)>;
    struct ResponseTuple {
        toolkit::Ticker ticker;
        uint32_t ttl_ms;
        std::string method;
        ResponseTrigger cb;

        bool expired() {
            return ticker.elapsedTime() > ttl_ms;
        }
    };

    bool responseFilter(SIGNALING_MSG_ARGS, ResponseTrigger& trigger);

    void sendRegisterRequest(ResponseTrigger trigger);
    void handleRegisterAccept(SIGNALING_MSG_ARGS);

    void handleRegisterReject(SIGNALING_MSG_ARGS);
    void sendUnregisterRequest(ResponseTrigger trigger);
    void handleUnregisterAccept(SIGNALING_MSG_ARGS);
    void handleUnregisterReject(SIGNALING_MSG_ARGS);

    void sendCallRequest(const std::string& peer_room_id, const std::string& guest_id, const MediaTuple &tuple, const std::string& sdp, bool is_play, ResponseTrigger trigger);
    void sendCallAccept(const std::string& peer_guest_id, const MediaTuple &tuple, const std::string& sdp, bool is_play, const std::string& transaction_id);
    void handleCallRequest(SIGNALING_MSG_ARGS);
    void handleCallAccept(SIGNALING_MSG_ARGS);
    void handleCallReject(SIGNALING_MSG_ARGS);

    void sendCandidateIndication(const std::string& transport_identifier, const std::string& candidate, const std::string& ice_ufrag, const std::string& ice_pwd);
    void handleCandidateIndication(SIGNALING_MSG_ARGS);

    void sendByeIndication(const std::string& peer_room_id, const std::string &guest_id);
    void handleByeIndication(SIGNALING_MSG_ARGS);

    void sendAcceptResponse(const std::string& method, const std::string& transaction_id, const std::string& room_id, const std::string& guest_id, const std::string& reason);
    void sendRefusesResponse(Json::Value &body, const std::string& transaction_id, const std::string& reason);

    void sendIndication(Json::Value &body);
    void sendRequest(Json::Value& body, ResponseTrigger trigger, float seconds = 10);
    void sendResponse(Json::Value &body, const std::string& transaction_id);
    void sendPacket(Json::Value& body);

private:
    toolkit::Timer::Ptr _expire_timer;
    std::string _ws_url;
    std::string _room_key;
    std::string _room_id;
    std::unordered_map<std::string /*peer_guest_id*/, std::string /*transport_identifier*/> _peer_guests; //作为被叫
    std::unordered_map<std::string /*peer_room_id*/, std::pair<std::string /*guest_id*/, std::string /*transport_identifier*/>> _tours;  //作为主叫
    RTC::IceServerInfo::Ptr _ice_server;
    std::unordered_map<std::string /*transcation ID*/, ResponseTuple> _response_list;

    std::function<void(const toolkit::SockException &ex)> _on_connect;
    std::function<void(const toolkit::SockException &ex)> _on_shutdown;
    toolkit::Timer::Ptr _offer_timeout_timer = nullptr;
};

void addWebrtcRoomKeeper(const std::string &host, uint16_t port, const std::string& room_id, bool ssl,
                         const std::function<void(const toolkit::SockException &ex, const std::string &key)> &cb);
void delWebrtcRoomKeeper(const std::string &key, const std::function<void(const toolkit::SockException &ex)> &cb);
void listWebrtcRoomKeepers(const std::function<void(const std::string& key, const WebRtcSignalingPeer::Ptr& p)> &cb);
Json::Value ToJson(const WebRtcSignalingPeer::Ptr& p);
WebRtcSignalingPeer::Ptr getWebrtcRoomKeeper(const std::string &host, uint16_t port);

} // namespace mediakit

#endif // ZLMEDIAKIT_WEBRTC_SIGNALING_PEER_H
