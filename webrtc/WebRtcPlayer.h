﻿/*
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

private:
    //媒体相关元数据
    MediaInfo _media_info;
    //播放的rtsp源
    std::weak_ptr<RtspMediaSource> _play_src;
    //播放rtsp源的reader对象
    RtspMediaSource::RingType::RingReader::Ptr _reader;
};

}// namespace mediakit
#endif // ZLMEDIAKIT_WEBRTCPLAYER_H
