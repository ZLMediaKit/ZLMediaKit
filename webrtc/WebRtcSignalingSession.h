/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTC_SIGNALING_SESSION_H
#define ZLMEDIAKIT_WEBRTC_SIGNALING_SESSION_H

#include "Network/Session.h"
#include "Http/WebSocketSession.h"
#include "webrtc/WebRtcSignalingMsg.h"

namespace mediakit {

// webrtc 信令, 基于websocket实现
class WebRtcSignalingSession : public toolkit::Session {
public:
    struct ClassMethodHash {
        bool operator()(std::pair<std::string /*class*/, std::string /*method*/> key) const {
            std::size_t h = 0;
            h ^= std::hash<std::string>()(key.first) << 0;
            h ^= std::hash<std::string>()(key.second) << 1;
            return h;
        }
    };

    using Ptr = std::shared_ptr<WebRtcSignalingSession>;
    using WeakPtr = std::weak_ptr<WebRtcSignalingSession>;

    WebRtcSignalingSession(const toolkit::Socket::Ptr &sock);
    virtual ~WebRtcSignalingSession();

    Json::Value makeInfoJson();

    std::string getRoomId() { return _room_id; };

    //// Session override////
    void onRecv(const toolkit::Buffer::Ptr &) override;
    void onError(const toolkit::SockException &err) override;
    void onManager() override;

protected:
    void handleRegisterRequest(SIGNALING_MSG_ARGS);
    void handleUnregisterRequest(SIGNALING_MSG_ARGS);
    void handleCallRequest(SIGNALING_MSG_ARGS);
    void handleCallAccept(SIGNALING_MSG_ARGS);
    #define handleCallReject handleCallAccept
    void handleByeIndication(SIGNALING_MSG_ARGS);
    void handleCandidateIndication(SIGNALING_MSG_ARGS);
    void handleOtherMsg(SIGNALING_MSG_ARGS);

    void notifyByeIndication();
    void forwardCallRequest(WebRtcSignalingSession::WeakPtr sender, SIGNALING_MSG_ARGS);
    void forwardCallAccept(SIGNALING_MSG_ARGS);
    void forwardBye(SIGNALING_MSG_ARGS);
    void forwardBye(Json::Value allArgs);
    void forwardPacket(SIGNALING_MSG_ARGS);

    void sendRegisterAccept(Json::Value& body, const std::string& transaction_id);
    void sendAcceptResponse(Json::Value &body, const std::string& transaction_id);
    void sendRejectResponse(Json::Value &body, const std::string& transaction_id, const std::string& reason);

    void sendResponse(Json::Value &body, const std::string& transaction_id);
    void sendPacket(const Json::Value &body);

private:
    std::string _room_id; //
    std::unordered_map<std::string /*guest id*/, std::string /*peer_room_id*/> _tours;  //作为主叫
    std::unordered_map<std::string /*peer_guest_id*/, WebRtcSignalingSession::WeakPtr /*session*/> _guests; //作为被叫
};

using WebRtcWebcosktSignalingSession = WebSocketSession<WebRtcSignalingSession, HttpSession>;
using WebRtcWebcosktSignalSslSession = WebSocketSession<WebRtcSignalingSession, HttpsSession>;

void listWebrtcRooms(const std::function<void(const std::string& key, const WebRtcSignalingSession::Ptr& p)> &cb);
Json::Value ToJson(const WebRtcSignalingSession::Ptr& p);
WebRtcSignalingSession::Ptr getWebrtcRoomKeeper(const std::string &room_id);
}// namespace mediakit

#endif //ZLMEDIAKIT_WEBRTC_SIGNALING_SESSION_H
