/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTCPLAYER_H
#define ZLMEDIAKIT_WEBRTCPLAYER_H

#include "WebRtcTransport.h"

class WebRtcPlayer : public WebRtcTransportImp {
public:
    using Ptr = std::shared_ptr<WebRtcPlayer>;
    ~WebRtcPlayer() override = default;
    static Ptr create(const EventPoller::Ptr &poller, const mediakit::RtspMediaSource::Ptr &src, const mediakit::MediaInfo &info);

protected:
    ///////WebRtcTransportImp override///////
    void onStartWebRTC() override;
    void onDestory() override;
    void onRtcConfigure(RtcConfigure &configure) const override;
    void onRecvRtp(MediaTrack &track, const std::string &rid, mediakit::RtpPacket::Ptr rtp) override {};

private:
    WebRtcPlayer(const EventPoller::Ptr &poller, const mediakit::RtspMediaSource::Ptr &src, const mediakit::MediaInfo &info);

private:
    //媒体相关元数据
    mediakit::MediaInfo _media_info;
    //播放的rtsp源
    mediakit::RtspMediaSource::Ptr _play_src;
    //播放rtsp源的reader对象
    mediakit::RtspMediaSource::RingType::RingReader::Ptr _reader;
};


#endif //ZLMEDIAKIT_WEBRTCPLAYER_H
