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

#define MS_CLASS "RTC::DtlsTransport"
// #define MS_LOG_DEV_LEVEL 3

#include "DtlsTransport.hpp"
#include "logger.h"
#include <openssl/asn1.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <cstdio>  // std::sprintf(), std::fopen()
#include <cstring> // std::memcpy(), std::strcmp()
#include "Util/util.h"

using namespace std;

#define LOG_OPENSSL_ERROR(desc)                                                                    \
	do                                                                                               \
	{                                                                                                \
		if (ERR_peek_error() == 0)                                                                     \
			MS_ERROR("OpenSSL error [desc:'%s']", desc);                                                 \
		else                                                                                           \
		{                                                                                              \
			int64_t err;                                                                                 \
			while ((err = ERR_get_error()) != 0)                                                         \
			{                                                                                            \
				MS_ERROR("OpenSSL error [desc:'%s', error:'%s']", desc, ERR_error_string(err, nullptr));   \
			}                                                                                            \
			ERR_clear_error();                                                                           \
		}                                                                                              \
	} while (false)

/* Static methods for OpenSSL callbacks. */

inline static int onSslCertificateVerify(int /*preverifyOk*/, X509_STORE_CTX* /*ctx*/)
{
	MS_TRACE();

	// Always valid since DTLS certificates are self-signed.
	return 1;
}

inline static unsigned int onSslDtlsTimer(SSL* /*ssl*/, unsigned int timerUs)
{
	if (timerUs == 0)
		return 100000;
	else if (timerUs >= 4000000)
		return 4000000;
	else
		return 2 * timerUs;
}

namespace RTC
{
	/* Static. */

	// clang-format off
	static constexpr int DtlsMtu{ 1350 };
	// AES-HMAC: http://tools.ietf.org/html/rfc3711
	static constexpr size_t SrtpMasterKeyLength{ 16 };
	static constexpr size_t SrtpMasterSaltLength{ 14 };
	static constexpr size_t SrtpMasterLength{ SrtpMasterKeyLength + SrtpMasterSaltLength };
	// AES-GCM: http://tools.ietf.org/html/rfc7714
	static constexpr size_t SrtpAesGcm256MasterKeyLength{ 32 };
	static constexpr size_t SrtpAesGcm256MasterSaltLength{ 12 };
	static constexpr size_t SrtpAesGcm256MasterLength{ SrtpAesGcm256MasterKeyLength + SrtpAesGcm256MasterSaltLength };
	static constexpr size_t SrtpAesGcm128MasterKeyLength{ 16 };
	static constexpr size_t SrtpAesGcm128MasterSaltLength{ 12 };
	static constexpr size_t SrtpAesGcm128MasterLength{ SrtpAesGcm128MasterKeyLength + SrtpAesGcm128MasterSaltLength };
	// clang-format on

	/* Class variables. */
	// clang-format off
	std::map<std::string, DtlsTransport::FingerprintAlgorithm> DtlsTransport::string2FingerprintAlgorithm =
	{
		{ "sha-1",   DtlsTransport::FingerprintAlgorithm::SHA1   },
		{ "sha-224", DtlsTransport::FingerprintAlgorithm::SHA224 },
		{ "sha-256", DtlsTransport::FingerprintAlgorithm::SHA256 },
		{ "sha-384", DtlsTransport::FingerprintAlgorithm::SHA384 },
		{ "sha-512", DtlsTransport::FingerprintAlgorithm::SHA512 }
	};
	std::map<DtlsTransport::FingerprintAlgorithm, std::string> DtlsTransport::fingerprintAlgorithm2String =
	{
		{ DtlsTransport::FingerprintAlgorithm::SHA1,   "sha-1"   },
		{ DtlsTransport::FingerprintAlgorithm::SHA224, "sha-224" },
		{ DtlsTransport::FingerprintAlgorithm::SHA256, "sha-256" },
		{ DtlsTransport::FingerprintAlgorithm::SHA384, "sha-384" },
		{ DtlsTransport::FingerprintAlgorithm::SHA512, "sha-512" }
	};
	std::map<std::string, DtlsTransport::Role> DtlsTransport::string2Role =
	{
		{ "auto",   DtlsTransport::Role::AUTO   },
		{ "client", DtlsTransport::Role::CLIENT },
		{ "server", DtlsTransport::Role::SERVER }
	};
	std::vector<DtlsTransport::SrtpCryptoSuiteMapEntry> DtlsTransport::srtpCryptoSuites =
	{
		{ RTC::SrtpSession::CryptoSuite::AEAD_AES_256_GCM, "SRTP_AEAD_AES_256_GCM" },
		{ RTC::SrtpSession::CryptoSuite::AEAD_AES_128_GCM, "SRTP_AEAD_AES_128_GCM" },
		{ RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_80, "SRTP_AES128_CM_SHA1_80" },
		{ RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_32, "SRTP_AES128_CM_SHA1_32" }
	};
	// clang-format on

	INSTANCE_IMP(DtlsTransport::DtlsEnvironment);

	/* Class methods. */

    DtlsTransport::DtlsEnvironment::DtlsEnvironment()
	{
		MS_TRACE();

		// Generate a X509 certificate and private key (unless PEM files are provided).
		if (true /*
		  Settings::configuration.dtlsCertificateFile.empty() ||
		  Settings::configuration.dtlsPrivateKeyFile.empty()*/)
		{
			GenerateCertificateAndPrivateKey();
		}
		else
		{
			ReadCertificateAndPrivateKeyFromFiles();
		}

		// Create a global SSL_CTX.
		CreateSslCtx();

		// Generate certificate fingerprints.
		GenerateFingerprints();
	}

