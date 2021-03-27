#ifndef MS_RTC_SRTP_SESSION_HPP
#define MS_RTC_SRTP_SESSION_HPP

#include "Utils.hpp"
#include <srtp2/srtp.h>
#include <vector>
#include <memory>

namespace RTC
{
    class DepLibSRTP : public std::enable_shared_from_this<DepLibSRTP>
    {
    public:
        using Ptr = std::shared_ptr<DepLibSRTP>;
        ~DepLibSRTP();

        static bool IsError(srtp_err_status_t code);
        static const char *GetErrorString(srtp_err_status_t code);
        static DepLibSRTP &Instance();

    private:
        DepLibSRTP();
    };

	class SrtpSession
	{
	public:
		enum class CryptoSuite
		{
			NONE                    = 0,
			AES_CM_128_HMAC_SHA1_80 = 1,
			AES_CM_128_HMAC_SHA1_32,
			AEAD_AES_256_GCM,
			AEAD_AES_128_GCM
		};

	public:
		enum class Type
		{
			INBOUND = 1,
			OUTBOUND
		};

	public:
		SrtpSession(Type type, CryptoSuite cryptoSuite, uint8_t* key, size_t keyLen);
		~SrtpSession();

	public:
		bool EncryptRtp(const uint8_t** data, size_t* len);
		bool DecryptSrtp(uint8_t* data, size_t* len);
		bool EncryptRtcp(const uint8_t** data, size_t* len);
		bool DecryptSrtcp(uint8_t* data, size_t* len);
		void RemoveStream(uint32_t ssrc)
		{
			srtp_remove_stream(this->session, uint32_t{ htonl(ssrc) });
		}

	private:
		// Allocated by this.
		srtp_t session{ nullptr };
		//rtp包最大1600
        static constexpr size_t EncryptBufferSize{ 1600 };
        uint8_t EncryptBuffer[EncryptBufferSize];
        DepLibSRTP::Ptr _env;
	};
} // namespace RTC

#endif
