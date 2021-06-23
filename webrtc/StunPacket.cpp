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

#define MS_CLASS "RTC::StunPacket"
// #define MS_LOG_DEV_LEVEL 3

#include "StunPacket.hpp"
#include <cstdio>  // std::snprintf()
#include <cstring> // std::memcmp(), std::memcpy()

namespace RTC
{
    static const uint32_t crc32Table[] =
    {
            0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
            0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
            0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
            0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
            0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
            0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
            0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
            0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
            0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
            0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
            0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
            0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
            0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
            0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
            0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
            0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
            0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
            0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
            0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
            0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
            0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
            0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
            0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
            0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
            0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
            0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
            0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
            0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
            0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
            0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
            0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
            0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    inline uint32_t GetCRC32(const uint8_t *data, size_t size) {
        uint32_t crc{0xFFFFFFFF};
        const uint8_t *p = data;

        while (size--) {
            crc = crc32Table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
        }

        return crc ^ ~0U;
    }

    static std::string openssl_HMACsha1(const void *key, size_t key_len, const void *data, size_t data_len){
        std::string str;
        str.resize(20);
        unsigned int out_len;
#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10100000L)
        //openssl 1.1.0新增api，老版本api作废
        HMAC_CTX *ctx = HMAC_CTX_new();
        HMAC_CTX_reset(ctx);
        HMAC_Init_ex(ctx, key, (int)key_len, EVP_sha1(), NULL);
        HMAC_Update(ctx, (unsigned char*)data, data_len);
        HMAC_Final(ctx, (unsigned char *)str.data(), &out_len);
        HMAC_CTX_reset(ctx);
        HMAC_CTX_free(ctx);
#else
        HMAC_CTX ctx;
        HMAC_CTX_init(&ctx);
        HMAC_Init_ex(&ctx, key, key_len, EVP_sha1(), NULL);
        HMAC_Update(&ctx, (unsigned char*)data, data_len);
        HMAC_Final(&ctx, (unsigned char *)str.data(), &out_len);
        HMAC_CTX_cleanup(&ctx);
#endif //defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10100000L)
        return str;
    }

	/* Class variables. */

	const uint8_t StunPacket::magicCookie[] = { 0x21, 0x12, 0xA4, 0x42 };

	/* Class methods. */

	StunPacket* StunPacket::Parse(const uint8_t* data, size_t len)
	{
		MS_TRACE();

		if (!StunPacket::IsStun(data, len))
			return nullptr;

		/*
		  The message type field is decomposed further into the following
		    structure:

		    0                 1
		    2  3  4 5 6 7 8 9 0 1 2 3 4 5
		       +--+--+-+-+-+-+-+-+-+-+-+-+-+-+
		       |M |M |M|M|M|C|M|M|M|C|M|M|M|M|
		       |11|10|9|8|7|1|6|5|4|0|3|2|1|0|
		       +--+--+-+-+-+-+-+-+-+-+-+-+-+-+

		    Figure 3: Format of STUN Message Type Field

		   Here the bits in the message type field are shown as most significant
		   (M11) through least significant (M0).  M11 through M0 represent a 12-
		   bit encoding of the method.  C1 and C0 represent a 2-bit encoding of
		   the class.
		 */

		// Get type field.
		uint16_t msgType = Utils::Byte::Get2Bytes(data, 0);

		// Get length field.
		uint16_t msgLength = Utils::Byte::Get2Bytes(data, 2);

		// length field must be total size minus header's 20 bytes, and must be multiple of 4 Bytes.
		if ((static_cast<size_t>(msgLength) != len - 20) || ((msgLength & 0x03) != 0))
		{
			MS_WARN_TAG(
			  ice,
			  "length field + 20 does not match total size (or it is not multiple of 4 bytes), "
			  "packet discarded");

			return nullptr;
		}

		// Get STUN method.
		uint16_t msgMethod = (msgType & 0x000f) | ((msgType & 0x00e0) >> 1) | ((msgType & 0x3E00) >> 2);

		// Get STUN class.
		uint16_t msgClass = ((data[0] & 0x01) << 1) | ((data[1] & 0x10) >> 4);

		// Create a new StunPacket (data + 8 points to the received TransactionID field).
		auto* packet = new StunPacket(
		  static_cast<Class>(msgClass), static_cast<Method>(msgMethod), data + 8, data, len);

		/*
		    STUN Attributes

		    After the STUN header are zero or more attributes.  Each attribute
		    MUST be TLV encoded, with a 16-bit type, 16-bit length, and value.
		    Each STUN attribute MUST end on a 32-bit boundary.  As mentioned
		    above, all fields in an attribute are transmitted most significant
		    bit first.

		        0                   1                   2                   3
		        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |         Type                  |            Length             |
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |                         Value (variable)                ....
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */

		// Start looking for attributes after STUN header (Byte #20).
		size_t pos{ 20 };
		// Flags (positions) for special MESSAGE-INTEGRITY and FINGERPRINT attributes.
		bool hasMessageIntegrity{ false };
		bool hasFingerprint{ false };
		size_t fingerprintAttrPos; // Will point to the beginning of the attribute.
		uint32_t fingerprint;      // Holds the value of the FINGERPRINT attribute.

		// Ensure there are at least 4 remaining bytes (attribute with 0 length).
		while (pos + 4 <= len)
		{
			// Get the attribute type.
			auto attrType = static_cast<Attribute>(Utils::Byte::Get2Bytes(data, pos));

			// Get the attribute length.
			uint16_t attrLength = Utils::Byte::Get2Bytes(data, pos + 2);

			// Ensure the attribute length is not greater than the remaining size.
			if ((pos + 4 + attrLength) > len)
			{
				MS_WARN_TAG(ice, "the attribute length exceeds the remaining size, packet discarded");

				delete packet;
				return nullptr;
			}

			// FINGERPRINT must be the last attribute.
			if (hasFingerprint)
			{
				MS_WARN_TAG(ice, "attribute after FINGERPRINT is not allowed, packet discarded");

				delete packet;
				return nullptr;
			}

			// After a MESSAGE-INTEGRITY attribute just FINGERPRINT is allowed.
			if (hasMessageIntegrity && attrType != Attribute::FINGERPRINT)
			{
				MS_WARN_TAG(
				  ice,
				  "attribute after MESSAGE-INTEGRITY other than FINGERPRINT is not allowed, "
				  "packet discarded");

				delete packet;
				return nullptr;
			}

			const uint8_t* attrValuePos = data + pos + 4;

			switch (attrType)
			{
				case Attribute::USERNAME:
				{
					packet->SetUsername(
					  reinterpret_cast<const char*>(attrValuePos), static_cast<size_t>(attrLength));

					break;
				}

				case Attribute::PRIORITY:
				{
					// Ensure attribute length is 4 bytes.
					if (attrLength != 4)
					{
						MS_WARN_TAG(ice, "attribute PRIORITY must be 4 bytes length, packet discarded");

						delete packet;
						return nullptr;
					}

					packet->SetPriority(Utils::Byte::Get4Bytes(attrValuePos, 0));

					break;
				}

				case Attribute::ICE_CONTROLLING:
				{
					// Ensure attribute length is 8 bytes.
					if (attrLength != 8)
					{
						MS_WARN_TAG(ice, "attribute ICE-CONTROLLING must be 8 bytes length, packet discarded");

						delete packet;
						return nullptr;
					}

					packet->SetIceControlling(Utils::Byte::Get8Bytes(attrValuePos, 0));

					break;
				}

				case Attribute::ICE_CONTROLLED:
				{
					// Ensure attribute length is 8 bytes.
					if (attrLength != 8)
					{
						MS_WARN_TAG(ice, "attribute ICE-CONTROLLED must be 8 bytes length, packet discarded");

						delete packet;
						return nullptr;
					}

					packet->SetIceControlled(Utils::Byte::Get8Bytes(attrValuePos, 0));

					break;
				}

				case Attribute::USE_CANDIDATE:
				{
					// Ensure attribute length is 0 bytes.
					if (attrLength != 0)
					{
						MS_WARN_TAG(ice, "attribute USE-CANDIDATE must be 0 bytes length, packet discarded");

						delete packet;
						return nullptr;
					}

					packet->SetUseCandidate();

					break;
				}

				case Attribute::MESSAGE_INTEGRITY:
				{
					// Ensure attribute length is 20 bytes.
					if (attrLength != 20)
					{
						MS_WARN_TAG(ice, "attribute MESSAGE-INTEGRITY must be 20 bytes length, packet discarded");

						delete packet;
						return nullptr;
					}

					hasMessageIntegrity = true;
					packet->SetMessageIntegrity(attrValuePos);

					break;
				}

				case Attribute::FINGERPRINT:
				{
					// Ensure attribute length is 4 bytes.
					if (attrLength != 4)
					{
						MS_WARN_TAG(ice, "attribute FINGERPRINT must be 4 bytes length, packet discarded");

						delete packet;
						return nullptr;
					}

					hasFingerprint     = true;
					fingerprintAttrPos = pos;
					fingerprint        = Utils::Byte::Get4Bytes(attrValuePos, 0);
					packet->SetFingerprint();

					break;
				}

				case Attribute::ERROR_CODE:
				{
					// Ensure attribute length >= 4bytes.
					if (attrLength < 4)
					{
						MS_WARN_TAG(ice, "attribute ERROR-CODE must be >= 4bytes length, packet discarded");

						delete packet;
						return nullptr;
					}

					uint8_t errorClass  = Utils::Byte::Get1Byte(attrValuePos, 2);
					uint8_t errorNumber = Utils::Byte::Get1Byte(attrValuePos, 3);
					auto errorCode      = static_cast<uint16_t>(errorClass * 100 + errorNumber);

					packet->SetErrorCode(errorCode);

					break;
				}

				default:;
			}

			// Set next attribute position.
			pos =
			  static_cast<size_t>(Utils::Byte::PadTo4Bytes(static_cast<uint16_t>(pos + 4 + attrLength)));
		}

		// Ensure current position matches the total length.
		if (pos != len)
		{
			MS_WARN_TAG(ice, "computed packet size does not match total size, packet discarded");

			delete packet;
			return nullptr;
		}

		// If it has FINGERPRINT attribute then verify it.
		if (hasFingerprint)
		{
			// Compute the CRC32 of the received packet up to (but excluding) the
			// FINGERPRINT attribute and XOR it with 0x5354554e.
			uint32_t computedFingerprint = GetCRC32(data, fingerprintAttrPos) ^ 0x5354554e;

			// Compare with the FINGERPRINT value in the packet.
			if (fingerprint != computedFingerprint)
			{
				MS_WARN_TAG(
				  ice,
				  "computed FINGERPRINT value does not match the value in the packet, "
				  "packet discarded");

				delete packet;
				return nullptr;
			}
		}

		return packet;
	}

	/* Instance methods. */

	StunPacket::StunPacket(
	  Class klass, Method method, const uint8_t* transactionId, const uint8_t* data, size_t size)
	  : klass(klass), method(method), transactionId(transactionId), data(const_cast<uint8_t*>(data)),
	    size(size)
	{
		MS_TRACE();
	}

	StunPacket::~StunPacket()
	{
		MS_TRACE();
	}

#if 0
	void StunPacket::Dump() const
	{
		MS_TRACE();

		MS_DUMP("<StunPacket>");

		std::string klass;
		switch (this->klass)
		{
			case Class::REQUEST:
				klass = "Request";
				break;
			case Class::INDICATION:
				klass = "Indication";
				break;
			case Class::SUCCESS_RESPONSE:
				klass = "SuccessResponse";
				break;
			case Class::ERROR_RESPONSE:
				klass = "ErrorResponse";
				break;
		}
		if (this->method == Method::BINDING)
		{
			MS_DUMP("  Binding %s", klass.c_str());
		}
		else
		{
			// This prints the unknown method number. Example: TURN Allocate => 0x003.
			MS_DUMP("  %s with unknown method %#.3x", klass.c_str(), static_cast<uint16_t>(this->method));
		}
		MS_DUMP("  size: %zu bytes", this->size);

		static char transactionId[25];

		for (int i{ 0 }; i < 12; ++i)
		{
			// NOTE: n must be 3 because snprintf adds a \0 after printed chars.
			std::snprintf(transactionId + (i * 2), 3, "%.2x", this->transactionId[i]);
		}
		MS_DUMP("  transactionId: %s", transactionId);
		if (this->errorCode != 0u)
			MS_DUMP("  errorCode: %" PRIu16, this->errorCode);
		if (!this->username.empty())
			MS_DUMP("  username: %s", this->username.c_str());
		if (this->priority != 0u)
			MS_DUMP("  priority: %" PRIu32, this->priority);
		if (this->iceControlling != 0u)
			MS_DUMP("  iceControlling: %" PRIu64, this->iceControlling);
		if (this->iceControlled != 0u)
			MS_DUMP("  iceControlled: %" PRIu64, this->iceControlled);
		if (this->hasUseCandidate)
			MS_DUMP("  useCandidate");
		if (this->xorMappedAddress != nullptr)
		{
			int family;
			uint16_t port;
			std::string ip;

			Utils::IP::GetAddressInfo(this->xorMappedAddress, family, ip, port);

			MS_DUMP("  xorMappedAddress: %s : %" PRIu16, ip.c_str(), port);
		}
		if (this->messageIntegrity != nullptr)
		{
			static char messageIntegrity[41];

			for (int i{ 0 }; i < 20; ++i)
			{
				std::snprintf(messageIntegrity + (i * 2), 3, "%.2x", this->messageIntegrity[i]);
			}

			MS_DUMP("  messageIntegrity: %s", messageIntegrity);
		}
		if (this->hasFingerprint)
			MS_DUMP("  has fingerprint");

		MS_DUMP("</StunPacket>");
	}
#endif

	StunPacket::Authentication StunPacket::CheckAuthentication(
	  const std::string& localUsername, const std::string& localPassword)
	{
		MS_TRACE();

		switch (this->klass)
		{
			case Class::REQUEST:
			case Class::INDICATION:
			{
				// Both USERNAME and MESSAGE-INTEGRITY must be present.
				if (!this->messageIntegrity || this->username.empty())
					return Authentication::BAD_REQUEST;

				// Check that USERNAME attribute begins with our local username plus ":".
				size_t localUsernameLen = localUsername.length();

				if (
				  this->username.length() <= localUsernameLen || this->username.at(localUsernameLen) != ':' ||
				  (this->username.compare(0, localUsernameLen, localUsername) != 0))
				{
					return Authentication::UNAUTHORIZED;
				}

				break;
			}
			// This method cannot check authentication in received responses (as we
			// are ICE-Lite and don't generate requests).
			case Class::SUCCESS_RESPONSE:
			case Class::ERROR_RESPONSE:
			{
				MS_ERROR("cannot check authentication for a STUN response");

				return Authentication::BAD_REQUEST;
			}
		}

		// If there is FINGERPRINT it must be discarded for MESSAGE-INTEGRITY calculation,
		// so the header length field must be modified (and later restored).
		if (this->hasFingerprint)
			// Set the header length field: full size - header length (20) - FINGERPRINT length (8).
			Utils::Byte::Set2Bytes(this->data, 2, static_cast<uint16_t>(this->size - 20 - 8));

		// Calculate the HMAC-SHA1 of the message according to MESSAGE-INTEGRITY rules.
        auto computedMessageIntegrity = openssl_HMACsha1(
                localPassword.data(),localPassword.size(), this->data, (this->messageIntegrity - 4) - this->data);

		Authentication result;

		// Compare the computed HMAC-SHA1 with the MESSAGE-INTEGRITY in the packet.
		if (std::memcmp(this->messageIntegrity, computedMessageIntegrity.data(), computedMessageIntegrity.size()) == 0)
			result = Authentication::OK;
		else
			result = Authentication::UNAUTHORIZED;

		// Restore the header length field.
		if (this->hasFingerprint)
			Utils::Byte::Set2Bytes(this->data, 2, static_cast<uint16_t>(this->size - 20));

		return result;
	}

	StunPacket* StunPacket::CreateSuccessResponse()
	{
		MS_TRACE();

		MS_ASSERT(
		  this->klass == Class::REQUEST,
		  "attempt to create a success response for a non Request STUN packet");

		return new StunPacket(Class::SUCCESS_RESPONSE, this->method, this->transactionId, nullptr, 0);
	}

	StunPacket* StunPacket::CreateErrorResponse(uint16_t errorCode)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->klass == Class::REQUEST,
		  "attempt to create an error response for a non Request STUN packet");

