/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcEchoTest.h"

WebRtcEchoTest::Ptr WebRtcEchoTest::create(const EventPoller::Ptr &poller) {
    WebRtcEchoTest::Ptr ret(new WebRtcEchoTest(poller), [](WebRtcEchoTest *ptr) {
        ptr->onDestory();
        delete ptr;
    });
    ret->onCreate();
    return ret;
}

WebRtcEchoTest::WebRtcEchoTest(const EventPoller::Ptr &poller) : WebRtcTransportImp(poller) {}

void WebRtcEchoTest::onRtcConfigure(RtcConfigure &configure) const {
    WebRtcTransportImp::onRtcConfigure(configure);
    configure.audio.direction = configure.video.direction = RtpDirection::sendrecv;
    configure.audio.extmap.emplace(RtpExtType::sdes_mid, RtpDirection::sendrecv);
    configure.video.extmap.emplace(RtpExtType::sdes_mid, RtpDirection::sendrecv);
}

void WebRtcEchoTest::onRtp(const char *buf, size_t len, uint64_t stamp_ms) {
    updateTicker();
    sendRtpPacket(buf, len, true, nullptr);
}

void WebRtcEchoTest::onRtcp(const char *buf, size_t len) {
    sendRtcpPacket(buf, len, true, nullptr);
}

//修改mline的a=msid属性，目的是在echo test的情况下，如果offer和answer的msid相同，chrome会忽略远端的track
void WebRtcEchoTest::onCheckSdp(SdpType type, RtcSession &sdp) {
    if (type == SdpType::answer) {
        for (auto &m : sdp.media) {
            for (auto &ssrc : m.rtp_rtx_ssrc) {
                if (!ssrc.msid.empty()) {
                    ssrc.msid = "zlmediakit msid";
                }
            }
        }
    }
}