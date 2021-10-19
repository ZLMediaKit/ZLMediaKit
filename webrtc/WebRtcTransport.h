/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include <memory>
#include <string>
#include "DtlsTransport.hpp"
#include "IceServer.hpp"
#include "SrtpSession.hpp"
#include "StunPacket.hpp"
#include "Sdp.h"
#include "Poller/EventPoller.h"
#include "Network/Socket.h"
#include "Rtsp/RtspMediaSourceImp.h"
#include "Rtcp/RtcpContext.h"
#include "Rtcp/RtcpFCI.h"
#include "Nack.h"
#include "Network/Session.h"
#include "TwccContext.h"

using namespace toolkit;
using namespace mediakit;

//RTC配置项目
namespace RTC {
extern const string kPort;
extern const string kTimeOutSec;
}//namespace RTC

class WebRtcInterface {
public:
    WebRtcInterface() = default;
    virtual ~WebRtcInterface() = default;
    virtual string getAnswerSdp(const string &offer) = 0;
    virtual const string &getIdentifier() const = 0;
};

class WebRtcException : public WebRtcInterface {
public:
    WebRtcException(const SockException &ex) : _ex(ex) {};
    ~WebRtcException() override = default;
    string getAnswerSdp(const string &offer) override {
        throw _ex;
    }
    const string &getIdentifier() const override {
        static string s_null;
        return s_null;
    }

private:
    SockException _ex;
};

class WebRtcTransport : public WebRtcInterface, public RTC::DtlsTransport::Listener, public RTC::IceServer::Listener, public std::enable_shared_from_this<WebRtcTransport> {
public:
    using Ptr = std::shared_ptr<WebRtcTransport>;
    WebRtcTransport(const EventPoller::Ptr &poller);
    ~WebRtcTransport() override = default;

    /**
     * 创建对象
     */
    virtual void onCreate();

    /**
     * 销毁对象
     */
    virtual void onDestory();

    /**
     * 创建webrtc answer sdp
     * @param offer offer sdp
     * @return answer sdp
     */
    string getAnswerSdp(const string &offer) override;

    /**
     * 获取对象唯一id
     */
    const string& getIdentifier() const override;

    /**
     * socket收到udp数据
     * @param buf 数据指针
     * @param len 数据长度
     * @param tuple 数据来源
     */
    void inputSockData(char *buf, int len, RTC::TransportTuple *tuple);

    /**
     * 发送rtp
     * @param buf rtcp内容
     * @param len rtcp长度
     * @param flush 是否flush socket
     * @param ctx 用户指针
     */
    void sendRtpPacket(const char *buf, int len, bool flush, void *ctx = nullptr);
    void sendRtcpPacket(const char *buf, int len, bool flush, void *ctx = nullptr);

    const EventPoller::Ptr& getPoller() const;

protected:
    ////  dtls相关的回调 ////
    void OnDtlsTransportConnecting(const RTC::DtlsTransport *dtlsTransport) override;
    void OnDtlsTransportConnected(const RTC::DtlsTransport *dtlsTransport,
                                  RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
                                  uint8_t *srtpLocalKey,
                                  size_t srtpLocalKeyLen,
                                  uint8_t *srtpRemoteKey,
                                  size_t srtpRemoteKeyLen,
                                  std::string &remoteCert) override;

    void OnDtlsTransportFailed(const RTC::DtlsTransport *dtlsTransport) override;
    void OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) override;
    void OnDtlsTransportSendData(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) override;
    void OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) override;

protected:
    //// ice相关的回调 ///
    void OnIceServerSendStunPacket(const RTC::IceServer *iceServer, const RTC::StunPacket *packet, RTC::TransportTuple *tuple) override;
    void OnIceServerSelectedTuple(const RTC::IceServer *iceServer, RTC::TransportTuple *tuple) override;
    void OnIceServerConnected(const RTC::IceServer *iceServer) override;
    void OnIceServerCompleted(const RTC::IceServer *iceServer) override;
    void OnIceServerDisconnected(const RTC::IceServer *iceServer) override;

protected:
    virtual void onStartWebRTC() = 0;
    virtual void onRtcConfigure(RtcConfigure &configure) const;
    virtual void onCheckSdp(SdpType type, RtcSession &sdp) = 0;
    virtual void onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst, bool flush = true) = 0;

    virtual void onRtp(const char *buf, size_t len, uint64_t stamp_ms) = 0;
    virtual void onRtcp(const char *buf, size_t len) = 0;
    virtual void onShutdown(const SockException &ex) = 0;
    virtual void onBeforeEncryptRtp(const char *buf, int &len, void *ctx) = 0;
    virtual void onBeforeEncryptRtcp(const char *buf, int &len, void *ctx) = 0;

protected:
    RTC::TransportTuple* getSelectedTuple() const;
    void sendRtcpRemb(uint32_t ssrc, size_t bit_rate);
    void sendRtcpPli(uint32_t ssrc);

private:
    void onSendSockData(const char *buf, size_t len, bool flush = true);
    void setRemoteDtlsFingerprint(const RtcSession &remote);

protected:
    RtcSession::Ptr _offer_sdp;
    RtcSession::Ptr _answer_sdp;

