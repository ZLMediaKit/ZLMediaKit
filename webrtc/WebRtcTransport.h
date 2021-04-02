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
using namespace toolkit;
using namespace mediakit;

class WebRtcTransport : public RTC::DtlsTransport::Listener, public RTC::IceServer::Listener  {
public:
    using Ptr = std::shared_ptr<WebRtcTransport>;
    WebRtcTransport(const EventPoller::Ptr &poller);
    ~WebRtcTransport() override = default;

    /**
     * 消费对象
     */
    virtual void onDestory();

    /**
     * 创建webrtc answer sdp
     * @param offer offer sdp
     * @return answer sdp
     */
    std::string getAnswerSdp(const string &offer);

    /**
     * socket收到udp数据
     * @param buf 数据指针
     * @param len 数据长度
     * @param tuple 数据来源
     */
    void inputSockData(char *buf, size_t len, RTC::TransportTuple *tuple);

    /**
     * 发送rtp
     * @param buf rtcp内容
     * @param len rtcp长度
     */
    void sendRtpPacket(char *buf, size_t len, bool flush);

protected:
    ////  dtls相关的回调 ////
    void OnDtlsTransportConnecting(const RTC::DtlsTransport *dtlsTransport) override {};
    void OnDtlsTransportConnected(const RTC::DtlsTransport *dtlsTransport,
                                  RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
                                  uint8_t *srtpLocalKey,
                                  size_t srtpLocalKeyLen,
                                  uint8_t *srtpRemoteKey,
                                  size_t srtpRemoteKeyLen,
                                  std::string &remoteCert) override;

    void OnDtlsTransportFailed(const RTC::DtlsTransport *dtlsTransport) override {};
    void OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) override {};
    void OnDtlsTransportSendData(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) override;
    void OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) override {};

protected:
    //// ice相关的回调 ///
    void OnIceServerSendStunPacket(const RTC::IceServer *iceServer, const RTC::StunPacket *packet, RTC::TransportTuple *tuple) override;
    void OnIceServerSelectedTuple(const RTC::IceServer *iceServer, RTC::TransportTuple *tuple) override;
    void OnIceServerConnected(const RTC::IceServer *iceServer) override;
    void OnIceServerCompleted(const RTC::IceServer *iceServer) override;
    void OnIceServerDisconnected(const RTC::IceServer *iceServer) override;

protected:
    virtual void onStartWebRTC() = 0;
    virtual void onRtcConfigure(RtcConfigure &configure) const {}
    virtual void onCheckSdp(SdpType type, RtcSession &sdp) const;
    virtual void onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst, bool flush = true) = 0;

    virtual void onRtp(const char *buf, size_t len) = 0;
    virtual void onRtcp(const char *buf, size_t len) = 0;

protected:
    const RtcSession& getSdp(SdpType type) const;

private:
    void onSendSockData(const char *buf, size_t len, bool flush = true);
    void setRemoteDtlsFingerprint(const RtcSession &remote);

private:
    std::shared_ptr<RTC::IceServer> _ice_server;
    std::shared_ptr<RTC::DtlsTransport> _dtls_transport;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_send;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_recv;
    RtcSession::Ptr _offer_sdp;
    RtcSession::Ptr _answer_sdp;
};

class RtpReceiverImp;

class WebRtcTransportImp : public WebRtcTransport, public std::enable_shared_from_this<WebRtcTransportImp>{
public:
    using Ptr = std::shared_ptr<WebRtcTransportImp>;
    ~WebRtcTransportImp() override = default;

    /**
     * 创建WebRTC对象
     * @param poller 改对象需要绑定的线程
     * @return 对象
     */
    static Ptr create(const EventPoller::Ptr &poller);

    /**
     * 绑定rtsp媒体源
     * @param src 媒体源
     */
    void attach(const RtspMediaSource::Ptr &src);

protected:
    void onStartWebRTC() override;
    void onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst, bool flush = true) override;
    void onCheckSdp(SdpType type, RtcSession &sdp) const override;
    void onRtcConfigure(RtcConfigure &configure) const override;

    void onRtp(const char *buf, size_t len) override;
    void onRtcp(const char *buf, size_t len) override;

private:
    WebRtcTransportImp(const EventPoller::Ptr &poller);
    void onDestory() override;
    void onSendRtp(const RtpPacket::Ptr &rtp, bool flush);
    SdpAttrCandidate::Ptr getIceCandidate() const;
    bool canSendRtp() const;
    bool canRecvRtp() const;

    class RtpPayloadInfo {
    public:
        bool is_common_rtp;
        const RtcCodecPlan *plan;
        const RtcMedia *media;
        std::shared_ptr<RtpReceiverImp> receiver;
    };

    void onSortedRtp(const RtpPayloadInfo &info,RtpPacket::Ptr rtp);
    void onBeforeSortedRtp(const RtpPayloadInfo &info,const RtpPacket::Ptr &rtp);

private:
    Socket::Ptr _socket;
    RtspMediaSource::Ptr _src;
    RtspMediaSource::RingType::RingReader::Ptr _reader;
    RtcSession _answer_sdp;
    mutable RtcSession _rtsp_send_sdp;
    mutable uint8_t _send_rtp_pt[2] = {0, 0};
    RtspMediaSourceImp::Ptr _push_src;
    unordered_map<uint8_t, RtpPayloadInfo> _rtp_receiver;
};


















