/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTC_PROXY_PLAYER_H
#define ZLMEDIAKIT_WEBRTC_PROXY_PLAYER_H

#include "Network/Socket.h"
#include "Player/PlayerBase.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"
#include "WebRtcClient.h"
#include <memory>
#include <string>

namespace mediakit {

// 实现了webrtc代理拉流功能
class WebRtcProxyPlayer
    : public PlayerBase , public WebRtcClient {
public:
    using Ptr = std::shared_ptr<WebRtcProxyPlayer>;

    WebRtcProxyPlayer(const toolkit::EventPoller::Ptr &poller);
    ~WebRtcProxyPlayer() override;

    //// PlayerBase override////
    void play(const std::string &strUrl) override;
    void teardown() override;
    void pause(bool pause) override;
    void speed(float speed) override;

    std::shared_ptr<toolkit::SockInfo> getSockInfo() const override { 
        return getWebRtcTransport() ? getWebRtcTransport()->getSession() : nullptr;
    }
    size_t getRecvSpeed() override { 
        return getWebRtcTransport() ? getWebRtcTransport()->getRecvSpeed() : 0;
    }
    size_t getRecvTotalBytes() override { 
        return getWebRtcTransport() ? getWebRtcTransport()->getRecvTotalBytes() : 0; 
    }

protected:

    //// WebRtcClient override////
    bool isPlayer() override {return true;}
    float getTimeOutSec() override;
    void onNegotiateFinish() override;

protected:
    //是否为性能测试模式
    bool _benchmark_mode = false;

    //超时功能实现
    toolkit::Ticker _recv_ticker;
    std::shared_ptr<toolkit::Timer> _check_timer;
};

} /* namespace mediakit */
#endif /* ZLMEDIAKIT_WEBRTC_PROXY_PLAYER_H */
