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
#include "Rtsp/RtspMediaSource.h"
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
    void sendRtpPacket(char *buf, size_t len);

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

    virtual uint32_t getSSRC() const = 0;
    virtual uint16_t getPort() const = 0;
    virtual std::string getIP() const = 0;
    virtual void onStartWebRTC() = 0;
    virtual void onRtcConfigure(RtcConfigure &configure) const {}
    virtual void onCheckSdp(SdpType type, const RtcSession &sdp) const;

    virtual SdpAttrCandidate::Ptr getIceCandidate() const = 0;
    virtual void onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst) = 0;

private:
    void onSendSockData(const char *buf, size_t len);
    void setRemoteDtlsFingerprint(const RtcSession &remote);

private:
    std::shared_ptr<RTC::IceServer> _ice_server;
    std::shared_ptr<RTC::DtlsTransport> _dtls_transport;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_send;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_recv;
    RtcSession::Ptr _offer_sdp;
    RtcSession::Ptr _answer_sdp;
};

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
    void onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst) override;
    uint32_t getSSRC() const override;
    uint16_t getPort() const override;
    std::string getIP() const override;
    SdpAttrCandidate::Ptr getIceCandidate() const override;

private:
    WebRtcTransportImp(const EventPoller::Ptr &poller);
    void onDestory() override;

private:
    Socket::Ptr _socket;
    RtspMediaSource::Ptr _src;
    RtspMediaSource::RingType::RingReader::Ptr _reader;
};


















