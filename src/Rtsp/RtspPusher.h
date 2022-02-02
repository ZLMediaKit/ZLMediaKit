/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTSPPUSHER_H
#define ZLMEDIAKIT_RTSPPUSHER_H

#include <string>
#include <memory>
#include "RtspMediaSource.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Poller/Timer.h"
#include "Network/Socket.h"
#include "Network/TcpClient.h"
#include "RtspSplitter.h"
#include "Pusher/PusherBase.h"
#include "Rtcp/RtcpContext.h"

namespace mediakit {

class RtspPusher : public toolkit::TcpClient, public RtspSplitter, public PusherBase {
public:
    typedef std::shared_ptr<RtspPusher> Ptr;
    RtspPusher(const toolkit::EventPoller::Ptr &poller,const RtspMediaSource::Ptr &src);
    ~RtspPusher() override;
    void publish(const std::string &url) override;
    void teardown() override;

protected:
    //for Tcpclient override
    void onRecv(const toolkit::Buffer::Ptr &buf) override;
    void onConnect(const toolkit::SockException &err) override;
    void onErr(const toolkit::SockException &ex) override;

    //RtspSplitter override
    void onWholeRtspPacket(Parser &parser) override ;
    void onRtpPacket(const char *data,size_t len) override;

    virtual void onRtcpPacket(int track_idx, SdpTrack::Ptr &track, uint8_t *data, size_t len);

private:
    void onPublishResult_l(const toolkit::SockException &ex, bool handshake_done);

    void sendAnnounce();
    void sendSetup(unsigned int track_idx);
    void sendRecord();
    void sendOptions();
    void sendTeardown();

    void handleResAnnounce(const Parser &parser);
    void handleResSetup(const Parser &parser, unsigned int track_idx);
    bool handleAuthenticationFailure(const std::string &params_str);

    int getTrackIndexByInterleaved(int interleaved) const;
    int getTrackIndexByTrackType(TrackType type) const;

    void sendRtpPacket(const RtspMediaSource::RingDataType & pkt) ;
    void sendRtspRequest(const std::string &cmd, const std::string &url ,const StrCaseMap &header = StrCaseMap(),const std::string &sdp = "" );
    void sendRtspRequest(const std::string &cmd, const std::string &url ,const std::initializer_list<std::string> &header,const std::string &sdp = "");

    void createUdpSockIfNecessary(int track_idx);
    void setSocketFlags();
    void updateRtcpContext(const RtpPacket::Ptr &pkt);

private:
    unsigned int _cseq = 1;
    Rtsp::eRtpType _rtp_type = Rtsp::RTP_TCP;

    //rtsp鉴权相关
    std::string _nonce;
    std::string _realm;
    std::string _url;
    std::string _session_id;
    std::string _content_base;
    SdpParser _sdp_parser;
    std::vector<SdpTrack::Ptr> _track_vec;
    //RTP端口,trackid idx 为数组下标
    toolkit::Socket::Ptr _rtp_sock[2];
    //RTCP端口,trackid idx 为数组下标
    toolkit::Socket::Ptr _rtcp_sock[2];
    //超时功能实现
    std::shared_ptr<toolkit::Timer> _publish_timer;
    //心跳定时器
    std::shared_ptr<toolkit::Timer> _beat_timer;
    std::weak_ptr<RtspMediaSource> _push_src;
    RtspMediaSource::RingType::RingReader::Ptr _rtsp_reader;
    std::function<void(const Parser&)> _on_res_func;
    ////////// rtcp ////////////////
    //rtcp发送时间,trackid idx 为数组下标
    toolkit::Ticker _rtcp_send_ticker[2];
    //统计rtp并发送rtcp
    std::vector<RtcpContext::Ptr> _rtcp_context;
};

using RtspPusherImp = PusherImp<RtspPusher, PusherBase>;

} /* namespace mediakit */
#endif //ZLMEDIAKIT_RTSPPUSHER_H
