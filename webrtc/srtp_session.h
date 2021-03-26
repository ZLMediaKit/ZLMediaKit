#ifndef MS_RTC_SRTP_SESSION_HPP
#define MS_RTC_SRTP_SESSION_HPP

#include "utils.h"
#include <srtp2/srtp.h>
#include <vector>

namespace RTC
{
    class DepLibSRTP {
    public:
        static void ClassInit();
        static void ClassDestroy();
        static bool IsError(srtp_err_status_t code) { return (code != srtp_err_status_ok); }
        static const char *GetErrorString(srtp_err_status_t code) {
            // This throws out_of_range if the given index is not in the vector.
            return DepLibSRTP::errors.at(code);
        }

    private:
        static std::vector<const char *> errors;
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
		static void ClassInit();

	private:
		static void OnSrtpEvent(srtp_event_data_t* data);

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
	};
} // namespace RTC

#endif