    DtlsTransport::DtlsEnvironment::~DtlsEnvironment()
	{
		MS_TRACE();

		if (privateKey)
			EVP_PKEY_free(privateKey);
		if (certificate)
			X509_free(certificate);
		if (sslCtx)
			SSL_CTX_free(sslCtx);
	}

	void DtlsTransport::DtlsEnvironment::GenerateCertificateAndPrivateKey()
	{
		MS_TRACE();

		int ret{ 0 };
		EC_KEY* ecKey{ nullptr };
		X509_NAME* certName{ nullptr };
		std::string subject =
		  std::string("mediasoup") + to_string(rand() % 999999 + 100000);

		// Create key with curve.
		ecKey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

		if (!ecKey)
		{
			LOG_OPENSSL_ERROR("EC_KEY_new_by_curve_name() failed");

			goto error;
		}

		EC_KEY_set_asn1_flag(ecKey, OPENSSL_EC_NAMED_CURVE);

		// NOTE: This can take some time.
		ret = EC_KEY_generate_key(ecKey);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("EC_KEY_generate_key() failed");

			goto error;
		}

		// Create a private key object.
		privateKey = EVP_PKEY_new();

		if (!privateKey)
		{
			LOG_OPENSSL_ERROR("EVP_PKEY_new() failed");

			goto error;
		}

		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
		ret = EVP_PKEY_assign_EC_KEY(privateKey, ecKey);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("EVP_PKEY_assign_EC_KEY() failed");

