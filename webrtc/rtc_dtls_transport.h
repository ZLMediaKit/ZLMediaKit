#ifndef MS_RTC_DTLS_TRANSPORT_HPP
#define MS_RTC_DTLS_TRANSPORT_HPP

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <map>
#include <string>
#include <vector>

namespace RTC {
enum class CryptoSuite {
  NONE = 0,
  AES_CM_128_HMAC_SHA1_80 = 1,
  AES_CM_128_HMAC_SHA1_32,
  AEAD_AES_256_GCM,
  AEAD_AES_128_GCM
};
class DtlsTransport {
 public:
  enum class DtlsState { NEW = 1, CONNECTING, CONNECTED, FAILED, CLOSED };

 public:
  enum class Role { NONE = 0, AUTO = 1, CLIENT, SERVER };

 public:
  enum class FingerprintAlgorithm { NONE = 0, SHA1 = 1, SHA224, SHA256, SHA384, SHA512 };

 public:
  struct Fingerprint {
    FingerprintAlgorithm algorithm{FingerprintAlgorithm::NONE};
    std::string value;
  };

 private:
  struct SrtpCryptoSuiteMapEntry {
    RTC::CryptoSuite cryptoSuite;
    const char* name;
  };

 public:
  class Listener {
   public:
    // DTLS is in the process of negotiating a secure connection. Incoming
    // media can flow through.
    // NOTE: The caller MUST NOT call any method during this callback.
    virtual void OnDtlsTransportConnecting(const RTC::DtlsTransport* dtlsTransport) = 0;
    // DTLS has completed negotiation of a secure connection (including DTLS-SRTP
    // and remote fingerprint verification). Outgoing media can now flow through.
    // NOTE: The caller MUST NOT call any method during this callback.
    virtual void OnDtlsTransportConnected(const RTC::DtlsTransport* dtlsTransport,
                                          RTC::CryptoSuite srtpCryptoSuite, uint8_t* srtpLocalKey,
                                          size_t srtpLocalKeyLen, uint8_t* srtpRemoteKey,
                                          size_t srtpRemoteKeyLen, std::string& remoteCert) = 0;
    // The DTLS connection has been closed as the result of an error (such as a
    // DTLS alert or a failure to validate the remote fingerprint).
    virtual void OnDtlsTransportFailed(const RTC::DtlsTransport* dtlsTransport) = 0;
    // The DTLS connection has been closed due to receipt of a close_notify alert.
    virtual void OnDtlsTransportClosed(const RTC::DtlsTransport* dtlsTransport) = 0;
    // Need to send DTLS data to the peer.
    virtual void OnDtlsTransportSendData(const RTC::DtlsTransport* dtlsTransport,
                                         const uint8_t* data, size_t len) = 0;
    // DTLS application data received.
    virtual void OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport* dtlsTransport,
                                                        const uint8_t* data, size_t len) = 0;
  };

 public:
  static void ClassInit();
  static void ClassDestroy();
  static Role StringToRole(const std::string& role) {
    auto it = DtlsTransport::string2Role.find(role);

    if (it != DtlsTransport::string2Role.end())
      return it->second;
    else
      return DtlsTransport::Role::NONE;
  }
  static FingerprintAlgorithm GetFingerprintAlgorithm(const std::string& fingerprint) {
    auto it = DtlsTransport::string2FingerprintAlgorithm.find(fingerprint);

    if (it != DtlsTransport::string2FingerprintAlgorithm.end())
      return it->second;
    else
      return DtlsTransport::FingerprintAlgorithm::NONE;
  }
  static std::string& GetFingerprintAlgorithmString(FingerprintAlgorithm fingerprint) {
    auto it = DtlsTransport::fingerprintAlgorithm2String.find(fingerprint);

    return it->second;
  }
  static bool IsDtls(const uint8_t* data, size_t len) {
    // clang-format off
			return (
				// Minimum DTLS record length is 13 bytes.
				(len >= 13) &&
				// DOC: https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
				(data[0] > 19 && data[0] < 64)
			);
    // clang-format on
  }

 private:
  static void GenerateCertificateAndPrivateKey();
  static void ReadCertificateAndPrivateKeyFromFiles();
  static void CreateSslCtx();
  static void GenerateFingerprints();

 private:
  static X509* certificate;
  static EVP_PKEY* privateKey;
  static SSL_CTX* sslCtx;
  static uint8_t sslReadBuffer[];
  static std::map<std::string, Role> string2Role;
  static std::map<std::string, FingerprintAlgorithm> string2FingerprintAlgorithm;
  static std::map<FingerprintAlgorithm, std::string> fingerprintAlgorithm2String;
  static std::vector<Fingerprint> localFingerprints;
  static std::vector<SrtpCryptoSuiteMapEntry> srtpCryptoSuites;

 public:
  explicit DtlsTransport(Listener* listener);
  ~DtlsTransport();

 public:
  void Dump() const;
  void Run(Role localRole);
  std::vector<Fingerprint>& GetLocalFingerprints() const {
    return DtlsTransport::localFingerprints;
  }
  bool SetRemoteFingerprint(Fingerprint fingerprint);
  void ProcessDtlsData(const uint8_t* data, size_t len);
  DtlsState GetState() const { return this->state; }
  Role GetLocalRole() const { return this->localRole; }
  void SendApplicationData(const uint8_t* data, size_t len);

 private:
  bool IsRunning() const {
    switch (this->state) {
      case DtlsState::NEW:
        return false;
      case DtlsState::CONNECTING:
      case DtlsState::CONNECTED:
        return true;
      case DtlsState::FAILED:
      case DtlsState::CLOSED:
        return false;
    }

    // Make GCC 4.9 happy.
    return false;
  }
  void Reset();
  bool CheckStatus(int returnCode);
  void SendPendingOutgoingDtlsData();
  bool SetTimeout();
  bool ProcessHandshake();
  bool CheckRemoteFingerprint();
  void ExtractSrtpKeys(RTC::CryptoSuite srtpCryptoSuite);
  RTC::CryptoSuite GetNegotiatedSrtpCryptoSuite();

  /* Callbacks fired by OpenSSL events. */
 public:
  void OnSslInfo(int where, int ret);

  /* Pure virtual methods inherited from Timer::Listener. */
 public:
  void OnTimer();

 private:
  // Passed by argument.
  Listener* listener{nullptr};
  // Allocated by this.
  SSL* ssl{nullptr};
  BIO* sslBioFromNetwork{nullptr};  // The BIO from which ssl reads.
  BIO* sslBioToNetwork{nullptr};    // The BIO in which ssl writes.
  // Others.
  DtlsState state{DtlsState::NEW};
  Role localRole{Role::NONE};
  Fingerprint remoteFingerprint;
  bool handshakeDone{false};
  bool handshakeDoneNow{false};
  std::string remoteCert;
};
}  // namespace RTC

#endif
