/**
ISC License

Copyright © 2015, Iñaki Baz Castillo <ibc@aliax.net>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MS_RTC_DTLS_TRANSPORT_HPP
#define MS_RTC_DTLS_TRANSPORT_HPP

#include "SrtpSession.hpp"
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <map>
#include <string>
#include <vector>
#include "Poller/Timer.h"
#include "Poller/EventPoller.h"
using namespace toolkit;

namespace RTC
{
    class DtlsTransport : public std::enable_shared_from_this<DtlsTransport>
	{
	public:
		enum class DtlsState
		{
			NEW = 1,
			CONNECTING,
			CONNECTED,
			FAILED,
			CLOSED
		};

	public:
		enum class Role
		{
			NONE = 0,
			AUTO = 1,
			CLIENT,
			SERVER
		};

	public:
		enum class FingerprintAlgorithm
		{
			NONE = 0,
			SHA1 = 1,
			SHA224,
			SHA256,
			SHA384,
			SHA512
		};

	public:
		struct Fingerprint
		{
			FingerprintAlgorithm algorithm{ FingerprintAlgorithm::NONE };
			std::string value;
		};

	private:
		struct SrtpCryptoSuiteMapEntry
		{
			RTC::SrtpSession::CryptoSuite cryptoSuite;
			const char* name;
		};

        class DtlsEnvironment : public std::enable_shared_from_this<DtlsEnvironment>
        {
        public:
            using Ptr = std::shared_ptr<DtlsEnvironment>;
            ~DtlsEnvironment();
            static DtlsEnvironment& Instance();

        private:
            DtlsEnvironment();
            void GenerateCertificateAndPrivateKey();
            void ReadCertificateAndPrivateKeyFromFiles();
            void CreateSslCtx();
            void GenerateFingerprints();

        public:
            X509* certificate{ nullptr };
            EVP_PKEY* privateKey{ nullptr };
            SSL_CTX* sslCtx{ nullptr };
            std::vector<Fingerprint> localFingerprints;
        };

	public:
		class Listener
		{
		public:
			// DTLS is in the process of negotiating a secure connection. Incoming
			// media can flow through.
			// NOTE: The caller MUST NOT call any method during this callback.
			virtual void OnDtlsTransportConnecting(const RTC::DtlsTransport* dtlsTransport) = 0;
			// DTLS has completed negotiation of a secure connection (including DTLS-SRTP
			// and remote fingerprint verification). Outgoing media can now flow through.
			// NOTE: The caller MUST NOT call any method during this callback.
			virtual void OnDtlsTransportConnected(
			  const RTC::DtlsTransport* dtlsTransport,
			  RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
			  uint8_t* srtpLocalKey,
			  size_t srtpLocalKeyLen,
			  uint8_t* srtpRemoteKey,
			  size_t srtpRemoteKeyLen,
			  std::string& remoteCert) = 0;
			// The DTLS connection has been closed as the result of an error (such as a
			// DTLS alert or a failure to validate the remote fingerprint).
			virtual void OnDtlsTransportFailed(const RTC::DtlsTransport* dtlsTransport) = 0;
			// The DTLS connection has been closed due to receipt of a close_notify alert.
			virtual void OnDtlsTransportClosed(const RTC::DtlsTransport* dtlsTransport) = 0;
			// Need to send DTLS data to the peer.
			virtual void OnDtlsTransportSendData(
			  const RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len) = 0;
			// DTLS application data received.
			virtual void OnDtlsTransportApplicationDataReceived(
			  const RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len) = 0;
		};

	public:
		static Role StringToRole(const std::string& role)
		{
			auto it = DtlsTransport::string2Role.find(role);

			if (it != DtlsTransport::string2Role.end())
				return it->second;
			else
				return DtlsTransport::Role::NONE;
		}
		static FingerprintAlgorithm GetFingerprintAlgorithm(const std::string& fingerprint)
		{
			auto it = DtlsTransport::string2FingerprintAlgorithm.find(fingerprint);

			if (it != DtlsTransport::string2FingerprintAlgorithm.end())
				return it->second;
			else
				return DtlsTransport::FingerprintAlgorithm::NONE;
		}
		static std::string& GetFingerprintAlgorithmString(FingerprintAlgorithm fingerprint)
		{
			auto it = DtlsTransport::fingerprintAlgorithm2String.find(fingerprint);

			return it->second;
		}
		static bool IsDtls(const uint8_t* data, size_t len)
		{
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
		static std::map<std::string, Role> string2Role;
		static std::map<std::string, FingerprintAlgorithm> string2FingerprintAlgorithm;
		static std::map<FingerprintAlgorithm, std::string> fingerprintAlgorithm2String;
		static std::vector<SrtpCryptoSuiteMapEntry> srtpCryptoSuites;

	public:
		DtlsTransport(EventPoller::Ptr poller, Listener* listener);
		~DtlsTransport();

	public:
		void Dump() const;
		void Run(Role localRole);
		std::vector<Fingerprint>& GetLocalFingerprints() const
		{
			return env->localFingerprints;
		}
		bool SetRemoteFingerprint(Fingerprint fingerprint);
		void ProcessDtlsData(const uint8_t* data, size_t len);
		DtlsState GetState() const
		{
			return this->state;
		}
		Role GetLocalRole() const
		{
			return this->localRole;
		}
		void SendApplicationData(const uint8_t* data, size_t len);

	private:
		bool IsRunning() const
		{
			switch (this->state)
			{
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
		void ExtractSrtpKeys(RTC::SrtpSession::CryptoSuite srtpCryptoSuite);
		RTC::SrtpSession::CryptoSuite GetNegotiatedSrtpCryptoSuite();

    private:
	    void OnSslInfo(int where, int ret);
		void OnTimer();

	private:
        DtlsEnvironment::Ptr env;
        EventPoller::Ptr poller;
        // Passed by argument.
		Listener* listener{ nullptr };
		// Allocated by this.
		SSL* ssl{ nullptr };
		BIO* sslBioFromNetwork{ nullptr }; // The BIO from which ssl reads.
		BIO* sslBioToNetwork{ nullptr };   // The BIO in which ssl writes.
		Timer::Ptr timer;
		// Others.
		DtlsState state{ DtlsState::NEW };
		Role localRole{ Role::NONE };
		Fingerprint remoteFingerprint;
		bool handshakeDone{ false };
		bool handshakeDoneNow{ false };
		std::string remoteCert;
		//最大不超过mtu
		static constexpr int SslReadBufferSize{ 2000 };
        uint8_t sslReadBuffer[SslReadBufferSize];
};
} // namespace RTC

#endif