			goto error;
		}

		// The EC key now belongs to the private key, so don't clean it up separately.
		ecKey = nullptr;

		// Create the X509 certificate.
		certificate = X509_new();

		if (!certificate)
		{
			LOG_OPENSSL_ERROR("X509_new() failed");

			goto error;
		}

		// Set version 3 (note that 0 means version 1).
		X509_set_version(certificate, 2);

		// Set serial number (avoid default 0).
		ASN1_INTEGER_set(
		  X509_get_serialNumber(certificate),
		  static_cast<uint64_t>(rand() % 999999 + 100000));

		// Set valid period.
		X509_gmtime_adj(X509_get_notBefore(certificate), -315360000); // -10 years.
		X509_gmtime_adj(X509_get_notAfter(certificate), 315360000);   // 10 years.

		// Set the public key for the certificate using the key.
		ret = X509_set_pubkey(certificate, privateKey);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("X509_set_pubkey() failed");

			goto error;
		}

		// Set certificate fields.
		certName = X509_get_subject_name(certificate);

		if (!certName)
		{
			LOG_OPENSSL_ERROR("X509_get_subject_name() failed");

			goto error;
		}

		X509_NAME_add_entry_by_txt(
		  certName, "O", MBSTRING_ASC, reinterpret_cast<const uint8_t*>(subject.c_str()), -1, -1, 0);
		X509_NAME_add_entry_by_txt(
		  certName, "CN", MBSTRING_ASC, reinterpret_cast<const uint8_t*>(subject.c_str()), -1, -1, 0);

		// It is self-signed so set the issuer name to be the same as the subject.
		ret = X509_set_issuer_name(certificate, certName);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("X509_set_issuer_name() failed");

			goto error;
		}

		// Sign the certificate with its own private key.
		ret = X509_sign(certificate, privateKey, EVP_sha1());

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("X509_sign() failed");

			goto error;
		}

		return;

	error:

		if (ecKey)
			EC_KEY_free(ecKey);

		if (privateKey)
			EVP_PKEY_free(privateKey); // NOTE: This also frees the EC key.

		if (certificate)
			X509_free(certificate);

		MS_THROW_ERROR("DTLS certificate and private key generation failed");
	}

	void DtlsTransport::DtlsEnvironment::ReadCertificateAndPrivateKeyFromFiles()
	{
#if 0
		MS_TRACE();

		FILE* file{ nullptr };

		file = fopen(Settings::configuration.dtlsCertificateFile.c_str(), "r");

		if (!file)
		{
			MS_ERROR("error reading DTLS certificate file: %s", std::strerror(errno));

			goto error;
		}

		certificate = PEM_read_X509(file, nullptr, nullptr, nullptr);

		if (!certificate)
		{
			LOG_OPENSSL_ERROR("PEM_read_X509() failed");

			goto error;
		}

		fclose(file);

		file = fopen(Settings::configuration.dtlsPrivateKeyFile.c_str(), "r");

		if (!file)
		{
			MS_ERROR("error reading DTLS private key file: %s", std::strerror(errno));

			goto error;
		}

		privateKey = PEM_read_PrivateKey(file, nullptr, nullptr, nullptr);

		if (!privateKey)
		{
			LOG_OPENSSL_ERROR("PEM_read_PrivateKey() failed");

			goto error;
		}

		fclose(file);

		return;

	error:

		MS_THROW_ERROR("error reading DTLS certificate and private key PEM files");
#endif
	}

	void DtlsTransport::DtlsEnvironment::CreateSslCtx()
	{
		MS_TRACE();

		std::string dtlsSrtpCryptoSuites;
		int ret;

		/* Set the global DTLS context. */

		// Both DTLS 1.0 and 1.2 (requires OpenSSL >= 1.1.0).
		sslCtx = SSL_CTX_new(DTLS_method());

		if (!sslCtx)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_new() failed");

			goto error;
		}

		ret = SSL_CTX_use_certificate(sslCtx, certificate);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_use_certificate() failed");

			goto error;
		}

		ret = SSL_CTX_use_PrivateKey(sslCtx, privateKey);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_use_PrivateKey() failed");

			goto error;
		}

		ret = SSL_CTX_check_private_key(sslCtx);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_check_private_key() failed");

			goto error;
		}

		// Set options.
		SSL_CTX_set_options(
		  sslCtx,
		  SSL_OP_CIPHER_SERVER_PREFERENCE | SSL_OP_NO_TICKET | SSL_OP_SINGLE_ECDH_USE |
		    SSL_OP_NO_QUERY_MTU);

		// Don't use sessions cache.
		SSL_CTX_set_session_cache_mode(sslCtx, SSL_SESS_CACHE_OFF);

		// Read always as much into the buffer as possible.
		// NOTE: This is the default for DTLS, but a bug in non latest OpenSSL
		// versions makes this call required.
		SSL_CTX_set_read_ahead(sslCtx, 1);

		SSL_CTX_set_verify_depth(sslCtx, 4);

		// Require certificate from peer.
		SSL_CTX_set_verify(
		  sslCtx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, onSslCertificateVerify);

		// Set SSL info callback.
		SSL_CTX_set_info_callback(sslCtx, [](const SSL* ssl, int where, int ret){
            static_cast<RTC::DtlsTransport*>(SSL_get_ex_data(ssl, 0))->OnSslInfo(where, ret);
        });
		// Set ciphers.
		ret = SSL_CTX_set_cipher_list(
		  sslCtx, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_set_cipher_list() failed");

			goto error;
		}

		// Enable ECDH ciphers.
		// DOC: http://en.wikibooks.org/wiki/OpenSSL/Diffie-Hellman_parameters
		// NOTE: https://code.google.com/p/chromium/issues/detail?id=406458
		// NOTE: https://bugs.ruby-lang.org/issues/12324

		// For OpenSSL >= 1.0.2.
		SSL_CTX_set_ecdh_auto(sslCtx, 1);

		// Set the "use_srtp" DTLS extension.
		for (auto it = DtlsTransport::srtpCryptoSuites.begin();
		     it != DtlsTransport::srtpCryptoSuites.end();
		     ++it)
		{
			if (it != DtlsTransport::srtpCryptoSuites.begin())
				dtlsSrtpCryptoSuites += ":";

			SrtpCryptoSuiteMapEntry* cryptoSuiteEntry = std::addressof(*it);
			dtlsSrtpCryptoSuites += cryptoSuiteEntry->name;
		}

		MS_DEBUG_2TAGS(dtls, srtp, "setting SRTP cryptoSuites for DTLS: %s", dtlsSrtpCryptoSuites.c_str());

		// NOTE: This function returns 0 on success.
		ret = SSL_CTX_set_tlsext_use_srtp(sslCtx, dtlsSrtpCryptoSuites.c_str());

		if (ret != 0)
		{
			MS_ERROR(
			  "SSL_CTX_set_tlsext_use_srtp() failed when entering '%s'", dtlsSrtpCryptoSuites.c_str());
			LOG_OPENSSL_ERROR("SSL_CTX_set_tlsext_use_srtp() failed");

			goto error;
		}

		return;

	error:

		if (sslCtx)
		{
			SSL_CTX_free(sslCtx);
			sslCtx = nullptr;
		}

		MS_THROW_ERROR("SSL context creation failed");
	}

	void DtlsTransport::DtlsEnvironment::GenerateFingerprints()
	{
		MS_TRACE();

		for (auto& kv : DtlsTransport::string2FingerprintAlgorithm)
		{
			const std::string& algorithmString = kv.first;
			FingerprintAlgorithm algorithm     = kv.second;
			uint8_t binaryFingerprint[EVP_MAX_MD_SIZE];
			unsigned int size{ 0 };
			char hexFingerprint[(EVP_MAX_MD_SIZE * 3) + 1];
			const EVP_MD* hashFunction;
			int ret;

			switch (algorithm)
			{
				case FingerprintAlgorithm::SHA1:
					hashFunction = EVP_sha1();
					break;

				case FingerprintAlgorithm::SHA224:
					hashFunction = EVP_sha224();
					break;

				case FingerprintAlgorithm::SHA256:
					hashFunction = EVP_sha256();
					break;

				case FingerprintAlgorithm::SHA384:
					hashFunction = EVP_sha384();
					break;

				case FingerprintAlgorithm::SHA512:
					hashFunction = EVP_sha512();
					break;

				default:
					MS_THROW_ERROR("unknown algorithm");
			}

			ret = X509_digest(certificate, hashFunction, binaryFingerprint, &size);

			if (ret == 0)
			{
				MS_ERROR("X509_digest() failed");
				MS_THROW_ERROR("Fingerprints generation failed");
			}

			// Convert to hexadecimal format in uppercase with colons.
			for (unsigned int i{ 0 }; i < size; ++i)
			{
				std::sprintf(hexFingerprint + (i * 3), "%.2X:", binaryFingerprint[i]);
			}
			hexFingerprint[(size * 3) - 1] = '\0';

			MS_DEBUG_TAG(dtls, "%-7s fingerprint: %s", algorithmString.c_str(), hexFingerprint);

			// Store it in the vector.
			DtlsTransport::Fingerprint fingerprint;

			fingerprint.algorithm = DtlsTransport::GetFingerprintAlgorithm(algorithmString);
			fingerprint.value     = hexFingerprint;

			localFingerprints.push_back(fingerprint);
		}
	}

	/* Instance methods. */

	DtlsTransport::DtlsTransport(EventPoller::Ptr poller,Listener* listener) : poller(std::move(poller)), listener(listener)
	{
		MS_TRACE();
        env = DtlsEnvironment::Instance().shared_from_this();

		/* Set SSL. */

		this->ssl = SSL_new(env->sslCtx);

		if (!this->ssl)
		{
			LOG_OPENSSL_ERROR("SSL_new() failed");

			goto error;
		}

		// Set this as custom data.
		SSL_set_ex_data(this->ssl, 0, static_cast<void*>(this));

		this->sslBioFromNetwork = BIO_new(BIO_s_mem());

		if (!this->sslBioFromNetwork)
		{
			LOG_OPENSSL_ERROR("BIO_new() failed");

			SSL_free(this->ssl);

			goto error;
		}

		this->sslBioToNetwork = BIO_new(BIO_s_mem());

		if (!this->sslBioToNetwork)
		{
			LOG_OPENSSL_ERROR("BIO_new() failed");

			BIO_free(this->sslBioFromNetwork);
			SSL_free(this->ssl);

			goto error;
		}

		SSL_set_bio(this->ssl, this->sslBioFromNetwork, this->sslBioToNetwork);

		// Set the MTU so that we don't send packets that are too large with no fragmentation.
		SSL_set_mtu(this->ssl, DtlsMtu);
		DTLS_set_link_mtu(this->ssl, DtlsMtu);

		// Set callback handler for setting DTLS timer interval.
		DTLS_set_timer_cb(this->ssl, onSslDtlsTimer);

		return;

	error:

		// NOTE: At this point SSL_set_bio() was not called so we must free BIOs as
		// well.
		if (this->sslBioFromNetwork)
			BIO_free(this->sslBioFromNetwork);

		if (this->sslBioToNetwork)
			BIO_free(this->sslBioToNetwork);

		if (this->ssl)
			SSL_free(this->ssl);

		// NOTE: If this is not catched by the caller the program will abort, but
		// this should never happen.
		MS_THROW_ERROR("DtlsTransport instance creation failed");
	}

	DtlsTransport::~DtlsTransport()
	{
		MS_TRACE();

		if (IsRunning())
		{
			// Send close alert to the peer.
			SSL_shutdown(this->ssl);
			SendPendingOutgoingDtlsData();
		}

		if (this->ssl)
		{
			SSL_free(this->ssl);

			this->ssl               = nullptr;
			this->sslBioFromNetwork = nullptr;
			this->sslBioToNetwork   = nullptr;
		}

		// Close the DTLS timer.
		this->timer = nullptr;
	}

	void DtlsTransport::Dump() const
	{
		MS_TRACE();

		std::string state{ "new" };
		std::string role{ "none " };

		switch (this->state)
		{
			case DtlsState::CONNECTING:
				state = "connecting";
				break;
			case DtlsState::CONNECTED:
				state = "connected";
				break;
			case DtlsState::FAILED:
				state = "failed";
				break;
			case DtlsState::CLOSED:
				state = "closed";
				break;
			default:;
		}

		switch (this->localRole)
		{
			case Role::AUTO:
				role = "auto";
				break;
			case Role::SERVER:
				role = "server";
				break;
			case Role::CLIENT:
				role = "client";
				break;
			default:;
		}

		MS_DUMP("<DtlsTransport>");
		MS_DUMP("  state           : %s", state.c_str());
		MS_DUMP("  role            : %s", role.c_str());
		MS_DUMP("  handshake done: : %s", this->handshakeDone ? "yes" : "no");
		MS_DUMP("</DtlsTransport>");
	}

	void DtlsTransport::Run(Role localRole)
	{
		MS_TRACE();

		MS_ASSERT(
		  localRole == Role::CLIENT || localRole == Role::SERVER,
		  "local DTLS role must be 'client' or 'server'");

		Role previousLocalRole = this->localRole;

		if (localRole == previousLocalRole)
		{
			MS_ERROR("same local DTLS role provided, doing nothing");

			return;
		}

		// If the previous local DTLS role was 'client' or 'server' do reset.
		if (previousLocalRole == Role::CLIENT || previousLocalRole == Role::SERVER)
		{
			MS_DEBUG_TAG(dtls, "resetting DTLS due to local role change");

			Reset();
		}

		// Update local role.
		this->localRole = localRole;

		// Set state and notify the listener.
		this->state = DtlsState::CONNECTING;
		this->listener->OnDtlsTransportConnecting(this);

		switch (this->localRole)
		{
			case Role::CLIENT:
			{
				MS_DEBUG_TAG(dtls, "running [role:client]");

				SSL_set_connect_state(this->ssl);
				SSL_do_handshake(this->ssl);
				SendPendingOutgoingDtlsData();
				SetTimeout();

				break;
			}

			case Role::SERVER:
			{
				MS_DEBUG_TAG(dtls, "running [role:server]");

				SSL_set_accept_state(this->ssl);
				SSL_do_handshake(this->ssl);

				break;
			}

			default:
			{
				MS_ABORT("invalid local DTLS role");
			}
		}
	}

	bool DtlsTransport::SetRemoteFingerprint(Fingerprint fingerprint)
	{
		MS_TRACE();

		MS_ASSERT(
		  fingerprint.algorithm != FingerprintAlgorithm::NONE, "no fingerprint algorithm provided");

		this->remoteFingerprint = fingerprint;

		// The remote fingerpring may have been set after DTLS handshake was done,
		// so we may need to process it now.
		if (this->handshakeDone && this->state != DtlsState::CONNECTED)
		{
			MS_DEBUG_TAG(dtls, "handshake already done, processing it right now");

			return ProcessHandshake();
		}

		return true;
	}

	void DtlsTransport::ProcessDtlsData(const uint8_t* data, size_t len)
	{
		MS_TRACE();

		int written;
		int read;

		if (!IsRunning())
		{
			MS_ERROR("cannot process data while not running");

			return;
		}

		// Write the received DTLS data into the sslBioFromNetwork.
		written =
		  BIO_write(this->sslBioFromNetwork, static_cast<const void*>(data), static_cast<int>(len));

		if (written != static_cast<int>(len))
		{
			MS_WARN_TAG(
			  dtls,
			  "OpenSSL BIO_write() wrote less (%zu bytes) than given data (%zu bytes)",
			  static_cast<size_t>(written),
			  len);
		}

		// Must call SSL_read() to process received DTLS data.
		read = SSL_read(this->ssl, static_cast<void*>(DtlsTransport::sslReadBuffer), SslReadBufferSize);

		// Send data if it's ready.
		SendPendingOutgoingDtlsData();

		// Check SSL status and return if it is bad/closed.
		if (!CheckStatus(read))
			return;

		// Set/update the DTLS timeout.
		if (!SetTimeout())
			return;

		// Application data received. Notify to the listener.
		if (read > 0)
		{
			// It is allowed to receive DTLS data even before validating remote fingerprint.
			if (!this->handshakeDone)
			{
				MS_WARN_TAG(dtls, "ignoring application data received while DTLS handshake not done");

				return;
			}

			// Notify the listener.
			this->listener->OnDtlsTransportApplicationDataReceived(
			  this, (uint8_t*)DtlsTransport::sslReadBuffer, static_cast<size_t>(read));
		}
	}

	void DtlsTransport::SendApplicationData(const uint8_t* data, size_t len)
	{
		MS_TRACE();

		// We cannot send data to the peer if its remote fingerprint is not validated.
		if (this->state != DtlsState::CONNECTED)
		{
			MS_WARN_TAG(dtls, "cannot send application data while DTLS is not fully connected");

			return;
		}

		if (len == 0)
		{
			MS_WARN_TAG(dtls, "ignoring 0 length data");

			return;
		}

		int written;

		written = SSL_write(this->ssl, static_cast<const void*>(data), static_cast<int>(len));

		if (written < 0)
		{
			LOG_OPENSSL_ERROR("SSL_write() failed");

			if (!CheckStatus(written))
				return;
		}
		else if (written != static_cast<int>(len))
		{
			MS_WARN_TAG(
			  dtls, "OpenSSL SSL_write() wrote less (%d bytes) than given data (%zu bytes)", written, len);
		}

		// Send data.
		SendPendingOutgoingDtlsData();
	}

	void DtlsTransport::Reset()
	{
		MS_TRACE();

		int ret;

		if (!IsRunning())
			return;

		MS_WARN_TAG(dtls, "resetting DTLS transport");

		// Stop the DTLS timer.
		this->timer = nullptr;

		// We need to reset the SSL instance so we need to "shutdown" it, but we
		// don't want to send a Close Alert to the peer, so just don't call
		// SendPendingOutgoingDTLSData().
		SSL_shutdown(this->ssl);

		this->localRole        = Role::NONE;
		this->state            = DtlsState::NEW;
		this->handshakeDone    = false;
		this->handshakeDoneNow = false;

		// Reset SSL status.
		// NOTE: For this to properly work, SSL_shutdown() must be called before.
		// NOTE: This may fail if not enough DTLS handshake data has been received,
		// but we don't care so just clear the error queue.
		ret = SSL_clear(this->ssl);

		if (ret == 0)
			ERR_clear_error();
	}

	inline bool DtlsTransport::CheckStatus(int returnCode)
	{
		MS_TRACE();

		int err;
		bool wasHandshakeDone = this->handshakeDone;

		err = SSL_get_error(this->ssl, returnCode);

		switch (err)
		{
			case SSL_ERROR_NONE:
				break;

			case SSL_ERROR_SSL:
				LOG_OPENSSL_ERROR("SSL status: SSL_ERROR_SSL");
				break;

			case SSL_ERROR_WANT_READ:
				break;

			case SSL_ERROR_WANT_WRITE:
				MS_WARN_TAG(dtls, "SSL status: SSL_ERROR_WANT_WRITE");
				break;

			case SSL_ERROR_WANT_X509_LOOKUP:
				MS_DEBUG_TAG(dtls, "SSL status: SSL_ERROR_WANT_X509_LOOKUP");
				break;

			case SSL_ERROR_SYSCALL:
				LOG_OPENSSL_ERROR("SSL status: SSL_ERROR_SYSCALL");
				break;

			case SSL_ERROR_ZERO_RETURN:
				break;

			case SSL_ERROR_WANT_CONNECT:
				MS_WARN_TAG(dtls, "SSL status: SSL_ERROR_WANT_CONNECT");
				break;

			case SSL_ERROR_WANT_ACCEPT:
				MS_WARN_TAG(dtls, "SSL status: SSL_ERROR_WANT_ACCEPT");
				break;

			default:
				MS_WARN_TAG(dtls, "SSL status: unknown error");
		}

		// Check if the handshake (or re-handshake) has been done right now.
		if (this->handshakeDoneNow)
		{
			this->handshakeDoneNow = false;
			this->handshakeDone    = true;

			// Stop the timer.
			this->timer = nullptr;

			// Process the handshake just once (ignore if DTLS renegotiation).
			if (!wasHandshakeDone && this->remoteFingerprint.algorithm != FingerprintAlgorithm::NONE)
				return ProcessHandshake();

			return true;
		}
		// Check if the peer sent close alert or a fatal error happened.
		else if (((SSL_get_shutdown(this->ssl) & SSL_RECEIVED_SHUTDOWN) != 0) || err == SSL_ERROR_SSL || err == SSL_ERROR_SYSCALL)
		{
			if (this->state == DtlsState::CONNECTED)
			{
				MS_DEBUG_TAG(dtls, "disconnected");

				Reset();

				// Set state and notify the listener.
				this->state = DtlsState::CLOSED;
				this->listener->OnDtlsTransportClosed(this);
			}
			else
			{
				MS_WARN_TAG(dtls, "connection failed");

				Reset();

				// Set state and notify the listener.
				this->state = DtlsState::FAILED;
				this->listener->OnDtlsTransportFailed(this);
			}

			return false;
		}
		else
		{
			return true;
		}
	}

	inline void DtlsTransport::SendPendingOutgoingDtlsData()
	{
		MS_TRACE();

		if (BIO_eof(this->sslBioToNetwork))
			return;

		int64_t read;
		char* data{ nullptr };

		read = BIO_get_mem_data(this->sslBioToNetwork, &data); // NOLINT

		if (read <= 0)
			return;

		MS_DEBUG_DEV("%" PRIu64 " bytes of DTLS data ready to sent to the peer", read);

		// Notify the listener.
		this->listener->OnDtlsTransportSendData(
		  this, reinterpret_cast<uint8_t*>(data), static_cast<size_t>(read));

		// Clear the BIO buffer.
		// NOTE: the (void) avoids the -Wunused-value warning.
		(void)BIO_reset(this->sslBioToNetwork);
	}

	inline bool DtlsTransport::SetTimeout()
	{
		MS_TRACE();

		MS_ASSERT(
		  this->state == DtlsState::CONNECTING || this->state == DtlsState::CONNECTED,
		  "invalid DTLS state");

		int64_t ret;
        struct timeval dtlsTimeout{ 0, 0 };
		uint64_t timeoutMs;

		// NOTE: If ret == 0 then ignore the value in dtlsTimeout.
		// NOTE: No DTLSv_1_2_get_timeout() or DTLS_get_timeout() in OpenSSL 1.1.0-dev.
		ret = DTLSv1_get_timeout(this->ssl, static_cast<void*>(&dtlsTimeout)); // NOLINT

		if (ret == 0)
			return true;

		timeoutMs = (dtlsTimeout.tv_sec * static_cast<uint64_t>(1000)) + (dtlsTimeout.tv_usec / 1000);

		if (timeoutMs == 0)
		{
			return true;
		}
		else if (timeoutMs < 30000)
		{
			MS_DEBUG_DEV("DTLS timer set in %" PRIu64 "ms", timeoutMs);

			weak_ptr<DtlsTransport> weak_self = shared_from_this();
			this->timer = std::make_shared<Timer>(timeoutMs / 1000.0f, [weak_self](){
			    auto strong_self = weak_self.lock();
			    if(strong_self){
                    strong_self->OnTimer();
			    }
                return true;
			}, this->poller);

			return true;
		}
		// NOTE: Don't start the timer again if the timeout is greater than 30 seconds.
		else
		{
			MS_WARN_TAG(dtls, "DTLS timeout too high (%" PRIu64 "ms), resetting DLTS", timeoutMs);

			Reset();

			// Set state and notify the listener.
			this->state = DtlsState::FAILED;
			this->listener->OnDtlsTransportFailed(this);

			return false;
		}
	}

	inline bool DtlsTransport::ProcessHandshake()
	{
		MS_TRACE();

		MS_ASSERT(this->handshakeDone, "handshake not done yet");
		MS_ASSERT(
		  this->remoteFingerprint.algorithm != FingerprintAlgorithm::NONE, "remote fingerprint not set");

		// Validate the remote fingerprint.
		if (!CheckRemoteFingerprint())
		{
			Reset();

			// Set state and notify the listener.
			this->state = DtlsState::FAILED;
			this->listener->OnDtlsTransportFailed(this);

			return false;
		}

		// Get the negotiated SRTP crypto suite.
		RTC::SrtpSession::CryptoSuite srtpCryptoSuite = GetNegotiatedSrtpCryptoSuite();

		if (srtpCryptoSuite != RTC::SrtpSession::CryptoSuite::NONE)
		{
			// Extract the SRTP keys (will notify the listener with them).
			ExtractSrtpKeys(srtpCryptoSuite);

			return true;
		}

		// NOTE: We assume that "use_srtp" DTLS extension is required even if
		// there is no audio/video.
		MS_WARN_2TAGS(dtls, srtp, "SRTP crypto suite not negotiated");

		Reset();

		// Set state and notify the listener.
		this->state = DtlsState::FAILED;
		this->listener->OnDtlsTransportFailed(this);

		return false;
	}

	inline bool DtlsTransport::CheckRemoteFingerprint()
	{
		MS_TRACE();

		MS_ASSERT(
		  this->remoteFingerprint.algorithm != FingerprintAlgorithm::NONE, "remote fingerprint not set");

		X509* certificate;
		uint8_t binaryFingerprint[EVP_MAX_MD_SIZE];
		unsigned int size{ 0 };
		char hexFingerprint[(EVP_MAX_MD_SIZE * 3) + 1];
		const EVP_MD* hashFunction;
		int ret;

		certificate = SSL_get_peer_certificate(this->ssl);

		if (!certificate)
		{
			MS_WARN_TAG(dtls, "no certificate was provided by the peer");

			return false;
		}

		switch (this->remoteFingerprint.algorithm)
		{
			case FingerprintAlgorithm::SHA1:
				hashFunction = EVP_sha1();
				break;

			case FingerprintAlgorithm::SHA224:
				hashFunction = EVP_sha224();
				break;

			case FingerprintAlgorithm::SHA256:
				hashFunction = EVP_sha256();
				break;

			case FingerprintAlgorithm::SHA384:
				hashFunction = EVP_sha384();
				break;

			case FingerprintAlgorithm::SHA512:
				hashFunction = EVP_sha512();
				break;

			default:
				MS_ABORT("unknown algorithm");
		}

		// Compare the remote fingerprint with the value given via signaling.
		ret = X509_digest(certificate, hashFunction, binaryFingerprint, &size);

		if (ret == 0)
		{
			MS_ERROR("X509_digest() failed");

			X509_free(certificate);

			return false;
		}

		// Convert to hexadecimal format in uppercase with colons.
		for (unsigned int i{ 0 }; i < size; ++i)
		{
			std::sprintf(hexFingerprint + (i * 3), "%.2X:", binaryFingerprint[i]);
		}
		hexFingerprint[(size * 3) - 1] = '\0';

		if (this->remoteFingerprint.value != hexFingerprint)
		{
			MS_WARN_TAG(
			  dtls,
			  "fingerprint in the remote certificate (%s) does not match the announced one (%s)",
			  hexFingerprint,
			  this->remoteFingerprint.value.c_str());
			X509_free(certificate);
			return false;
		}

		MS_DEBUG_TAG(dtls, "valid remote fingerprint");

		// Get the remote certificate in PEM format.

		BIO* bio = BIO_new(BIO_s_mem());

		// Ensure the underlying BUF_MEM structure is also freed.
		// NOTE: Avoid stupid "warning: value computed is not used [-Wunused-value]" since
		// BIO_set_close() always returns 1.
		(void)BIO_set_close(bio, BIO_CLOSE);

		ret = PEM_write_bio_X509(bio, certificate);

		if (ret != 1)
		{
			LOG_OPENSSL_ERROR("PEM_write_bio_X509() failed");

			X509_free(certificate);
			BIO_free(bio);

			return false;
		}

		BUF_MEM* mem;

		BIO_get_mem_ptr(bio, &mem); // NOLINT[cppcoreguidelines-pro-type-cstyle-cast]

		if (!mem || !mem->data || mem->length == 0u)
		{
			LOG_OPENSSL_ERROR("BIO_get_mem_ptr() failed");

			X509_free(certificate);
			BIO_free(bio);

			return false;
		}

		this->remoteCert = std::string(mem->data, mem->length);

		X509_free(certificate);
		BIO_free(bio);

		return true;
	}

	inline void DtlsTransport::ExtractSrtpKeys(RTC::SrtpSession::CryptoSuite srtpCryptoSuite)
	{
		MS_TRACE();

		size_t srtpKeyLength{ 0 };
		size_t srtpSaltLength{ 0 };
		size_t srtpMasterLength{ 0 };

		switch (srtpCryptoSuite)
		{
			case RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_80:
			case RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_32:
			{
				srtpKeyLength    = SrtpMasterKeyLength;
				srtpSaltLength   = SrtpMasterSaltLength;
				srtpMasterLength = SrtpMasterLength;

				break;
			}

			case RTC::SrtpSession::CryptoSuite::AEAD_AES_256_GCM:
			{
				srtpKeyLength    = SrtpAesGcm256MasterKeyLength;
				srtpSaltLength   = SrtpAesGcm256MasterSaltLength;
				srtpMasterLength = SrtpAesGcm256MasterLength;

				break;
			}

			case RTC::SrtpSession::CryptoSuite::AEAD_AES_128_GCM:
			{
				srtpKeyLength    = SrtpAesGcm128MasterKeyLength;
				srtpSaltLength   = SrtpAesGcm128MasterSaltLength;
				srtpMasterLength = SrtpAesGcm128MasterLength;

				break;
			}

			default:
			{
				MS_ABORT("unknown SRTP crypto suite");
			}
		}

		auto* srtpMaterial = new uint8_t[srtpMasterLength * 2];
		uint8_t* srtpLocalKey{ nullptr };
		uint8_t* srtpLocalSalt{ nullptr };
		uint8_t* srtpRemoteKey{ nullptr };
		uint8_t* srtpRemoteSalt{ nullptr };
		auto* srtpLocalMasterKey  = new uint8_t[srtpMasterLength];
		auto* srtpRemoteMasterKey = new uint8_t[srtpMasterLength];
		int ret;

		ret = SSL_export_keying_material(
		  this->ssl, srtpMaterial, srtpMasterLength * 2, "EXTRACTOR-dtls_srtp", 19, nullptr, 0, 0);

		MS_ASSERT(ret != 0, "SSL_export_keying_material() failed");

		switch (this->localRole)
		{
			case Role::SERVER:
			{
				srtpRemoteKey  = srtpMaterial;
				srtpLocalKey   = srtpRemoteKey + srtpKeyLength;
				srtpRemoteSalt = srtpLocalKey + srtpKeyLength;
				srtpLocalSalt  = srtpRemoteSalt + srtpSaltLength;

				break;
			}

			case Role::CLIENT:
			{
				srtpLocalKey   = srtpMaterial;
				srtpRemoteKey  = srtpLocalKey + srtpKeyLength;
				srtpLocalSalt  = srtpRemoteKey + srtpKeyLength;
				srtpRemoteSalt = srtpLocalSalt + srtpSaltLength;

				break;
			}

			default:
			{
				MS_ABORT("no DTLS role set");
			}
		}

		// Create the SRTP local master key.
		std::memcpy(srtpLocalMasterKey, srtpLocalKey, srtpKeyLength);
		std::memcpy(srtpLocalMasterKey + srtpKeyLength, srtpLocalSalt, srtpSaltLength);
		// Create the SRTP remote master key.
		std::memcpy(srtpRemoteMasterKey, srtpRemoteKey, srtpKeyLength);
		std::memcpy(srtpRemoteMasterKey + srtpKeyLength, srtpRemoteSalt, srtpSaltLength);

		// Set state and notify the listener.
		this->state = DtlsState::CONNECTED;
		this->listener->OnDtlsTransportConnected(
		  this,
		  srtpCryptoSuite,
		  srtpLocalMasterKey,
		  srtpMasterLength,
		  srtpRemoteMasterKey,
		  srtpMasterLength,
		  this->remoteCert);

		delete[] srtpMaterial;
		delete[] srtpLocalMasterKey;
		delete[] srtpRemoteMasterKey;
	}

	inline RTC::SrtpSession::CryptoSuite DtlsTransport::GetNegotiatedSrtpCryptoSuite()
	{
		MS_TRACE();

		RTC::SrtpSession::CryptoSuite negotiatedSrtpCryptoSuite = RTC::SrtpSession::CryptoSuite::NONE;

		// Ensure that the SRTP crypto suite has been negotiated.
		// NOTE: This is a OpenSSL type.
		SRTP_PROTECTION_PROFILE* sslSrtpCryptoSuite = SSL_get_selected_srtp_profile(this->ssl);

		if (!sslSrtpCryptoSuite)
			return negotiatedSrtpCryptoSuite;

		// Get the negotiated SRTP crypto suite.
		for (auto& srtpCryptoSuite : DtlsTransport::srtpCryptoSuites)
		{
			SrtpCryptoSuiteMapEntry* cryptoSuiteEntry = std::addressof(srtpCryptoSuite);

			if (std::strcmp(sslSrtpCryptoSuite->name, cryptoSuiteEntry->name) == 0)
			{
				MS_DEBUG_2TAGS(dtls, srtp, "chosen SRTP crypto suite: %s", cryptoSuiteEntry->name);

				negotiatedSrtpCryptoSuite = cryptoSuiteEntry->cryptoSuite;
			}
		}

		MS_ASSERT(
		  negotiatedSrtpCryptoSuite != RTC::SrtpSession::CryptoSuite::NONE,
		  "chosen SRTP crypto suite is not an available one");

		return negotiatedSrtpCryptoSuite;
	}

	inline void DtlsTransport::OnSslInfo(int where, int ret)
	{
		MS_TRACE();

		int w = where & -SSL_ST_MASK;
		const char* role;

		if ((w & SSL_ST_CONNECT) != 0)
			role = "client";
		else if ((w & SSL_ST_ACCEPT) != 0)
			role = "server";
		else
			role = "undefined";

		if ((where & SSL_CB_LOOP) != 0)
		{
			MS_DEBUG_TAG(dtls, "[role:%s, action:'%s']", role, SSL_state_string_long(this->ssl));
		}
		else if ((where & SSL_CB_ALERT) != 0)
		{
			const char* alertType;

			switch (*SSL_alert_type_string(ret))
			{
				case 'W':
					alertType = "warning";
					break;

				case 'F':
					alertType = "fatal";
					break;

				default:
					alertType = "undefined";
			}

			if ((where & SSL_CB_READ) != 0)
			{
				MS_WARN_TAG(dtls, "received DTLS %s alert: %s", alertType, SSL_alert_desc_string_long(ret));
			}
			else if ((where & SSL_CB_WRITE) != 0)
			{
				MS_DEBUG_TAG(dtls, "sending DTLS %s alert: %s", alertType, SSL_alert_desc_string_long(ret));
			}
			else
			{
				MS_DEBUG_TAG(dtls, "DTLS %s alert: %s", alertType, SSL_alert_desc_string_long(ret));
			}
		}
		else if ((where & SSL_CB_EXIT) != 0)
		{
			if (ret == 0)
				MS_DEBUG_TAG(dtls, "[role:%s, failed:'%s']", role, SSL_state_string_long(this->ssl));
			else if (ret < 0)
				MS_DEBUG_TAG(dtls, "role: %s, waiting:'%s']", role, SSL_state_string_long(this->ssl));
		}
		else if ((where & SSL_CB_HANDSHAKE_START) != 0)
		{
			MS_DEBUG_TAG(dtls, "DTLS handshake start");
		}
		else if ((where & SSL_CB_HANDSHAKE_DONE) != 0)
		{
			MS_DEBUG_TAG(dtls, "DTLS handshake done");

			this->handshakeDoneNow = true;
		}

		// NOTE: checking SSL_get_shutdown(this->ssl) & SSL_RECEIVED_SHUTDOWN here upon
		// receipt of a close alert does not work (the flag is set after this callback).
	}

	inline void DtlsTransport::OnTimer()
	{
		MS_TRACE();

		// Workaround for https://github.com/openssl/openssl/issues/7998.
		if (this->handshakeDone)
		{
			MS_DEBUG_DEV("handshake is done so return");

			return;
		}

		DTLSv1_handle_timeout(this->ssl);

		// If required, send DTLS data.
		SendPendingOutgoingDtlsData();

		// Set the DTLS timer again.
		SetTimeout();
	}
} // namespace RTC
