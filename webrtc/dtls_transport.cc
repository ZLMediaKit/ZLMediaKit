//
// Created by xueyuegui on 19-12-7.
//

#include "dtls_transport.h"

#include <iostream>

DtlsTransport::DtlsTransport(bool is_server) : is_server_(is_server) {
    dtls_transport_.reset(new RTC::DtlsTransport(this));
}

DtlsTransport::~DtlsTransport() {}

void DtlsTransport::Start() {
    if (is_server_) {
        dtls_transport_->Run(RTC::DtlsTransport::Role::SERVER);
    } else {
        dtls_transport_->Run(RTC::DtlsTransport::Role::CLIENT);
    }
}

void DtlsTransport::Close() {}

void DtlsTransport::OnDtlsTransportConnecting(const RTC::DtlsTransport *dtlsTransport) {}

void DtlsTransport::OnDtlsTransportConnected(const RTC::DtlsTransport *dtlsTransport,
                                             RTC::CryptoSuite srtp_crypto_suite,
                                             uint8_t *srtpLocalKey, size_t srtpLocalKeyLen,
                                             uint8_t *srtpRemoteKey, size_t srtpRemoteKeyLen,
                                             std::string &remoteCert) {
    std::string client_key;
    std::string server_key;
    server_key.assign((char *) srtpLocalKey, srtpLocalKeyLen);
    client_key.assign((char *) srtpRemoteKey, srtpRemoteKeyLen);
    if (is_server_) {
        // If we are server, we swap the keys
        client_key.swap(server_key);
    }
    if (handshake_completed_callback_) {
        handshake_completed_callback_(client_key, server_key, srtp_crypto_suite);
    }
}

void DtlsTransport::OnDtlsTransportFailed(const RTC::DtlsTransport *dtlsTransport) {
    if (handshake_failed_callback_) {
        handshake_failed_callback_();
    }
}

void DtlsTransport::OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) {}

void DtlsTransport::OnDtlsTransportSendData(const RTC::DtlsTransport *dtlsTransport,
                                            const uint8_t *data, size_t len) {
    if (output_callback_) {
        output_callback_((char *) data, len);
    }
}

void DtlsTransport::OutputData(char *buf, size_t len) {
    if (output_callback_) {
        output_callback_(buf, len);
    }
}

void DtlsTransport::OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport *dtlsTransport,
                                                           const uint8_t *data, size_t len) {}

bool DtlsTransport::IsDtlsPacket(const char *buf, size_t len) {
    return RTC::DtlsTransport::IsDtls((uint8_t *) buf, len);
}

void DtlsTransport::InputData(char *buf, size_t len) {
    dtls_transport_->ProcessDtlsData((uint8_t *) buf, len);
}
