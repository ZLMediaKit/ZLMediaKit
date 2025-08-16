/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTC_CLIENT_H
#define ZLMEDIAKIT_WEBRTC_CLIENT_H

#include "Network/Socket.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"
#include "Http/HttpRequester.h"
#include "webrtc/Sdp.h"
#include "webrtc/WebRtcTransport.h"
#include "webrtc/WebRtcSignalingPeer.h"
#include <memory>
#include <string>

namespace mediakit {

// 解析webrtc 信令url的工具类
class WebRTCUrl {
public:
    bool _is_ssl;
    std::string _full_url;
    std::string _negotiate_url;   //for whep or whip
    std::string _delete_url;   //for whep or whip
    std::string _target_secret;
    std::string _params;
    std::string _host;
    uint16_t _port;
    std::string _vhost;
    std::string _app;
    std::string _stream;
    WebRtcTransport::SignalingProtocols _signaling_protocols 
    = WebRtcTransport::SignalingProtocols::WHEP_WHIP;
    std::string _peer_room_id;    //peer room_id

public:
    void parse(const std::string &url, bool isPlayer);

private:
};

namespace Rtc {
typedef enum {
    Signaling_Invalid   = -1,
    Signaling_WHEP_WHIP = 0,
    Signaling_WEBSOCKET = 1,
} eSignalingProtocols;
}//namespace RTC

// 实现了webrtc代理功能
class WebRtcClient : public std::enable_shared_from_this<WebRtcClient>{
public:
    using Ptr = std::shared_ptr<WebRtcClient>;

    WebRtcClient(const toolkit::EventPoller::Ptr &poller);
    virtual ~WebRtcClient();

    const toolkit::EventPoller::Ptr &getPoller() const {return _poller;}
    void setPoller(const toolkit::EventPoller::Ptr &poller)  {_poller = poller;};
    
    // 获取WebRTC transport，用于API查询
    WebRtcTransport::Ptr getWebRtcTransport() const { return _transport; }

protected:
    virtual bool isPlayer() = 0;
    virtual void startConnect();
    virtual void onResult(const toolkit::SockException &ex);
    virtual void onNegotiateFinish();
    virtual float getTimeOutSec();

    void doNegotiate();
    void doNegotiateWebsocket();
    void doNegotiateWhepOrWhip();
    void checkIn();
    void doBye();
    void doByeWhepOrWhip();
    void checkOut();

    void gatheringCandidate(IceServerInfo::Ptr ice_server);
    void connectivityCheck();
    void candidate(const std::string& candidate, const std::string& ufrag, const std::string pwd);

protected:
    toolkit::EventPoller::Ptr _poller;

    //for _negotiate_sdp
    WebRTCUrl _url;
    HttpRequester::Ptr _negotiate = nullptr;
    WebRtcSignalingPeer::Ptr _peer = nullptr;
    WebRtcTransport::Ptr _transport = nullptr;
    bool _is_negotiate_finished = false;

private:
    std::map<std::string /*candidate key*/, SocketHelper::Ptr> _socket_map;
};

} /*namespace mediakit */
#endif /* ZLMEDIAKIT_WEBRTC_CLIENT_H */
