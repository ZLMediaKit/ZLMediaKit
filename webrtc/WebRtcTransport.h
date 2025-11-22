/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTC_TRANSPORT_H
#define ZLMEDIAKIT_WEBRTC_TRANSPORT_H

#include <memory>
#include <string>
#include <functional>
#include "DtlsTransport.hpp"
#include "IceTransport.hpp"
#include "SrtpSession.hpp"
#include "StunPacket.hpp"
#include "Sdp.h"
#include "Util/mini.h"
#include "Poller/EventPoller.h"
#include "Network/Socket.h"
#include "Network/Session.h"
#include "Nack.h"
#include "TwccContext.h"
#include "SctpAssociation.hpp"
#include "Rtcp/RtcpContext.h"
#include "Rtsp/RtspMediaSource.h"

using namespace RTC;
namespace mediakit {

// ICE transport policy enum
enum class IceTransportPolicy {
    kAll = 0,        // 不限制，支持所有连接类型（默认）
    kRelayOnly = 1,  // 仅支持Relay转发
    kP2POnly = 2     // 仅支持P2P直连
};

// RTC配置项目  [AUTO-TRANSLATED:65784416]
// RTC configuration project
namespace Rtc {
extern const std::string kPort;
extern const std::string kTcpPort;
extern const std::string kTimeOutSec;
extern const std::string kSignalingPort;
extern const std::string kSignalingSslPort;
extern const std::string kIcePort;
extern const std::string kIceTcpPort;
extern const std::string kEnableTurn;
extern const std::string kIceTransportPolicy;
extern const std::string kIceUfrag;
extern const std::string kIcePwd;
extern const std::string kExternIP;
extern const std::string kInterfaces;
}//namespace RTC

class WebRtcInterface {
public:
    virtual ~WebRtcInterface() = default;
    virtual std::string getAnswerSdp(const std::string &offer) = 0;
    virtual std::string createOfferSdp() = 0;
    virtual void setAnswerSdp(const std::string &answer) = 0;
    virtual const std::string& getIdentifier() const = 0;
    virtual const std::string& deleteRandStr() const { static std::string s_null; return s_null; }
    virtual void setIceCandidate(std::vector<SdpAttrCandidate> cands) {}
    virtual void setLocalIp(std::string localIp) {}
    virtual void setPreferredTcp(bool flag) {}

    using onGatheringCandidateCB = std::function<void(const std::string& transport_identifier, const std::string& candidate, const std::string& ufrag, const std::string& pwd)>;
    virtual void gatheringCandidate(IceServerInfo::Ptr ice_server, onGatheringCandidateCB cb = nullptr) = 0;
};

class WebRtcException : public WebRtcInterface {
public:
    WebRtcException(const toolkit::SockException &ex) : _ex(ex) {};

    std::string createOfferSdp() override {
        throw _ex;
    }

    std::string getAnswerSdp(const std::string &offer) override {
        throw _ex;
    }

    void setAnswerSdp(const std::string &answer) override {
        throw _ex;
    }

    void gatheringCandidate(IceServerInfo::Ptr ice_server, onGatheringCandidateCB cb = nullptr) override {
        throw _ex;
    }

