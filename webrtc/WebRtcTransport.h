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
#include "SctpAssociation.hpp"

//RTC配置项目
namespace RTC {
extern const std::string kPort;
extern const std::string kTimeOutSec;
}//namespace RTC

class WebRtcInterface {
public:
    WebRtcInterface() = default;
    virtual ~WebRtcInterface() = default;
    // sdp interface: in offer, out answer
    virtual std::string getAnswerSdp(const std::string &offer) = 0;
    // Identifier, use as ice_username
    virtual const std::string &getIdentifier() const = 0;
};

class WebRtcException : public WebRtcInterface {
public:
    WebRtcException(const SockException &ex) : _ex(ex) {};
    ~WebRtcException() override = default;
    // 这边用来重抛异常
    std::string getAnswerSdp(const std::string &offer) override {
        throw _ex;
    }
    const std::string &getIdentifier() const override {
        static std::string s_null;
        return s_null;
    }

private:
    SockException _ex;
};

class WebRtcTransport : public WebRtcInterface, 
    public RTC::DtlsTransport::Listener, 
    public RTC::IceServer::Listener, 
#ifdef ENABLE_SCTP
    public RTC::SctpAssociation::Listener,
#endif
    public std::enable_shared_from_this<WebRtcTransport> {
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
    std::string getAnswerSdp(const std::string &offer) override;

    /**
     * 获取对象唯一id
     */
    const std::string& getIdentifier() const override;

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

#ifdef ENABLE_SCTP
    void OnSctpAssociationConnecting(RTC::SctpAssociation* sctpAssociation) override;
    void OnSctpAssociationConnected(RTC::SctpAssociation* sctpAssociation) override;
    void OnSctpAssociationFailed(RTC::SctpAssociation* sctpAssociation) override;
    void OnSctpAssociationClosed(RTC::SctpAssociation* sctpAssociation) override;
    void OnSctpAssociationSendData(RTC::SctpAssociation* sctpAssociation, const uint8_t* data, size_t len) override;
    void OnSctpAssociationMessageReceived(RTC::SctpAssociation *sctpAssociation, uint16_t streamId, uint32_t ppid,
                                          const uint8_t *msg, size_t len) override;
#endif

protected:
    /// virtual function/api to implement in subclass ///
    // dlts密钥协商完毕后回调
    virtual void onStartWebRTC() = 0;
    // 重写此函数配置answer的生成参数
    virtual void onRtcConfigure(RtcConfigure &configure) const;
    // 检查sdp(offer,answer)合法性
    virtual void onCheckSdp(SdpType type, RtcSession &sdp) = 0;

    // 明文rtp接收函数回调
    virtual void onRtp(const char *buf, size_t len, uint64_t stamp_ms) = 0;
    // 明文rtcp接收函数回调
    virtual void onRtcp(const char *buf, size_t len) = 0;

    virtual void onShutdown(const SockException &ex) = 0;
    // 明文rtp发送函数回调, call by sendRtpPacket
    virtual void onBeforeEncryptRtp(const char *buf, int &len, void *ctx) = 0;
    // 明文rtcp发送函数回调, call by sendRtcpPacket
    virtual void onBeforeEncryptRtcp(const char *buf, int &len, void *ctx) = 0;

    // 实现udp发送函数
    virtual void onSendSockData(Buffer::Ptr buf, bool flush = true, RTC::TransportTuple *tuple = nullptr) = 0;

protected:
    // send data to network, internal call onSendSockData
    void sendSockData(const char *buf, size_t len, RTC::TransportTuple *tuple);

    RTC::TransportTuple* getSelectedTuple() const;

    void sendRtcpRemb(uint32_t ssrc, size_t bit_rate);
    void sendRtcpPli(uint32_t ssrc);

    void setRemoteDtlsFingerprint(const RtcSession &remote);
protected:
    RtcSession::Ptr _offer_sdp;
    RtcSession::Ptr _answer_sdp;

private:
    std::string _identifier;
    EventPoller::Ptr _poller;
    std::shared_ptr<RTC::IceServer> _ice_server;
    std::shared_ptr<RTC::DtlsTransport> _dtls_transport;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_send;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_recv;
    Ticker _ticker;
    //循环池
    ResourcePool<BufferRaw> _packet_pool;

#ifdef ENABLE_SCTP
    RTC::SctpAssociationImp::Ptr _sctp;
#endif
};

class RtpChannel;
// 代表一路音频或视频流的(发送和接收)
class MediaTrack {
public:
    using Ptr = std::shared_ptr<MediaTrack>;
    const RtcCodecPlan *plan_rtp;
    const RtcCodecPlan *plan_rtx;
    uint32_t offer_ssrc_rtp = 0;
    uint32_t offer_ssrc_rtx = 0;
    uint32_t answer_ssrc_rtp = 0;
    uint32_t answer_ssrc_rtx = 0;
    // sdp m line
    const RtcMedia *media;
    RtpExtContext::Ptr rtp_ext_ctx;

    //for send rtp
    NackList nack_list;
    mediakit::RtcpContext::Ptr rtcp_context_send;

    //for recv rtp
    std::unordered_map<std::string/*rid*/, std::shared_ptr<RtpChannel> > rtp_channel;
    std::shared_ptr<RtpChannel> getRtpChannel(uint32_t ssrc) const;
};