private:
    uint8_t _srtp_buf[2000];
    string _identifier;
    EventPoller::Ptr _poller;
    std::shared_ptr<RTC::IceServer> _ice_server;
    std::shared_ptr<RTC::DtlsTransport> _dtls_transport;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_send;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_recv;
    Ticker _ticker;
};

class RtpChannel;
class MediaTrack {
public:
    using Ptr = std::shared_ptr<MediaTrack>;
    const RtcCodecPlan *plan_rtp;
    const RtcCodecPlan *plan_rtx;
    uint32_t offer_ssrc_rtp = 0;
    uint32_t offer_ssrc_rtx = 0;
    uint32_t answer_ssrc_rtp = 0;
    uint32_t answer_ssrc_rtx = 0;
    const RtcMedia *media;
    RtpExtContext::Ptr rtp_ext_ctx;

    //for send rtp
    NackList nack_list;
    RtcpContext::Ptr rtcp_context_send;

    //for recv rtp
    unordered_map<string/*rid*/, std::shared_ptr<RtpChannel> > rtp_channel;
    std::shared_ptr<RtpChannel> getRtpChannel(uint32_t ssrc) const;
};

class WebRtcTransportImp : public WebRtcTransport {
public:
    using Ptr = std::shared_ptr<WebRtcTransportImp>;
    ~WebRtcTransportImp() override;

    void setSession(Session::Ptr session);
    const Session::Ptr& getSession() const;
    uint64_t getBytesUsage() const;
    uint64_t getDuration() const;
    bool canSendRtp() const;
    bool canRecvRtp() const;
    void onSendRtp(const RtpPacket::Ptr &rtp, bool flush, bool rtx = false);

protected:
    WebRtcTransportImp(const EventPoller::Ptr &poller);
    void onStartWebRTC() override;
    void onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst, bool flush = true) override;
    void onCheckSdp(SdpType type, RtcSession &sdp) override;
    void onRtcConfigure(RtcConfigure &configure) const override;

    void onRtp(const char *buf, size_t len, uint64_t stamp_ms) override;
    void onRtcp(const char *buf, size_t len) override;
    void onBeforeEncryptRtp(const char *buf, int &len, void *ctx) override;
    void onBeforeEncryptRtcp(const char *buf, int &len, void *ctx) override {};
    void onCreate() override;
    void onDestory() override;
    void onShutdown(const SockException &ex) override;
    virtual void onRecvRtp(MediaTrack &track, const string &rid, RtpPacket::Ptr rtp) = 0;
    void updateTicker();

private:
    SdpAttrCandidate::Ptr getIceCandidate() const;
    void onSortedRtp(MediaTrack &track, const string &rid, RtpPacket::Ptr rtp);
    void onSendNack(MediaTrack &track, const FCI_NACK &nack, uint32_t ssrc);
    void onSendTwcc(uint32_t ssrc, const string &twcc_fci);
    void createRtpChannel(const string &rid, uint32_t ssrc, MediaTrack &track);
    void registerSelf();
    void unregisterSelf();
    void unrefSelf();
    void onCheckAnswer(RtcSession &sdp);

private:
    uint16_t _rtx_seq[2] = {0, 0};
    //用掉的总流量
    uint64_t _bytes_usage = 0;
    //保持自我强引用
    Ptr _self;
    //检测超时的定时器
    Timer::Ptr _timer;
    //刷新计时器
    Ticker _alive_ticker;
    //pli rtcp计时器
    Ticker _pli_ticker;
    //udp session
    Session::Ptr _session;
    //twcc rtcp发送上下文对象
    TwccContext _twcc_ctx;
    //根据发送rtp的track类型获取相关信息
    MediaTrack::Ptr _type_to_track[2];
    //根据rtcp的ssrc获取相关信息，收发rtp和rtx的ssrc都会记录
    unordered_map<uint32_t/*ssrc*/, MediaTrack::Ptr> _ssrc_to_track;
    //根据接收rtp的pt获取相关信息
    unordered_map<uint8_t/*pt*/, std::pair<bool/*is rtx*/,MediaTrack::Ptr> > _pt_to_track;
};

class WebRtcTransportManager {
public:
    friend class WebRtcTransportImp;
    static WebRtcTransportManager &Instance();
    WebRtcTransportImp::Ptr getItem(const string &key);

private:
    WebRtcTransportManager() = default;
    void addItem(const string &key, const WebRtcTransportImp::Ptr &ptr);
    void removeItem(const string &key);

private:
    mutable mutex _mtx;
    unordered_map<string, weak_ptr<WebRtcTransportImp> > _map;
};

class WebRtcArgs {
public:
    WebRtcArgs() = default;
    virtual ~WebRtcArgs() = default;

    virtual variant operator[](const string &key) const = 0;
};

class WebRtcPluginManager {
public:
    using onCreateRtc = function<void(const WebRtcInterface &rtc)>;
    using Plugin = function<void(Session &sender, const string &offer, const WebRtcArgs &args, const onCreateRtc &cb)>;

    static WebRtcPluginManager &Instance();

    void registerPlugin(const string &type, Plugin cb);
    void getAnswerSdp(Session &sender, const string &type, const string &offer, const WebRtcArgs &args, const onCreateRtc &cb);

private:
    WebRtcPluginManager() = default;

private:
    mutable mutex _mtx_creator;
    unordered_map<string, Plugin> _map_creator;
};