    const std::string &getIdentifier() const override {
        static std::string s_null;
        return s_null;
    }

private:
    toolkit::SockException _ex;
};

class WebRtcTransport : public WebRtcInterface, public RTC::DtlsTransport::Listener, public IceTransport::Listener, public std::enable_shared_from_this<WebRtcTransport>
#ifdef ENABLE_SCTP
    , public RTC::SctpAssociation::Listener
#endif
{
public:
    enum class Role {
        NONE = 0,
        CLIENT,
        PEER,
    };
    static const char* RoleStr(Role role);

    enum class SignalingProtocols {
        Invalid   = -1,
        WHEP_WHIP = 0,
        WEBSOCKET = 1,  //FOR P2P
    };
    static const char* SignalingProtocolsStr(SignalingProtocols protocol);

    using WeakPtr = std::weak_ptr<WebRtcTransport>;
    using Ptr = std::shared_ptr<WebRtcTransport>;
    WebRtcTransport(const toolkit::EventPoller::Ptr &poller);

    virtual void onCreate();

    virtual void onDestory();

    std::string getAnswerSdp(const std::string &offer) override;
    void setAnswerSdp(const std::string &answer) override;

    const RtcSession::Ptr& answerSdp() const {
        return _answer_sdp;
    }

    std::string createOfferSdp() override;

    const std::string& getIdentifier() const override;
    const std::string& deleteRandStr() const override;

    void inputSockData(const char *buf, int len, const toolkit::SocketHelper::Ptr& socket, struct sockaddr *addr = nullptr, int addr_len = 0);
    void inputSockData(const char *buf, int len, const IceTransport::Pair::Ptr& pair = nullptr);
    void sendRtpPacket(const char *buf, int len, bool flush, void *ctx = nullptr);
    void sendRtcpPacket(const char *buf, int len, bool flush, void *ctx = nullptr);
    void sendDatachannel(uint16_t streamId, uint32_t ppid, const char *msg, size_t len);

    const toolkit::EventPoller::Ptr &getPoller() const { return _poller; }
    void setPoller(toolkit::EventPoller::Ptr poller) { _poller = std::move(poller); }

    toolkit::Session::Ptr getSession() const;
    void removePair(const toolkit::SocketHelper *socket);

    Role getRole() const { return _role; }
    void setRole(Role role) { _role = role; }

    SignalingProtocols getSignalingProtocols() const { return _signaling_protocols; }
    void setSignalingProtocols(SignalingProtocols signaling_protocols) { _signaling_protocols = signaling_protocols; }

    float getTimeOutSec();

    void getTransportInfo(const std::function<void(Json::Value)> &callback) const;
    size_t getRecvSpeed() const { return _ice_agent ? _ice_agent->getRecvSpeed() : 0; }
    size_t getRecvTotalBytes() const { return _ice_agent ? _ice_agent->getRecvTotalBytes() : 0; }
    size_t getSendSpeed() const { return _ice_agent ? _ice_agent->getSendSpeed() : 0; }
    size_t getSendTotalBytes() const { return _ice_agent ? _ice_agent->getSendTotalBytes() : 0; }

    void setOnShutdown(std::function<void(const toolkit::SockException &ex)> cb);

    void gatheringCandidate(IceServerInfo::Ptr ice_server, onGatheringCandidateCB cb = nullptr) override;
    void connectivityCheck(SdpAttrCandidate candidate_attr, const std::string &ufrag, const std::string &pwd);
    void connectivityCheckForSFU();

    void setOnStartWebRTC(std::function<void()> on_start);

protected:
    // DtlsTransport::Listener; dtls相关的回调
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
    // ice相关的回调; IceTransport::Listener.
    void onIceTransportRecvData(const toolkit::Buffer::Ptr& buffer, const IceTransport::Pair::Ptr& pair) override;
    void onIceTransportGatheringCandidate(const IceTransport::Pair::Ptr& pair, const CandidateInfo& candidate) override;
    void onIceTransportCompleted() override;
    void onIceTransportDisconnected() override;

    // SctpAssociation::Listener
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
    virtual void onStartWebRTC() = 0;
    virtual void onRtcConfigure(RtcConfigure &configure) const;
    virtual void onCheckSdp(SdpType type, RtcSession &sdp) = 0;
    virtual void onSendSockData(toolkit::Buffer::Ptr buf, bool flush = true, const IceTransport::Pair::Ptr& pair = nullptr) = 0;

    virtual void onRtp(const char *buf, size_t len, uint64_t stamp_ms) = 0;
    virtual void onRtcp(const char *buf, size_t len) = 0;
    virtual void onShutdown(const toolkit::SockException &ex);
    virtual void onBeforeEncryptRtp(const char *buf, int &len, void *ctx) = 0;
    virtual void onBeforeEncryptRtcp(const char *buf, int &len, void *ctx) = 0;
    virtual void onRtcpBye() = 0;

protected:
    void sendRtcpRemb(uint32_t ssrc, size_t bit_rate);
    void sendRtcpPli(uint32_t ssrc);

private:
    void sendSockData(const char *buf, size_t len, const IceTransport::Pair::Ptr& pair = nullptr);
    void setRemoteDtlsFingerprint(SdpType type, const RtcSession &remote);

protected:
    SignalingProtocols  _signaling_protocols = SignalingProtocols::WHEP_WHIP;
    Role _role = Role::PEER;
    RtcSession::Ptr _offer_sdp;
    RtcSession::Ptr _answer_sdp;

    IceAgent::Ptr _ice_agent;
    onGatheringCandidateCB _on_gathering_candidate = nullptr;

private:
    mutable std::string _delete_rand_str;
    std::string _identifier;
    toolkit::EventPoller::Ptr _poller;
    DtlsTransport::Ptr  _dtls_transport;
    SrtpSession::Ptr _srtp_session_send;
    SrtpSession::Ptr _srtp_session_recv;
    toolkit::Ticker _ticker;
    // 循环池  [AUTO-TRANSLATED:b7059f37]
    // Cycle pool
    toolkit::ResourcePool<toolkit::BufferRaw> _packet_pool;

    //超时功能实现
    toolkit::Ticker _recv_ticker;
    std::shared_ptr<toolkit::Timer> _check_timer;
    std::function<void()> _on_start;
    std::function<void(const toolkit::SockException &ex)> _on_shutdown;

#ifdef ENABLE_SCTP
    RTC::SctpAssociationImp::Ptr _sctp;
#endif
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
    std::unordered_map<std::string/*rid*/, std::shared_ptr<RtpChannel> > rtp_channel;
    std::shared_ptr<RtpChannel> getRtpChannel(uint32_t ssrc) const;
};

struct WrappedMediaTrack {
    MediaTrack::Ptr track;
    explicit WrappedMediaTrack(MediaTrack::Ptr ptr): track(ptr) {}
    virtual ~WrappedMediaTrack() {}
    virtual void inputRtp(const char *buf, size_t len, uint64_t stamp_ms, RtpHeader *rtp) = 0;
};

struct WrappedRtxTrack: public WrappedMediaTrack {
    explicit WrappedRtxTrack(MediaTrack::Ptr ptr)
        : WrappedMediaTrack(std::move(ptr)) {}
    void inputRtp(const char *buf, size_t len, uint64_t stamp_ms, RtpHeader *rtp) override;
};

class WebRtcTransportImp;

struct WrappedRtpTrack : public WrappedMediaTrack {
    explicit WrappedRtpTrack(MediaTrack::Ptr ptr, TwccContext& twcc, WebRtcTransportImp& t)
        : WrappedMediaTrack(std::move(ptr))
        , _twcc_ctx(twcc)
        , _transport(t) {}
    TwccContext& _twcc_ctx;
    WebRtcTransportImp& _transport;
    void inputRtp(const char *buf, size_t len, uint64_t stamp_ms, RtpHeader *rtp) override;
};

class WebRtcTransportImp : public WebRtcTransport {
public:
    using Ptr = std::shared_ptr<WebRtcTransportImp>;
    ~WebRtcTransportImp() override;

