/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTC_TALK_H
#define ZLMEDIAKIT_WEBRTC_TALK_H

#include "WebRtcTransport.h"
#include "Rtsp/RtspMediaSource.h"
#include "Rtsp/RtspDemuxer.h"
#include "Rtp/RtpSender.h"

namespace mediakit {

class WebRtcTalk : public WebRtcTransportImp {
public:
    using Ptr = std::shared_ptr<WebRtcTalk>;
    static Ptr create(const toolkit::EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info,
                      WebRtcTransport::Role role, WebRtcTransport::SignalingProtocols signaling_protocols);

protected:
    ///////WebRtcTransportImp override///////
    void onStartWebRTC() override;
    void onDestory() override;
    void onRtcConfigure(RtcConfigure &configure) const override;
    void onRecvRtp(MediaTrack &track, const std::string &rid, RtpPacket::Ptr rtp) override;

private:
    WebRtcTalk(const toolkit::EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info);

private:
    // 媒体相关元数据  [AUTO-TRANSLATED:f4cf8045]
    // Media related metadata
    MediaInfo _media_info;
    // 播放的rtsp源  [AUTO-TRANSLATED:9963eed1]
    // Playing rtsp source
    std::weak_ptr<RtspMediaSource> _play_src;

    // 播放rtsp源的reader对象  [AUTO-TRANSLATED:7b305055]
    // Reader object for playing rtsp source
    RtspMediaSource::RingType::RingReader::Ptr _reader;

    // 解析对讲语音rtp流为帧数据
    RtspDemuxer::Ptr _demuxer;
    // 打包语音帧数据为特定rtp并回复过去
    RtpSender::Ptr _sender;
};

}// namespace mediakit
#endif // ZLMEDIAKIT_WEBRTC_TALK_H