struct WrappedMediaTrack {
    MediaTrack::Ptr track;
    explicit WrappedMediaTrack(MediaTrack::Ptr ptr): track(ptr) {}
    virtual ~WrappedMediaTrack() {}
    virtual void inputRtp(const char *buf, size_t len, uint64_t stamp_ms, mediakit::RtpHeader *rtp) = 0;
};

struct WrappedRtxTrack: public WrappedMediaTrack {
    explicit WrappedRtxTrack(MediaTrack::Ptr ptr)
        : WrappedMediaTrack(std::move(ptr)) {}
    void inputRtp(const char *buf, size_t len, uint64_t stamp_ms, mediakit::RtpHeader *rtp) override;
};

class WebRtcTransportImp;

struct WrappedRtpTrack : public WrappedMediaTrack {
    explicit WrappedRtpTrack(MediaTrack::Ptr ptr, TwccContext& twcc, WebRtcTransportImp& t)
        : WrappedMediaTrack(std::move(ptr)), _twcc_ctx(twcc), _transport(t) {}
    TwccContext& _twcc_ctx;
    WebRtcTransportImp& _transport;
    void inputRtp(const char *buf, size_t len, uint64_t stamp_ms, mediakit::RtpHeader *rtp) override;
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

    void createRtpChannel(const std::string &rid, uint32_t ssrc, MediaTrack &track);

    // 发送rtp数据包，带rtcp和nack功能
    void onSendRtp(const mediakit::RtpPacket::Ptr &rtp, bool flush, bool rtx = false);
protected:
    // rtp包经排序和nack后的数据回调
    virtual void onRecvRtp(MediaTrack &track, const std::string &rid, mediakit::RtpPacket::Ptr rtp) = 0;

    WebRtcTransportImp(const EventPoller::Ptr &poller);

    void onSendSockData(Buffer::Ptr buf, bool flush = true, RTC::TransportTuple *tuple = nullptr) override;

    void onCheckSdp(SdpType type, RtcSession &sdp) override;
    void onRtcConfigure(RtcConfigure &configure) const override;

    void onRtp(const char *buf, size_t len, uint64_t stamp_ms) override;
    void onRtcp(const char *buf, size_t len) override;

    void onBeforeEncryptRtp(const char *buf, int &len, void *ctx) override;
    void onBeforeEncryptRtcp(const char *buf, int &len, void *ctx) override {};

    void onCreate() override;
    void onDestory() override;
    void onStartWebRTC() override;
    void onShutdown(const SockException &ex) override;

    void updateTicker();

private:
    void onSortedRtp(MediaTrack &track, const std::string &rid, mediakit::RtpPacket::Ptr rtp);
    void onSendNack(MediaTrack &track, const mediakit::FCI_NACK &nack, uint32_t ssrc);
    void onSendTwcc(uint32_t ssrc, const std::string &twcc_fci);

    void registerSelf();
    void unregisterSelf();
    void unrefSelf();

    void onCheckAnswer(RtcSession &sdp);

private:
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

    //当前选中的udp链接
    Session::Ptr _selected_session;
    //链接迁移前后使用过的udp链接
    std::unordered_map<Session *, std::weak_ptr<Session> > _history_sessions;
    
    //twcc rtcp发送上下文对象
    TwccContext _twcc_ctx;

    //根据发送rtp的track类型获取相关信息
    MediaTrack::Ptr _type_to_track[2];
    uint16_t _rtx_seq[2] = {0, 0};

    //根据rtcp的ssrc获取相关信息，收发rtp和rtx的ssrc都会记录
    std::unordered_map<uint32_t/*ssrc*/, MediaTrack::Ptr> _ssrc_to_track;
    //根据接收rtp的pt获取相关信息
    std::unordered_map<uint8_t/*pt*/, std::unique_ptr<WrappedMediaTrack>> _pt_to_track;
};

class WebRtcTransportManager {
public:
    friend class WebRtcTransportImp;
    static WebRtcTransportManager &Instance();
    WebRtcTransportImp::Ptr getItem(const std::string &key);

private:
    WebRtcTransportManager() = default;
    void addItem(const std::string &key, const WebRtcTransportImp::Ptr &ptr);
    void removeItem(const std::string &key);

private:
    mutable std::mutex _mtx;
    std::unordered_map<std::string, std::weak_ptr<WebRtcTransportImp> > _map;
};

// 抽象通过http传递的参数
class WebRtcArgs {
public:
    WebRtcArgs() = default;
    virtual ~WebRtcArgs() = default;

    virtual variant operator[](const std::string &key) const = 0;
};

class WebRtcPluginManager {
public:
    using onCreateRtc = std::function<void(const WebRtcInterface &rtc)>;
    using Plugin = std::function<void(Session &sender, const std::string &offer, const WebRtcArgs &args, const onCreateRtc &cb)>;

    static WebRtcPluginManager &Instance();

    void registerPlugin(const std::string &type, Plugin cb);
    // 抽象出http接口，给出 offer sdp，创建 WebRtcInterface, 然后响应 answer sdp
    void getAnswerSdp(Session &sender, const std::string &type, const std::string &offer, const WebRtcArgs &args, const onCreateRtc &cb);

private:
    WebRtcPluginManager() = default;

private:
    mutable std::mutex _mtx_creator;
    std::unordered_map<std::string, Plugin> _map_creator;
};