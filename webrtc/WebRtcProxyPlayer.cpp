/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcProxyPlayer.h"
#include "WebRtcProxyPlayerImp.h"
#include "WebRtcPusher.h"
#include "Common/config.h"
#include "Http/HlsPlayer.h"
#include "Rtsp/RtspMediaSourceImp.h"

using namespace toolkit;
using namespace std;

namespace mediakit {

WebRtcProxyPlayer::WebRtcProxyPlayer(const EventPoller::Ptr &poller) 
    : WebRtcClient(poller) {
    DebugL;
}

WebRtcProxyPlayer::~WebRtcProxyPlayer(void) {
    DebugL;
}

void WebRtcProxyPlayer::play(const string &strUrl) {
    DebugL;
    try {
        _url.parse(strUrl, isPlayer());
    } catch (std::exception &ex) {
        onResult(SockException(Err_other, StrPrinter << "illegal webrtc url:" << ex.what()));
        return;
    }

    startConnect();
}

void WebRtcProxyPlayer::teardown() {
    DebugL;
    doBye();
}

void WebRtcProxyPlayer::pause(bool bPause) {
    DebugL;
}

void WebRtcProxyPlayer::speed(float speed) {
    DebugL;
}

float WebRtcProxyPlayer::getTimeOutSec() {
    auto timeoutMS = (*this)[Client::kTimeoutMS].as<uint64_t>();
    return (float)timeoutMS / (float)1000;
}

void WebRtcProxyPlayer::onNegotiateFinish() {
    DebugL;
    onResult(SockException(Err_success, "webrtc play success"));
    WebRtcClient::onNegotiateFinish();
}

///////////////////////////////////////////////////
// WebRtcProxyPlayerImp

void WebRtcProxyPlayerImp::startConnect() {
    DebugL;
    MediaInfo info(_url._full_url);
    ProtocolOption option;
    std::weak_ptr<WebRtcProxyPlayerImp> weak_self = std::static_pointer_cast<WebRtcProxyPlayerImp>(shared_from_this());
    _transport = WebRtcPlayerClient::create(getPoller(), WebRtcTransport::Role::CLIENT, _url._signaling_protocols);
    _transport->setOnShutdown([weak_self](const SockException &ex) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->onResult(ex);
    });
    WebRtcClient::startConnect();
}

void WebRtcProxyPlayerImp::onResult(const SockException &ex) {
    if (!ex) {
        // 播放成功
        _benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();

        WebRtcPlayerClient::Ptr transport = std::dynamic_pointer_cast<WebRtcPlayerClient>(_transport);
        auto media_src = dynamic_pointer_cast<RtspMediaSource>(_media_src);
        transport->setMediaSource(media_src);
        std::weak_ptr<WebRtcProxyPlayerImp> weak_self = std::static_pointer_cast<WebRtcProxyPlayerImp>(shared_from_this());
        if (!ex) {
            transport->setOnStartWebRTC([weak_self, ex]() {
                if (auto strong_self = weak_self.lock()) {
                    strong_self->onPlayResult(ex);
                }
            });
        }
    } else {
        WarnL << ex.getErrCode() << " " << ex.what();
        if (ex.getErrCode() == Err_shutdown) {
            // 主动shutdown的，不触发回调
            return;
        }

        if (!_is_negotiate_finished) {
            onPlayResult(ex);
        } else {
            onShutdown(ex);
        }
    }
}

std::vector<Track::Ptr> WebRtcProxyPlayerImp::getTracks(bool ready /*= true*/) const {
    auto transport = static_pointer_cast<WebRtcPlayerClient>(_transport);
    return transport ? transport->getTracks(ready) : Super::getTracks(ready);
}

void WebRtcProxyPlayerImp::addTrackCompleted() {
}

} /* namespace mediakit */
