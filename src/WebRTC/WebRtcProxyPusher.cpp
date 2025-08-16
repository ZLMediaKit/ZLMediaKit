/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcProxyPusher.h"
#include "webrtc/WebRtcPlayer.h"
#include "Common/config.h"
#include "Http/HlsPlayer.h"
#include "Rtsp/RtspMediaSourceImp.h"

using namespace toolkit;
using namespace std;

namespace mediakit {

WebRtcProxyPusher::WebRtcProxyPusher(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src) : WebRtcClient(poller) {
    _push_src = src;
    DebugL;
}

WebRtcProxyPusher::~WebRtcProxyPusher(void) {
    teardown();
    DebugL;
}

void WebRtcProxyPusher::publish(const string &strUrl) {
    DebugL;
    try {
        _url.parse(strUrl, isPlayer());
    } catch (std::exception &ex) {
        onResult(SockException(Err_other, StrPrinter << "illegal webrtc url:" << ex.what()));
        return;
    }

    startConnect();
    return;
}

void WebRtcProxyPusher::teardown() {
    DebugL;
    WebRtcClient::onResult(SockException(Err_other, StrPrinter << "teardown: " << _url._full_url));
}

void WebRtcProxyPusher::onResult(const SockException &ex) {
    WebRtcClient::onResult(ex);

    DebugL;
    if (!ex) {
        // 播放成功
        onPublishResult(ex);
    } else {
        WarnL << ex.getErrCode() << " " << ex.what();
        if (ex.getErrCode() == Err_shutdown) {
            // 主动shutdown的，不触发回调
            return;
        }
        if (!_is_negotiate_finished) {
            onPublishResult(ex);
        } else {
            onShutdown(ex);
        }
    }
    return;
}

float WebRtcProxyPusher::getTimeOutSec() {
    auto timeoutMS = (*this)[Client::kTimeoutMS].as<uint64_t>();
    return (float)timeoutMS / (float)1000;
}

void WebRtcProxyPusher::onNegotiateFinish() {
    DebugL;
    onResult(SockException(Err_success, "webrtc push success"));
    WebRtcClient::onNegotiateFinish();
    doPublish();
}

void WebRtcProxyPusher::doPublish() {
    auto src = _push_src.lock();
    if (!src) {
        onResult(SockException(Err_eof, "the media source was released"));
        return;
    }

    std::weak_ptr<WebRtcProxyPusher> weak_self = static_pointer_cast<WebRtcProxyPusher>(shared_from_this());
    _rtsp_reader = src->getRing()->attach(getPoller());
    _rtsp_reader->setDetachCB([weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->onShutdown(SockException(Err_shutdown));
    });
    _rtsp_reader->setReadCB([weak_self](const RtspMediaSource::RingDataType &pkt) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        // strong_self->sendRtpPacket(pkt);
    });
}

void WebRtcProxyPusher::startConnect() {
    DebugL;
    MediaInfo info(_url._full_url);
    info.schema = "rtc";
    auto src = _push_src.lock();
    std::weak_ptr<WebRtcProxyPusher> weak_self = std::static_pointer_cast<WebRtcProxyPusher>(shared_from_this());
    _transport = WebRtcPlayer::create(getPoller(), src, info, WebRtcTransport::Role::CLIENT, _url._signaling_protocols);
    _transport->setOnShutdown([weak_self](const SockException &ex) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->onResult(ex);
    });
    WebRtcClient::startConnect();
}

} /* namespace mediakit */

