/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTCPUSHER_H
#define ZLMEDIAKIT_WEBRTCPUSHER_H

#include "WebRtcTransport.h"
#include "Rtsp/RtspMediaSource.h"

namespace mediakit {

class WebRtcPusher : public WebRtcTransportImp, public MediaSourceEvent {
public:
    using Ptr = std::shared_ptr<WebRtcPusher>;
    static Ptr create(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src,
                      const std::shared_ptr<void> &ownership, const MediaInfo &info, const ProtocolOption &option);

protected:
    ///////WebRtcTransportImp override///////
    void onStartWebRTC() override;
    void onDestory() override;
    void onRtcConfigure(RtcConfigure &configure) const override;
    void onRecvRtp(MediaTrack &track, const std::string &rid, RtpPacket::Ptr rtp) override;
    void onShutdown(const SockException &ex) override;
    void onRtcpBye() override;
    // //  dtls相关的回调 ////  [AUTO-TRANSLATED:31a1f32c]
    // //  dtls related callbacks ////
    void OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) override;

protected:
    ///////MediaSourceEvent override///////
    // 关闭  [AUTO-TRANSLATED:92392f02]
    // Close
    bool close(MediaSource &sender) override;
    // 播放总人数  [AUTO-TRANSLATED:c42a3161]
    // Total number of players
    int totalReaderCount(MediaSource &sender) override;
    // 获取媒体源类型  [AUTO-TRANSLATED:34290a69]
    // Get media source type
    MediaOriginType getOriginType(MediaSource &sender) const override;
    // 获取媒体源url或者文件路径  [AUTO-TRANSLATED:fa34d795]
    // Get media source url or file path
    std::string getOriginUrl(MediaSource &sender) const override;
    // 获取媒体源客户端相关信息  [AUTO-TRANSLATED:037ef910]
    // Get media source client related information
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;
    // 由于支持断连续推，存在OwnerPoller变更的可能  [AUTO-TRANSLATED:1c863b40]
    // Due to support for discontinuous pushing, there is a possibility of OwnerPoller changes
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;
    // 获取丢包率  [AUTO-TRANSLATED:ec61b378]
    // Get packet loss rate
    float getLossRate(MediaSource &sender,TrackType type) override;

private:
    WebRtcPusher(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src,
                 const std::shared_ptr<void> &ownership, const MediaInfo &info, const ProtocolOption &option);

private:
    bool _simulcast = false;
    // 断连续推延时  [AUTO-TRANSLATED:13ad578a]
    // Discontinuous pushing delay
    uint32_t _continue_push_ms = 0;
    // 媒体相关元数据  [AUTO-TRANSLATED:f4cf8045]
    // Media related metadata
    MediaInfo _media_info;
    // 推流的rtsp源  [AUTO-TRANSLATED:4f976bca]
    // Rtsp source of the stream
    RtspMediaSource::Ptr _push_src;
    // 推流所有权  [AUTO-TRANSLATED:d0ddf5c7]
    // Stream ownership
    std::shared_ptr<void> _push_src_ownership;
    // 推流的rtsp源,支持simulcast  [AUTO-TRANSLATED:44be9120]
    // Rtsp source of the stream, supports simulcast
    std::recursive_mutex _mtx;
    std::unordered_map<std::string/*rid*/, RtspMediaSource::Ptr> _push_src_sim;
    std::unordered_map<std::string/*rid*/, std::shared_ptr<void> > _push_src_sim_ownership;
};

}// namespace mediakit
#endif //ZLMEDIAKIT_WEBRTCPUSHER_H