		auto* response =
		  new StunPacket(Class::ERROR_RESPONSE, this->method, this->transactionId, nullptr, 0);

		response->SetErrorCode(errorCode);

		return response;
	}

	void StunPacket::Authenticate(const std::string& password)
	{
		// Just for Request, Indication and SuccessResponse messages.
		if (this->klass == Class::ERROR_RESPONSE)
		{
			MS_ERROR("cannot set password for ErrorResponse messages");

			return;
		}

		this->password = password;
	}

	void StunPacket::Serialize(uint8_t* buffer)
	{
		MS_TRACE();

		// Some useful variables.
		uint16_t usernamePaddedLen{ 0 };
		uint16_t xorMappedAddressPaddedLen{ 0 };
		bool addXorMappedAddress =
		  ((this->xorMappedAddress != nullptr) && this->method == StunPacket::Method::BINDING &&
		   this->klass == Class::SUCCESS_RESPONSE);
		bool addErrorCode        = ((this->errorCode != 0u) && this->klass == Class::ERROR_RESPONSE);
		bool addMessageIntegrity = (this->klass != Class::ERROR_RESPONSE && !this->password.empty());
		bool addFingerprint{ true }; // Do always.

		// Update data pointer.
		this->data = buffer;

		// First calculate the total required size for the entire packet.
		this->size = 20; // Header.

		if (!this->username.empty())
		{
			usernamePaddedLen = Utils::Byte::PadTo4Bytes(static_cast<uint16_t>(this->username.length()));
			this->size += 4 + usernamePaddedLen;
		}

		if (this->priority != 0u)
			this->size += 4 + 4;

		if (this->iceControlling != 0u)
			this->size += 4 + 8;

		if (this->iceControlled != 0u)
			this->size += 4 + 8;

		if (this->hasUseCandidate)
			this->size += 4;

		if (addXorMappedAddress)
		{
			switch (this->xorMappedAddress->sa_family)
			{
				case AF_INET:
				{
					xorMappedAddressPaddedLen = 8;
					this->size += 4 + 8;

					break;
				}

				case AF_INET6:
				{
					xorMappedAddressPaddedLen = 20;
					this->size += 4 + 20;

					break;
				}

				default:
				{
					MS_ERROR("invalid inet family in XOR-MAPPED-ADDRESS attribute");

					addXorMappedAddress = false;
				}
			}
		}

		if (addErrorCode)
			this->size += 4 + 4;

		if (addMessageIntegrity)
			this->size += 4 + 20;

		if (addFingerprint)
			this->size += 4 + 4;

		// Merge class and method fields into type.
		uint16_t typeField = (static_cast<uint16_t>(this->method) & 0x0f80) << 2;

		typeField |= (static_cast<uint16_t>(this->method) & 0x0070) << 1;
		typeField |= (static_cast<uint16_t>(this->method) & 0x000f);
		typeField |= (static_cast<uint16_t>(this->klass) & 0x02) << 7;
		typeField |= (static_cast<uint16_t>(this->klass) & 0x01) << 4;

		// Set type field.
		Utils::Byte::Set2Bytes(buffer, 0, typeField);
		// Set length field.
		Utils::Byte::Set2Bytes(buffer, 2, static_cast<uint16_t>(this->size) - 20);
		// Set magic cookie.
		std::memcpy(buffer + 4, StunPacket::magicCookie, 4);
		// Set TransactionId field.
		std::memcpy(buffer + 8, this->transactionId, 12);
		// Update the transaction ID pointer.
		this->transactionId = buffer + 8;
		// Add atributes.
		size_t pos{ 20 };

		// Add USERNAME.
		if (usernamePaddedLen != 0u)
		{
			Utils::Byte::Set2Bytes(buffer, pos, static_cast<uint16_t>(Attribute::USERNAME));
			Utils::Byte::Set2Bytes(buffer, pos + 2, static_cast<uint16_t>(this->username.length()));
			std::memcpy(buffer + pos + 4, this->username.c_str(), this->username.length());
			pos += 4 + usernamePaddedLen;
		}

		// Add PRIORITY.
		if (this->priority != 0u)
		{
			Utils::Byte::Set2Bytes(buffer, pos, static_cast<uint16_t>(Attribute::PRIORITY));
			Utils::Byte::Set2Bytes(buffer, pos + 2, 4);
			Utils::Byte::Set4Bytes(buffer, pos + 4, this->priority);
			pos += 4 + 4;
		}

		// Add ICE-CONTROLLING.
		if (this->iceControlling != 0u)
		{
			Utils::Byte::Set2Bytes(buffer, pos, static_cast<uint16_t>(Attribute::ICE_CONTROLLING));
			Utils::Byte::Set2Bytes(buffer, pos + 2, 8);
			Utils::Byte::Set8Bytes(buffer, pos + 4, this->iceControlling);
			pos += 4 + 8;
		}

		// Add ICE-CONTROLLED.
		if (this->iceControlled != 0u)
		{
			Utils::Byte::Set2Bytes(buffer, pos, static_cast<uint16_t>(Attribute::ICE_CONTROLLED));
			Utils::Byte::Set2Bytes(buffer, pos + 2, 8);
			Utils::Byte::Set8Bytes(buffer, pos + 4, this->iceControlled);
			pos += 4 + 8;
		}

		// Add USE-CANDIDATE.
		if (this->hasUseCandidate)
		{
			Utils::Byte::Set2Bytes(buffer, pos, static_cast<uint16_t>(Attribute::USE_CANDIDATE));
			Utils::Byte::Set2Bytes(buffer, pos + 2, 0);
			pos += 4;
		}

		// Add XOR-MAPPED-ADDRESS
		if (addXorMappedAddress)
		{
			Utils::Byte::Set2Bytes(buffer, pos, static_cast<uint16_t>(Attribute::XOR_MAPPED_ADDRESS));
			Utils::Byte::Set2Bytes(buffer, pos + 2, xorMappedAddressPaddedLen);

			uint8_t* attrValue = buffer + pos + 4;

			switch (this->xorMappedAddress->sa_family)
			{
				case AF_INET:
				{
					// Set first byte to 0.
					attrValue[0] = 0;
					// Set inet family.
					attrValue[1] = 0x01;
					// Set port and XOR it.
					std::memcpy(
					  attrValue + 2,
					  &(reinterpret_cast<const sockaddr_in*>(this->xorMappedAddress))->sin_port,
					  2);
					attrValue[2] ^= StunPacket::magicCookie[0];
					attrValue[3] ^= StunPacket::magicCookie[1];
					// Set address and XOR it.
					std::memcpy(
					  attrValue + 4,
					  &(reinterpret_cast<const sockaddr_in*>(this->xorMappedAddress))->sin_addr.s_addr,
					  4);
					attrValue[4] ^= StunPacket::magicCookie[0];
					attrValue[5] ^= StunPacket::magicCookie[1];
					attrValue[6] ^= StunPacket::magicCookie[2];
					attrValue[7] ^= StunPacket::magicCookie[3];

					pos += 4 + 8;

					break;
				}

				case AF_INET6:
				{
					// Set first byte to 0.
					attrValue[0] = 0;
					// Set inet family.
					attrValue[1] = 0x02;
					// Set port and XOR it.
					std::memcpy(
					  attrValue + 2,
					  &(reinterpret_cast<const sockaddr_in6*>(this->xorMappedAddress))->sin6_port,
					  2);
					attrValue[2] ^= StunPacket::magicCookie[0];
					attrValue[3] ^= StunPacket::magicCookie[1];
					// Set address and XOR it.
					std::memcpy(
					  attrValue + 4,
					  &(reinterpret_cast<const sockaddr_in6*>(this->xorMappedAddress))->sin6_addr.s6_addr,
					  16);
					attrValue[4] ^= StunPacket::magicCookie[0];
					attrValue[5] ^= StunPacket::magicCookie[1];
					attrValue[6] ^= StunPacket::magicCookie[2];
					attrValue[7] ^= StunPacket::magicCookie[3];
					attrValue[8] ^= this->transactionId[0];
					attrValue[9] ^= this->transactionId[1];
					attrValue[10] ^= this->transactionId[2];
					attrValue[11] ^= this->transactionId[3];
					attrValue[12] ^= this->transactionId[4];
					attrValue[13] ^= this->transactionId[5];
					attrValue[14] ^= this->transactionId[6];
					attrValue[15] ^= this->transactionId[7];
					attrValue[16] ^= this->transactionId[8];
					attrValue[17] ^= this->transactionId[9];
					attrValue[18] ^= this->transactionId[10];
					attrValue[19] ^= this->transactionId[11];

					pos += 4 + 20;

					break;
				}
			}
		}

		// Add ERROR-CODE.
		if (addErrorCode)
		{
			Utils::Byte::Set2Bytes(buffer, pos, static_cast<uint16_t>(Attribute::ERROR_CODE));
			Utils::Byte::Set2Bytes(buffer, pos + 2, 4);

			auto codeClass     = static_cast<uint8_t>(this->errorCode / 100);
			uint8_t codeNumber = static_cast<uint8_t>(this->errorCode) - (codeClass * 100);

			Utils::Byte::Set2Bytes(buffer, pos + 4, 0);
			Utils::Byte::Set1Byte(buffer, pos + 6, codeClass);
			Utils::Byte::Set1Byte(buffer, pos + 7, codeNumber);
			pos += 4 + 4;
		}

		// Add MESSAGE-INTEGRITY.
		if (addMessageIntegrity)
		{
			// Ignore FINGERPRINT.
			if (addFingerprint)
				Utils::Byte::Set2Bytes(buffer, 2, static_cast<uint16_t>(this->size - 20 - 8));

			// Calculate the HMAC-SHA1 of the packet according to MESSAGE-INTEGRITY rules.
            auto computedMessageIntegrity = openssl_HMACsha1(this->password.data(), this->password.size(), buffer, pos);

            Utils::Byte::Set2Bytes(buffer, pos, static_cast<uint16_t>(Attribute::MESSAGE_INTEGRITY));
			Utils::Byte::Set2Bytes(buffer, pos + 2, 20);
			std::memcpy(buffer + pos + 4, computedMessageIntegrity.data(), computedMessageIntegrity.size());

			// Update the pointer.
			this->messageIntegrity = buffer + pos + 4;
			pos += 4 + 20;

			// Restore length field.
			if (addFingerprint)
				Utils::Byte::Set2Bytes(buffer, 2, static_cast<uint16_t>(this->size - 20));
		}
		else
		{
			// Unset the pointer (if it was set).
			this->messageIntegrity = nullptr;
		}

		// Add FINGERPRINT.
		if (addFingerprint)
		{
			// Compute the CRC32 of the packet up to (but excluding) the FINGERPRINT
			// attribute and XOR it with 0x5354554e.
			uint32_t computedFingerprint = GetCRC32(buffer, pos) ^ 0x5354554e;

			Utils::Byte::Set2Bytes(buffer, pos, static_cast<uint16_t>(Attribute::FINGERPRINT));
			Utils::Byte::Set2Bytes(buffer, pos + 2, 4);
			Utils::Byte::Set4Bytes(buffer, pos + 4, computedFingerprint);
			pos += 4 + 4;

			// Set flag.
			this->hasFingerprint = true;
		}
		else
		{
			this->hasFingerprint = false;
		}

		MS_ASSERT(pos == this->size, "pos != this->size");
	}
} // namespace RTC
