/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTC_PROXY_PUSHER_H
#define ZLMEDIAKIT_WEBRTC_PROXY_PUSHER_H

#include "Network/Socket.h"
#include "Pusher/PusherBase.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"
#include "WebRtcClient.h"
#include <memory>
#include <string>

namespace mediakit {

// 实现了webrtc代理拉流功能
class WebRtcProxyPusher
    : public PusherBase , public WebRtcClient {
public:
    using Ptr = std::shared_ptr<WebRtcProxyPusher>;

    WebRtcProxyPusher(const toolkit::EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src);
    ~WebRtcProxyPusher() override;

    //// PusherBase override////
    void publish(const std::string &url) override;
    void teardown() override;

    size_t getSendSpeed() override { return getWebRtcTransport() ? getWebRtcTransport()->getSendSpeed() : 0; }
    size_t getSendTotalBytes() override { return getWebRtcTransport() ? getWebRtcTransport()->getSendTotalBytes() : 0; }

protected:
    //// WebRtcClient override////
    void startConnect() override;
    bool isPlayer() override { return false; }
    void onResult(const toolkit::SockException &ex) override;
    float getTimeOutSec() override;

protected:
    std::weak_ptr<RtspMediaSource> _push_src;
};

using WebRtcProxyPusherImp = PusherImp<WebRtcProxyPusher, PusherBase>;

} /* namespace mediakit */
#endif /* ZLMEDIAKIT_WEBRTC_PROXY_PUSHER_H */
