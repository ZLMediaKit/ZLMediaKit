//
// Created by xueyuegui on 19-12-7.
//

#ifndef MYWEBRTC_MYDTLSTRANSPORT_H
#define MYWEBRTC_MYDTLSTRANSPORT_H

#include <functional>
#include <memory>

#include "rtc_dtls_transport.h"

class DtlsTransport : RTC::DtlsTransport::Listener {
public:
    typedef std::shared_ptr<DtlsTransport> Ptr;

    DtlsTransport(bool bServer);
    ~DtlsTransport();

    void Start();
    void Close();
    void InputData(char *buf, size_t len);
    void OutputData(char *buf, size_t len);
    static bool IsDtlsPacket(const char *buf, size_t len);
    std::string GetMyFingerprint() {
        auto finger_prints = dtls_transport_->GetLocalFingerprints();
        for (size_t i = 0; i < finger_prints.size(); i++) {
            if (finger_prints[i].algorithm == RTC::DtlsTransport::FingerprintAlgorithm::SHA256) {
                return finger_prints[i].value;
            }
        }
        return "";
    };

    void SetHandshakeCompletedCB(std::function<void(std::string clientKey, std::string serverKey, RTC::CryptoSuite)> cb) {
        handshake_completed_callback_ = std::move(cb);
    }
    void SetHandshakeFailedCB(std::function<void()> cb) { handshake_failed_callback_ = std::move(cb); }
    void SetOutPutCB(std::function<void(char *buf, size_t len)> cb) { output_callback_ = std::move(cb); }

    /* Pure virtual methods inherited from RTC::DtlsTransport::Listener. */
public:
    void OnDtlsTransportConnecting(const RTC::DtlsTransport *dtlsTransport) override;
    void OnDtlsTransportConnected(const RTC::DtlsTransport *dtlsTransport, RTC::CryptoSuite srtpCryptoSuite, uint8_t *srtpLocalKey, size_t srtpLocalKeyLen, uint8_t *srtpRemoteKey, size_t srtpRemoteKeyLen, std::string &remoteCert) override;
    void OnDtlsTransportFailed(const RTC::DtlsTransport *dtlsTransport) override;
    void OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) override;
    void OnDtlsTransportSendData(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data,size_t len) override;
    void OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) override;

private:
    bool is_server_ = false;
    std::function<void()> handshake_failed_callback_;
    std::shared_ptr<RTC::DtlsTransport> dtls_transport_;
    std::function<void(char *buf, size_t len)> output_callback_;
    std::function<void(std::string client_key, std::string server_key, RTC::CryptoSuite srtp_crypto_suite)> handshake_completed_callback_;
};

#endif// MYWEBRTC_MYDTLSTRANSPORT_H
