/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTCPUSHER_H
#define ZLMEDIAKIT_WEBRTCPUSHER_H

#include "WebRtcTransport.h"

class WebRtcPusher : public WebRtcTransportImp, public MediaSourceEvent {
public:
    using Ptr = std::shared_ptr<WebRtcPusher>;
    ~WebRtcPusher() override = default;
    static Ptr create(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info);

protected:
    ///////WebRtcTransportImp override///////
    void onStartWebRTC() override;
    void onDestory() override;
    void onRtcConfigure(RtcConfigure &configure) const override;
    void onRecvRtp(MediaTrack &track, const string &rid, RtpPacket::Ptr rtp) override;

protected:
    ///////MediaSourceEvent override///////
    // 关闭
    bool close(MediaSource &sender, bool force) override;
    // 播放总人数
    int totalReaderCount(MediaSource &sender) override;
    // 获取媒体源类型
    MediaOriginType getOriginType(MediaSource &sender) const override;
    // 获取媒体源url或者文件路径
    string getOriginUrl(MediaSource &sender) const override;
    // 获取媒体源客户端相关信息
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;

private:
    WebRtcPusher(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info);

private:
    bool _simulcast = false;
    //媒体相关元数据
    MediaInfo _media_info;
    //推流的rtsp源
    RtspMediaSource::Ptr _push_src;
    //推流的rtsp源,支持simulcast
    unordered_map<string/*rid*/, RtspMediaSource::Ptr> _push_src_simulcast;
};

#endif //ZLMEDIAKIT_WEBRTCPUSHER_H