    uint64_t getBytesUsage() const;
    uint64_t getDuration() const;
    bool canSendRtp() const;
    bool canRecvRtp() const;
    bool canSendRtp(const RtcMedia& media) const;
    bool canRecvRtp(const RtcMedia& media) const;
    void onSendRtp(const RtpPacket::Ptr &rtp, bool flush, bool rtx = false);

    void createRtpChannel(const std::string &rid, uint32_t ssrc, MediaTrack &track);
    void safeShutdown(const toolkit::SockException &ex);

    void setPreferredTcp(bool flag) override;
    void setLocalIp(std::string local_ip) override;
    void setIceCandidate(std::vector<SdpAttrCandidate> cands) override;

protected:

    // // ice相关的回调 ///  [AUTO-TRANSLATED:30abf693]
    // // ice related callbacks ///

    WebRtcTransportImp(const toolkit::EventPoller::Ptr &poller);
    void OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) override;
    void onStartWebRTC() override;
    void onSendSockData(toolkit::Buffer::Ptr buf, bool flush = true, const IceTransport::Pair::Ptr& pair = nullptr) override;
    void onCheckSdp(SdpType type, RtcSession &sdp) override;
    void onRtcConfigure(RtcConfigure &configure) const override;

    void onRtp(const char *buf, size_t len, uint64_t stamp_ms) override;
    void onRtcp(const char *buf, size_t len) override;
    void onBeforeEncryptRtp(const char *buf, int &len, void *ctx) override;
    void onBeforeEncryptRtcp(const char *buf, int &len, void *ctx) override {};
    void onCreate() override;
    void onDestory() override;
    void onShutdown(const toolkit::SockException &ex) override;
    virtual void onRecvRtp(MediaTrack &track, const std::string &rid, RtpPacket::Ptr rtp) {}
    void updateTicker();
    float getLossRate(TrackType type);
    void onRtcpBye() override;

private:
    void onSortedRtp(MediaTrack &track, const std::string &rid, RtpPacket::Ptr rtp);
    void onSendNack(MediaTrack &track, const FCI_NACK &nack, uint32_t ssrc);
    void onSendTwcc(uint32_t ssrc, const std::string &twcc_fci);

