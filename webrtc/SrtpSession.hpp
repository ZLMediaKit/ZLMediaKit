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
		bool EncryptRtp(uint8_t* data, size_t* len);
		bool DecryptSrtp(uint8_t* data, size_t* len);
		bool EncryptRtcp(uint8_t* data, size_t* len);
		bool DecryptSrtcp(uint8_t* data, size_t* len);
		void RemoveStream(uint32_t ssrc)
		{
			srtp_remove_stream(this->session, uint32_t{ htonl(ssrc) });
		}

	private:
		// Allocated by this.
		srtp_t session{ nullptr };
        DepLibSRTP::Ptr _env;
	};
} // namespace RTC

#endif
