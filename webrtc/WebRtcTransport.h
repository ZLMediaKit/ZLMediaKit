#pragma once

#include <memory>
#include <string>

#include "DtlsTransport.hpp"
#include "IceServer.hpp"
#include "SrtpSession.hpp"
#include "StunPacket.hpp"
#include "Sdp.h"

class WebRtcTransport : public RTC::DtlsTransport::Listener, public RTC::IceServer::Listener  {
public:
    using Ptr = std::shared_ptr<WebRtcTransport>;
    WebRtcTransport(const EventPoller::Ptr &poller);
    ~WebRtcTransport() override = default;

    /// 销毁对象
    virtual void onDestory();

    std::string getAnswerSdp(const string &offer);

    std::string getOfferSdp();

    /// 收到udp数据
    /// \param buf
    /// \param len
    /// \param remote_address
    void OnInputDataPacket(char *buf, size_t len, RTC::TransportTuple *tuple);

    /// 发送rtp
    /// \param buf
    /// \param len
    void WritRtpPacket(char *buf, size_t len);

protected:
    // dtls相关的回调
    void OnDtlsTransportConnecting(const RTC::DtlsTransport *dtlsTransport) override {};
    void OnDtlsTransportConnected(
            const RTC::DtlsTransport *dtlsTransport,
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
    //ice相关的回调
    void OnIceServerSendStunPacket(const RTC::IceServer *iceServer, const RTC::StunPacket *packet, RTC::TransportTuple *tuple) override;
    void OnIceServerSelectedTuple(const RTC::IceServer *iceServer, RTC::TransportTuple *tuple) override;
    void OnIceServerConnected(const RTC::IceServer *iceServer) override;
    void OnIceServerCompleted(const RTC::IceServer *iceServer) override;
    void OnIceServerDisconnected(const RTC::IceServer *iceServer) override;

protected:
    /// 输出udp数据
    /// \param buf
    /// \param len
    /// \param dst
    virtual void onWrite(const char *buf, size_t len, struct sockaddr_in *dst) = 0;
    virtual uint32_t getSSRC() const = 0;
    virtual uint16_t getPort() const = 0;
    virtual std::string getIP() const = 0;
    virtual int getPayloadType() const = 0;
    virtual void onDtlsConnected() = 0;

private:
    void onWrite(const char *buf, size_t len);

private:
    std::shared_ptr<RTC::IceServer> ice_server_;
    std::shared_ptr<RTC::DtlsTransport> dtls_transport_;
    std::shared_ptr<RTC::SrtpSession> srtp_session_;
    RtcSession::Ptr _offer_sdp;
    RtcSession::Ptr _answer_sdp;
};

#include "Poller/EventPoller.h"
#include "Network/Socket.h"
#include "Rtsp/RtspMediaSource.h"
using namespace toolkit;
using namespace mediakit;

class WebRtcTransportImp : public WebRtcTransport, public std::enable_shared_from_this<WebRtcTransportImp>{
public:
    using Ptr = std::shared_ptr<WebRtcTransportImp>;

    static Ptr create(const EventPoller::Ptr &poller);
    ~WebRtcTransportImp() override = default;

    void attach(const RtspMediaSource::Ptr &src);

protected:
    void onWrite(const char *buf, size_t len, struct sockaddr_in *dst) override;
    int getPayloadType() const override;
    uint32_t getSSRC() const override;
    uint16_t getPort() const override;
    std::string getIP() const override;
    void onDtlsConnected() override;
    WebRtcTransportImp(const EventPoller::Ptr &poller);
    void onDestory() override;

private:
    Socket::Ptr _socket;
    RtspMediaSource::Ptr _src;
    RtspMediaSource::RingType::RingReader::Ptr _reader;
};


















