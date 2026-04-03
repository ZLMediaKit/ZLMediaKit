/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcTalk.h"

#include "Util/base64.h"
#include "Common/config.h"
#include "Extension/Factory.h"
#include "Common/MultiMediaSourceMuxer.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

WebRtcTalk::Ptr WebRtcTalk::create(
    const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info, WebRtcTransport::Role role,
    WebRtcTransport::SignalingProtocols signaling_protocols) {
    WebRtcTalk::Ptr ret(new WebRtcTalk(poller, src, info), [](WebRtcTalk *ptr) {
        ptr->onDestory();
        delete ptr;
    });
    ret->setRole(role);
    ret->setSignalingProtocols(signaling_protocols);
    ret->onCreate();
    return ret;
}

WebRtcTalk::WebRtcTalk(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info)
    : WebRtcTransportImp(poller) {
    _media_info = info;
    _play_src = src;
    CHECK(src);
    _demuxer = std::make_shared<RtspDemuxer>();
}

void WebRtcTalk::onStartWebRTC() {
    auto playSrc = _play_src.lock();
    if (!playSrc) {
        onShutdown(SockException(Err_shutdown, "rtsp media source was shutdown"));
        return;
    }
    WebRtcTransportImp::onStartWebRTC();
    // 不支持simulcast
    CHECK(!_answer_sdp->supportSimulcast());
    auto sdp = _answer_sdp->toRtspSdp();
    _demuxer->loadSdp(sdp);
    auto audio_track = _demuxer->getTrack(TrackAudio, false);
    // 必须包含音频track
    CHECK(audio_track);
    audio_track->addDelegate([this](const Frame::Ptr &frame) {
        // 发送对讲语音rtp流
        _sender->inputFrame(frame);
        return true;
    });

    MediaSourceEvent::SendRtpArgs args;
    args.con_type = MediaSourceEvent::SendRtpArgs::kVoiceTalk;
    args.recv_stream_vhost = playSrc->getMediaTuple().vhost;
    args.recv_stream_app = playSrc->getMediaTuple().app;
    args.recv_stream_id = playSrc->getMediaTuple().stream;
    auto url_args = Parser::parseArgs(_media_info.params);
    args.data_type = static_cast<MediaSourceEvent::SendRtpArgs::DataType>(atoi(url_args["data_type"].data()));
    args.only_audio = true;
    args.pt = static_cast<uint8_t>(atoi(url_args["pt"].data()));
    args.ssrc = url_args["ssrc"];

    std::weak_ptr<WebRtcTalk> weak_self = static_pointer_cast<WebRtcTalk>(shared_from_this());
    _sender = std::make_shared<RtpSender>(getPoller());
    _sender->startSend(*(playSrc->getMuxer()), args, [weak_self](uint16_t local_port, const SockException &ex) {
        if (!ex) {
            return;
        }
        if (auto strong_self = weak_self.lock()) {
            strong_self->onShutdown(ex);
        }
    });

    _sender->addTrack(audio_track);
    _sender->addTrackCompleted();

    if (canSendRtp()) {
        playSrc->pause(false);
        _reader = playSrc->getRing()->attach(getPoller(), true);
        weak_ptr<WebRtcTalk> weak_self = static_pointer_cast<WebRtcTalk>(shared_from_this());
        weak_ptr<Session> weak_session = static_pointer_cast<Session>(getSession());
        _reader->setGetInfoCB([weak_session]() {
            Any ret;
            ret.set(static_pointer_cast<Session>(weak_session.lock()));
            return ret;
        });
        _reader->setReadCB([weak_self](const RtspMediaSource::RingDataType &pkt) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }

            size_t i = 0;
            pkt->for_each([&](const RtpPacket::Ptr &rtp) { strong_self->onSendRtp(rtp, ++i == pkt->size()); });
        });
        _reader->setDetachCB([weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->onShutdown(SockException(Err_shutdown, "rtsp ring buffer detached"));
        });

        _reader->setMessageCB([weak_self](const toolkit::Any &data) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            if (data.is<Buffer>()) {
                auto &buffer = data.get<Buffer>();
                // PPID 51: 文本string  [AUTO-TRANSLATED:69a8cf81]
                // PPID 51: Text string
                // PPID 53: 二进制  [AUTO-TRANSLATED:faf00c3e]
                // PPID 53: Binary
                strong_self->sendDatachannel(0, 51, buffer.data(), buffer.size());
            } else {
                WarnL << "Send unknown message type to webrtc player: " << data.type_name();
            }
        });
    }
}
void WebRtcTalk::onDestory() {
    auto duration = getDuration();
    auto bytes_usage = getBytesUsage();
    // 流量统计事件广播  [AUTO-TRANSLATED:6b0b1234]
    // Traffic statistics event broadcast
    GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);
    auto session = getSession();
    if (_reader && session) {
        WarnL << "RTC对讲(" << _media_info.shortUrl() << ")结束播放,耗时(s):" << duration;
        if (bytes_usage >= iFlowThreshold * 1024) {
            NOTICE_EMIT(BroadcastFlowReportArgs, Broadcast::kBroadcastFlowReport, _media_info, bytes_usage, duration, true, *session);
        }
    }
    WebRtcTransportImp::onDestory();
}

void WebRtcTalk::onRtcConfigure(RtcConfigure &configure) const {
    WebRtcTransportImp::onRtcConfigure(configure);
    auto playSrc = _play_src.lock();
    if (playSrc) {
        configure.setPlayRtspInfo(playSrc->getSdp());
    }

    // 不接收视频
    configure.video.direction = static_cast<RtpDirection>(static_cast<int8_t>(configure.video.direction) & ~static_cast<int8_t>(RtpDirection::recvonly));
    // 开启音频接收
    configure.audio.direction = static_cast<RtpDirection>(static_cast<int8_t>(configure.audio.direction) | static_cast<int8_t>(RtpDirection::recvonly));
}

void WebRtcTalk::onRecvRtp(MediaTrack &track, const std::string &rid, RtpPacket::Ptr rtp) {
    // rtp解析为音频，视频丢弃
    if (rtp->type == TrackAudio) {
        _demuxer->inputRtp(rtp);
    }
}


} // namespace mediakit