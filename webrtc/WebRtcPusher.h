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

namespace mediakit {

class WebRtcPusher : public WebRtcTransportImp, public MediaSourceEvent {
public:
    using Ptr = std::shared_ptr<WebRtcPusher>;
    ~WebRtcPusher() override = default;
    static Ptr create(const EventPoller::Ptr &poller, const RtspMediaSourceImp::Ptr &src,
                      const std::shared_ptr<void> &ownership, const MediaInfo &info, const ProtocolOption &option);

protected:
    ///////WebRtcTransportImp override///////
    void onStartWebRTC() override;
    void onDestory() override;
    void onRtcConfigure(RtcConfigure &configure) const override;
    void onRecvRtp(MediaTrack &track, const std::string &rid, RtpPacket::Ptr rtp) override;
    void onRtcpBye() override;
    ////  dtls相关的回调 ////
   void OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) override;

protected:
    ///////MediaSourceEvent override///////
    // 关闭
    bool close(MediaSource &sender) override;
    // 播放总人数
    int totalReaderCount(MediaSource &sender) override;
    // 获取媒体源类型
    MediaOriginType getOriginType(MediaSource &sender) const override;
    // 获取媒体源url或者文件路径
    std::string getOriginUrl(MediaSource &sender) const override;
    // 获取媒体源客户端相关信息
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;
    // 获取丢包率
    float getLossRate(MediaSource &sender,TrackType type) override;

private:
    WebRtcPusher(const EventPoller::Ptr &poller, const RtspMediaSourceImp::Ptr &src,
                 const std::shared_ptr<void> &ownership, const MediaInfo &info, const ProtocolOption &option);

private:
    bool _simulcast = false;
    //断连续推延时
    uint32_t _continue_push_ms = 0;
    //媒体相关元数据
    MediaInfo _media_info;
    //推流的rtsp源
    RtspMediaSourceImp::Ptr _push_src;
    //推流所有权
    std::shared_ptr<void> _push_src_ownership;
    //推流的rtsp源,支持simulcast
    std::unordered_map<std::string/*rid*/, RtspMediaSource::Ptr> _push_src_sim;
    std::unordered_map<std::string/*rid*/, std::shared_ptr<void> > _push_src_sim_ownership;
};

}// namespace mediakit
#endif //ZLMEDIAKIT_WEBRTCPUSHER_H
