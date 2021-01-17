/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtspPusher : public TcpClient, public RtspSplitter, public PusherBase {
public:
    typedef std::shared_ptr<RtspPusher> Ptr;
    RtspPusher(const EventPoller::Ptr &poller,const RtspMediaSource::Ptr &src);
    ~RtspPusher() override;
    void publish(const string &url) override;
    void teardown() override;

    void setOnPublished(const Event &cb) override {
        _on_published = cb;
    }

    void setOnShutdown(const Event & cb) override{
        _on_shutdown = cb;
    }

protected:
    //for Tcpclient override
    void onRecv(const Buffer::Ptr &buf) override;
    void onConnect(const SockException &err) override;
    void onErr(const SockException &ex) override;

    //RtspSplitter override
    void onWholeRtspPacket(Parser &parser) override ;
    void onRtpPacket(const char *data,uint64_t len) override {};

private:
    void onPublishResult(const SockException &ex, bool handshake_done);

    void sendAnnounce();
    void sendSetup(unsigned int track_idx);
    void sendRecord();
    void sendOptions();
    void sendTeardown();

    void handleResAnnounce(const Parser &parser);
    void handleResSetup(const Parser &parser, unsigned int track_idx);
    bool handleAuthenticationFailure(const string &params_str);

    inline int getTrackIndexByTrackType(TrackType type);

    void sendRtpPacket(const RtspMediaSource::RingDataType & pkt) ;
    void sendRtspRequest(const string &cmd, const string &url ,const StrCaseMap &header = StrCaseMap(),const string &sdp = "" );
    void sendRtspRequest(const string &cmd, const string &url ,const std::initializer_list<string> &header,const string &sdp = "");

    void createUdpSockIfNecessary(int track_idx);
    void setSocketFlags();

private:
    unsigned int _cseq = 1;
    Rtsp::eRtpType _rtp_type = Rtsp::RTP_TCP;

    //rtsp鉴权相关
    string _nonce;
    string _realm;
    string _url;
    string _session_id;
    string _content_base;
    SdpParser _sdp_parser;
    vector<SdpTrack::Ptr> _track_vec;
    Socket::Ptr _udp_socks[2];
    //超时功能实现
    std::shared_ptr<Timer> _publish_timer;
    //心跳定时器
    std::shared_ptr<Timer> _beat_timer;
    std::weak_ptr<RtspMediaSource> _push_src;
    RtspMediaSource::RingType::RingReader::Ptr _rtsp_reader;
    //事件监听
    Event _on_shutdown;
    Event _on_published;
    function<void(const Parser&)> _on_res_func;
};

} /* namespace mediakit */
#endif //ZLMEDIAKIT_RTSPPUSHER_H
