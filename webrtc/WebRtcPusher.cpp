/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcPusher.h"
#include "Common/config.h"
#include "Rtsp/RtspMediaSourceImp.h"

using namespace std;

namespace mediakit {

WebRtcPusher::Ptr WebRtcPusher::create(const EventPoller::Ptr &poller,
                                       const RtspMediaSource::Ptr &src,
                                       const std::shared_ptr<void> &ownership,
                                       const MediaInfo &info,
                                       const ProtocolOption &option) {
    WebRtcPusher::Ptr ret(new WebRtcPusher(poller, src, ownership, info, option), [](WebRtcPusher *ptr) {
        ptr->onDestory();
        delete ptr;
    });
    ret->onCreate();
    return ret;
}

WebRtcPusher::WebRtcPusher(const EventPoller::Ptr &poller,
                           const RtspMediaSource::Ptr &src,
                           const std::shared_ptr<void> &ownership,
                           const MediaInfo &info,
                           const ProtocolOption &option) : WebRtcTransportImp(poller) {
    _media_info = info;
    _push_src = src;
    _push_src_ownership = ownership;
    _continue_push_ms = option.continue_push_ms;
    CHECK(_push_src);
}

bool WebRtcPusher::close(MediaSource &sender) {
    // 此回调在其他线程触发  [AUTO-TRANSLATED:c98e7686]
    // This callback is triggered in another thread
    string err = StrPrinter << "close media: " << sender.getUrl();
    weak_ptr<WebRtcPusher> weak_self = static_pointer_cast<WebRtcPusher>(shared_from_this());
    getPoller()->async([weak_self, err]() {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->onShutdown(SockException(Err_shutdown, err));
            // 主动关闭推流，那么不延时注销  [AUTO-TRANSLATED:ee7cc580]
            // Actively close the stream, then do not delay the logout
            strong_self->_push_src = nullptr;
        }
    });
    return true;
}

int WebRtcPusher::totalReaderCount(MediaSource &sender) {
    auto total_count = _push_src ? _push_src->totalReaderCount() : 0;
    if (_simulcast) {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        for (auto &src : _push_src_sim) {
            total_count += src.second->totalReaderCount();
        }
    }
    return total_count;
}

MediaOriginType WebRtcPusher::getOriginType(MediaSource &sender) const {
    return MediaOriginType::rtc_push;
}

string WebRtcPusher::getOriginUrl(MediaSource &sender) const {
    return _media_info.full_url;
}

std::shared_ptr<SockInfo> WebRtcPusher::getOriginSock(MediaSource &sender) const {
    return static_pointer_cast<SockInfo>(getSession());
}

toolkit::EventPoller::Ptr WebRtcPusher::getOwnerPoller(MediaSource &sender) {
    return getPoller();
}

void WebRtcPusher::onRecvRtp(MediaTrack &track, const string &rid, RtpPacket::Ptr rtp) {
    if (!_simulcast) {
        assert(_push_src);
        _push_src->onWrite(rtp, false);
        return;
    }

    if (rtp->type == TrackAudio) {
        // 音频  [AUTO-TRANSLATED:a577d8e1]
        // Audio
        for (auto &pr : _push_src_sim) {
            pr.second->onWrite(rtp, false);
        }
    } else {
        // 视频  [AUTO-TRANSLATED:904730ac]
        // Video
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        auto &src = _push_src_sim[rid];
        if (!src) {
            const auto& stream = _push_src->getMediaTuple().stream;
            auto src_imp = _push_src->clone(rid.empty() ? stream : stream + '_' + rid);
            _push_src_sim_ownership[rid] = src_imp->getOwnership();
            src_imp->setListener(static_pointer_cast<WebRtcPusher>(shared_from_this()));
            src = src_imp;
        }
        src->onWrite(std::move(rtp), false);
    }
}

void WebRtcPusher::onStartWebRTC() {
    WebRtcTransportImp::onStartWebRTC();
    _simulcast = _answer_sdp->supportSimulcast();
    if (canRecvRtp()) {
        _push_src->setSdp(_answer_sdp->toRtspSdp());
    }
}

void WebRtcPusher::onDestory() {
    auto duration = getDuration();
    auto bytes_usage = getBytesUsage();
    // 流量统计事件广播  [AUTO-TRANSLATED:6b0b1234]
    // Traffic statistics event broadcast
    GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);

    if (getSession()) {
        WarnL << "RTC推流器(" << _media_info.shortUrl() << ")结束推流,耗时(s):" << duration;
        if (bytes_usage >= iFlowThreshold * 1024) {
            NOTICE_EMIT(BroadcastFlowReportArgs, Broadcast::kBroadcastFlowReport, _media_info, bytes_usage, duration, false, *getSession());
        }
    }

    if (_push_src && _continue_push_ms) {
        // 取消所有权  [AUTO-TRANSLATED:4895d8fa]
        // Cancel ownership
        _push_src_ownership = nullptr;
        // 延时10秒注销流  [AUTO-TRANSLATED:e1bb11f9]
        // Delay 10 seconds to log out the stream
        auto push_src = std::move(_push_src);
        getPoller()->doDelayTask(_continue_push_ms, [push_src]() { return 0; });
    }
    WebRtcTransportImp::onDestory();
}

void WebRtcPusher::onRtcConfigure(RtcConfigure &configure) const {
    WebRtcTransportImp::onRtcConfigure(configure);
    // 这只是推流  [AUTO-TRANSLATED:f877bf98]
    // This is just pushing the stream
    configure.audio.direction = configure.video.direction = RtpDirection::recvonly;
}

float WebRtcPusher::getLossRate(MediaSource &sender,TrackType type) {
    return WebRtcTransportImp::getLossRate(type);
}

void WebRtcPusher::OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) {
   // 主动关闭推流，那么不等待重推  [AUTO-TRANSLATED:1ff514d7]
   // Actively close the stream, then do not wait for re-pushing
    _push_src = nullptr;
    WebRtcTransportImp::OnDtlsTransportClosed(dtlsTransport);
}

void WebRtcPusher::onRtcpBye() {
     WebRtcTransportImp::onRtcpBye();
}

void WebRtcPusher::onShutdown(const SockException &ex) {
     _push_src = nullptr;
     WebRtcTransportImp::onShutdown(ex);
}

}// namespace mediakit