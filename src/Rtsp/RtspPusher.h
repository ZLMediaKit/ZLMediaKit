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
    virtual ~RtspPusher();

    void publish(const string &strUrl) override;

    void teardown() override;

    void setOnPublished(const Event &cb) override {
        _onPublished = cb;
    }

    void setOnShutdown(const Event & cb) override{
        _onShutdown = cb;
    }
protected:
    //for Tcpclient override
    void onRecv(const Buffer::Ptr &pBuf) override;
    void onConnect(const SockException &err) override;
    void onErr(const SockException &ex) override;

    //RtspSplitter override
    void onWholeRtspPacket(Parser &parser) override ;
    void onRtpPacket(const char *data,uint64_t len) override {};
private:
    void onPublishResult(const SockException &ex, bool handshakeCompleted);

    void sendAnnounce();
    void sendSetup(unsigned int uiTrackIndex);
    void sendRecord();
    void sendOptions();

    void handleResAnnounce(const Parser &parser);
    void handleResSetup(const Parser &parser, unsigned int uiTrackIndex);
    bool handleAuthenticationFailure(const string &paramsStr);

    inline int getTrackIndexByTrackType(TrackType type);

    void sendRtpPacket(const RtspMediaSource::RingDataType & pkt) ;
    void sendRtspRequest(const string &cmd, const string &url ,const StrCaseMap &header = StrCaseMap(),const string &sdp = "" );
    void sendRtspRequest(const string &cmd, const string &url ,const std::initializer_list<string> &header,const string &sdp = "");

    void createUdpSockIfNecessary(int track_idx);
    void setSocketFlags();
private:
    //rtsp鉴权相关
    string _rtspMd5Nonce;
    string _rtspRealm;

    //超时功能实现
    std::shared_ptr<Timer> _pPublishTimer;
    //源
    std::weak_ptr<RtspMediaSource> _pMediaSrc;
    RtspMediaSource::RingType::RingReader::Ptr _pRtspReader;
    //事件监听
    Event _onShutdown;
    Event _onPublished;

    string _strUrl;
    SdpParser _sdpParser;
    vector<SdpTrack::Ptr> _aTrackInfo;
    string _strSession;
    unsigned int _uiCseq = 1;
    string _strContentBase;
    Rtsp::eRtpType _eType = Rtsp::RTP_TCP;
    Socket::Ptr _apUdpSock[2];
    function<void(const Parser&)> _onHandshake;
    //心跳定时器
    std::shared_ptr<Timer> _pBeatTimer;


};

} /* namespace mediakit */
#endif //ZLMEDIAKIT_RTSPPUSHER_H
