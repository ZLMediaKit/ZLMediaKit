#pragma once

#include <memory>
#include <string>

#include "dtls_transport.h"
#include "ice_server.h"
#include "srtp_session.h"
#include "stun_packet.h"

class WebRtcTransport {
public:
    using Ptr = std::shared_ptr<WebRtcTransport>;
    WebRtcTransport();
    virtual ~WebRtcTransport();

    /// 获取本地sdp
    /// \return
    std::string GetLocalSdp();

    /// 收到udp数据
    /// \param buf
    /// \param len
    /// \param remote_address
    void OnInputDataPacket(char *buf, size_t len, struct sockaddr_in *remote_address);

    /// 发送rtp
    /// \param buf
    /// \param len
    void WritRtpPacket(char *buf, size_t len);

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
    virtual void onIceConnected() = 0;
    virtual void onDtlsCompleted() = 0;

private:
    void OnIceServerCompleted();
    void OnDtlsCompleted(std::string client_key, std::string server_key, RTC::CryptoSuite srtp_crypto_suite);
    void WritePacket(char *buf, size_t len, struct sockaddr_in *remote_address = nullptr);

private:
    IceServer::Ptr ice_server_;
    DtlsTransport::Ptr dtls_transport_;
    std::shared_ptr<RTC::SrtpSession> srtp_session_;
};

#include "Poller/EventPoller.h"
#include "Network/Socket.h"
#include "Rtsp/RtspMediaSource.h"
using namespace toolkit;
using namespace mediakit;

class WebRtcTransportImp : public WebRtcTransport, public std::enable_shared_from_this<WebRtcTransportImp>{
public:
    using Ptr = std::shared_ptr<WebRtcTransportImp>;

    WebRtcTransportImp(const EventPoller::Ptr &poller);
    ~WebRtcTransportImp() override = default;

    void attach(const RtspMediaSource::Ptr &src);

protected:
    void onWrite(const char *buf, size_t len, struct sockaddr_in *dst) override;
    int getPayloadType() const ;
    uint32_t getSSRC() const override;
    uint16_t getPort() const override;
    std::string getIP() const override;
    void onIceConnected() override;
    void onDtlsCompleted() override;

private:
    Socket::Ptr _socket;
    RtspMediaSource::Ptr _src;
    RtspMediaSource::RingType::RingReader::Ptr _reader;
};

class WebRtcManager : public std::enable_shared_from_this<WebRtcManager> {
public:
    ~WebRtcManager();
    static WebRtcManager& Instance();

private:
    WebRtcManager();

};


















