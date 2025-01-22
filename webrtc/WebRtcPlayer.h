/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTCPLAYER_H
#define ZLMEDIAKIT_WEBRTCPLAYER_H

#include "WebRtcTransport.h"
#include "Rtsp/RtspMediaSource.h"

namespace mediakit {

class WebRtcPlayer : public WebRtcTransportImp {
public:
    using Ptr = std::shared_ptr<WebRtcPlayer>;
    static Ptr create(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info);
    MediaInfo getMediaInfo() { return _media_info; }

protected:
    ///////WebRtcTransportImp override///////
    void onStartWebRTC() override;
    void onDestory() override;
    void onRtcConfigure(RtcConfigure &configure) const override;

private:
    WebRtcPlayer(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info);

    void sendConfigFrames(uint32_t before_seq, uint32_t sample_rate, uint32_t timestamp, uint64_t ntp_timestamp);

private:
    // 媒体相关元数据  [AUTO-TRANSLATED:f4cf8045]
    // Media related metadata
    MediaInfo _media_info;
    // 播放的rtsp源  [AUTO-TRANSLATED:9963eed1]
    // Playing rtsp source
    std::weak_ptr<RtspMediaSource> _play_src;

    // rtp 直接转发情况下通常会缺少 sps/pps, 在转发 rtp 前, 先发送一次相关帧信息, 部分情况下是可以播放的  [AUTO-TRANSLATED:65fdf16a]
    // In the case of direct RTP forwarding, sps/pps is usually missing. Before forwarding RTP, send the relevant frame information once. In some cases, it can be played.
    bool _send_config_frames_once { false };

    // 播放rtsp源的reader对象  [AUTO-TRANSLATED:7b305055]
    // Reader object for playing rtsp source
    RtspMediaSource::RingType::RingReader::Ptr _reader;
};

}// namespace mediakit
#endif // ZLMEDIAKIT_WEBRTCPLAYER_H
