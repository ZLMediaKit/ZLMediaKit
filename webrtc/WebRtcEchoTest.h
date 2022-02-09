/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTCECHOTEST_H
#define ZLMEDIAKIT_WEBRTCECHOTEST_H

#include "WebRtcTransport.h"

class WebRtcEchoTest : public WebRtcTransportImp {
public:
    using Ptr = std::shared_ptr<WebRtcEchoTest>;
    ~WebRtcEchoTest() override = default;
    static Ptr create(const EventPoller::Ptr &poller);

protected:
    ///////WebRtcTransportImp override///////
    void onRtcConfigure(RtcConfigure &configure) const override;
    void onCheckSdp(SdpType type, RtcSession &sdp) override;
    void onRtp(const char *buf, size_t len, uint64_t stamp_ms) override;
    void onRtcp(const char *buf, size_t len) override;

    void onRecvRtp(MediaTrack &track, const std::string &rid, mediakit::RtpPacket::Ptr rtp) override {};
    void onBeforeEncryptRtp(const char *buf, int &len, void *ctx) override {};
    void onBeforeEncryptRtcp(const char *buf, int &len, void *ctx) override {};

private:
    WebRtcEchoTest(const EventPoller::Ptr &poller);
};

#endif //ZLMEDIAKIT_WEBRTCECHOTEST_H
