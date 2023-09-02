/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcPlayer.h"
#include "Common/config.h"

using namespace std;

namespace mediakit {

WebRtcPlayer::Ptr WebRtcPlayer::create(const EventPoller::Ptr &poller,
                                       const RtspMediaSource::Ptr &src,
                                       const MediaInfo &info,
                                       bool preferred_tcp) {
    WebRtcPlayer::Ptr ret(new WebRtcPlayer(poller, src, info, preferred_tcp), [](WebRtcPlayer *ptr) {
        ptr->onDestory();
        delete ptr;
    });
    ret->onCreate();
    return ret;
}

WebRtcPlayer::WebRtcPlayer(const EventPoller::Ptr &poller,
                           const RtspMediaSource::Ptr &src,
                           const MediaInfo &info,
                           bool preferred_tcp) : WebRtcTransportImp(poller,preferred_tcp) {
    _media_info = info;
    _play_src = src;
    CHECK(src);
}

void WebRtcPlayer::onStartWebRTC() {
    auto playSrc = _play_src.lock();
    if(!playSrc){
        onShutdown(SockException(Err_shutdown, "rtsp media source was shutdown"));
        return ;
    }
    WebRtcTransportImp::onStartWebRTC();
    if (canSendRtp()) {
        playSrc->pause(false);
        _reader = playSrc->getRing()->attach(getPoller(), true);
        weak_ptr<WebRtcPlayer> weak_self = static_pointer_cast<WebRtcPlayer>(shared_from_this());
        weak_ptr<Session> weak_session = static_pointer_cast<Session>(getSession());
        _reader->setGetInfoCB([weak_session]() {
            Any ret;
            ret.set(static_pointer_cast<SockInfo>(weak_session.lock()));
            return ret;
        });
        _reader->setReadCB([weak_self](const RtspMediaSource::RingDataType &pkt) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            size_t i = 0;
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                //TraceL<<"send track type:"<<rtp->type<<" ts:"<<rtp->getStamp()<<" ntp:"<<rtp->ntp_stamp<<" size:"<<rtp->getPayloadSize()<<" i:"<<i;
                strong_self->onSendRtp(rtp, ++i == pkt->size());
            });
        });
        _reader->setDetachCB([weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->onShutdown(SockException(Err_shutdown, "rtsp ring buffer detached"));
        });

        _reader->setMessageCB([weak_self] (const toolkit::Any &data) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            if (data.is<Buffer>()) {
                auto &buffer = data.get<Buffer>();
                // PPID 51: 文本string
                // PPID 53: 二进制
                strong_self->sendDatachannel(0, 51, buffer.data(), buffer.size());
            } else {
                WarnL << "Send unknown message type to webrtc player: " << data.type_name();
            }
        });
    }
}
void WebRtcPlayer::onDestory() {
    auto duration = getDuration();
    auto bytes_usage = getBytesUsage();
    //流量统计事件广播
    GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);
    if (_reader && getSession()) {
        WarnL << "RTC播放器(" << _media_info.shortUrl() << ")结束播放,耗时(s):" << duration;
        if (bytes_usage >= iFlowThreshold * 1024) {
            NOTICE_EMIT(BroadcastFlowReportArgs, Broadcast::kBroadcastFlowReport, _media_info, bytes_usage, duration, true, *getSession());
        }
    }
    WebRtcTransportImp::onDestory();
}

void WebRtcPlayer::onRtcConfigure(RtcConfigure &configure) const {
    auto playSrc = _play_src.lock();
    if(!playSrc){
        return ;
    }
    WebRtcTransportImp::onRtcConfigure(configure);
    //这是播放
    configure.audio.direction = configure.video.direction = RtpDirection::sendonly;
    configure.setPlayRtspInfo(playSrc->getSdp());
}

}// namespace mediakit