    void registerSelf();
    void unregisterSelf();
    void unrefSelf();
    void onCheckAnswer(RtcSession &sdp);

private:
    bool _preferred_tcp = false;
    uint16_t _rtx_seq[2] = {0, 0};
    // 用掉的总流量  [AUTO-TRANSLATED:713b61c9]
    // Total traffic used
    uint64_t _bytes_usage = 0;
    // 保持自我强引用  [AUTO-TRANSLATED:c2dc228f]
    // Keep self strong reference
    Ptr _self;
    // 检测超时的定时器  [AUTO-TRANSLATED:a58e1388]
    // Timeout detection timer
    toolkit::Timer::Ptr _timer;
    // 刷新计时器  [AUTO-TRANSLATED:61eb11e5]
    // Refresh timer
    toolkit::Ticker _alive_ticker;
    // pli rtcp计时器  [AUTO-TRANSLATED:a1a5fd18]
    // pli rtcp timer
    toolkit::Ticker _pli_ticker;

    toolkit::Ticker _rtcp_sr_send_ticker;
    toolkit::Ticker _rtcp_rr_send_ticker;

    // twcc rtcp发送上下文对象  [AUTO-TRANSLATED:aef6476a]
    // twcc rtcp send context object
    TwccContext _twcc_ctx;
    // 根据发送rtp的track类型获取相关信息  [AUTO-TRANSLATED:ff31c272]
    // Get relevant information based on the track type of the sent rtp
    MediaTrack::Ptr _type_to_track[2];
    // 根据rtcp的ssrc获取相关信息，收发rtp和rtx的ssrc都会记录  [AUTO-TRANSLATED:6c57cd48]
    // Get relevant information based on the ssrc of the rtcp, the ssrc of sending and receiving rtp and rtx will be recorded
    std::unordered_map<uint32_t/*ssrc*/, MediaTrack::Ptr> _ssrc_to_track;
    // 根据接收rtp的pt获取相关信息  [AUTO-TRANSLATED:39e56d7d]
    // Get relevant information based on the pt of the received rtp
    std::unordered_map<uint8_t/*pt*/, std::unique_ptr<WrappedMediaTrack>> _pt_to_track;
    std::vector<SdpAttrCandidate> _cands;
    // http访问时的host ip  [AUTO-TRANSLATED:e8fe6957]
    // Host ip for http access
    std::string _local_ip;
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

class WebRtcArgs : public std::enable_shared_from_this<WebRtcArgs> {
public:
    virtual ~WebRtcArgs() = default;
    virtual toolkit::variant operator[](const std::string &key) const = 0;
};

using onCreateWebRtc = std::function<void(const WebRtcInterface &rtc)>;
class WebRtcPluginManager {
public:
    using Plugin = std::function<void(toolkit::SocketHelper& sender, const WebRtcArgs &args, const onCreateWebRtc &cb)>;
    using Listener = std::function<void(toolkit::SocketHelper& sender, const std::string &type, const WebRtcArgs &args, const WebRtcInterface &rtc)>;

    static WebRtcPluginManager &Instance();

    void registerPlugin(const std::string &type, Plugin cb);
    void setListener(Listener cb);
    void negotiateSdp(toolkit::SocketHelper& sender, const std::string &type, const WebRtcArgs &args, const onCreateWebRtc &cb);

private:
    WebRtcPluginManager() = default;

private:
    mutable std::mutex _mtx_creator;
    Listener _listener;
    std::unordered_map<std::string, Plugin> _map_creator;
};

void translateIPFromEnv(std::vector<std::string> &v);

}// namespace mediakit

#endif // ZLMEDIAKIT_WEBRTC_TRANSPORT_H
