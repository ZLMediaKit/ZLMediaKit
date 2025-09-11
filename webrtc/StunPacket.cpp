/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
*/

#include "StunPacket.hpp"
#include <cstdio>  // std::snprintf()
#include <cstring> // std::memcmp(), std::memcpy()
#include <openssl/hmac.h>
#include "Util/logger.h"
#include "Common/macros.h"

using namespace std;
using namespace toolkit;

namespace RTC
{

static const uint32_t crc32Table[] = {
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

inline uint32_t getCRC32(const uint8_t *data, size_t size) {
    uint32_t crc { 0xFFFFFFFF };
    const uint8_t *p = data;

    while (size--) {
        crc = crc32Table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ ~0U;
}

static std::string openssl_HMACsha1(const void *key, size_t key_len, const void *data, size_t data_len) {
    std::string str;
    str.resize(20);
    unsigned int out_len;
#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10100000L)
    // openssl 1.1.0新增api，老版本api作废
    HMAC_CTX *ctx = HMAC_CTX_new();
    HMAC_CTX_reset(ctx);
    HMAC_Init_ex(ctx, key, (int)key_len, EVP_sha1(), NULL);
    HMAC_Update(ctx, (unsigned char *)data, data_len);
    HMAC_Final(ctx, (unsigned char *)str.data(), &out_len);
    HMAC_CTX_reset(ctx);
    HMAC_CTX_free(ctx);
#else
    HMAC_CTX ctx;
    HMAC_CTX_init(&ctx);
    HMAC_Init_ex(&ctx, key, key_len, EVP_sha1(), NULL);
    HMAC_Update(&ctx, (unsigned char *)data, data_len);
    HMAC_Final(&ctx, (unsigned char *)str.data(), &out_len);
    HMAC_CTX_cleanup(&ctx);
#endif // defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10100000L)
    return str;
}

static std::string openssl_MD5(const void *data, size_t data_len) {
    std::string str;
    str.resize(16);
    unsigned int out_len;
#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10100000L)
    // openssl 1.1.0新增api，老版本api作废
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
    EVP_DigestUpdate(ctx, data, data_len);
    unsigned int md_len;
    EVP_DigestFinal_ex(ctx, (unsigned char *)str.data(), &md_len);
    EVP_MD_CTX_free(ctx);
#else
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, data, data_len);
    MD5_Final((unsigned char *)str.data(), &ctx);
#endif // defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10100000L)
    return str;
}

///////////////////////////////////////////////////
// StunAttribute

bool StunAttribute::isComprehensionRequired(const uint8_t *data, size_t len) {
    return ((data[0] & 0xC0) == 0x00);
}

const uint8_t * StunAttribute::loadHeader(const uint8_t *buf) {
    _type = (Type)Byte::Get2Bytes(buf, 0);
    _length = Byte::Get2Bytes(buf, 2);
    return buf + ATTR_HEADER_SIZE;
}

uint8_t * StunAttribute::storeHeader() {
    _data = toolkit::BufferRaw::create(ATTR_HEADER_SIZE + Byte::PadTo4Bytes(_length));
    _data->setSize(_data->getCapacity());
    memset(_data->data(), 0, _data->size());
    uint8_t *ptr = (uint8_t *)_data->data();
    Byte::Set2Bytes(ptr, 0, (uint16_t)_type);
    Byte::Set2Bytes(ptr, 2, _length);
    return ptr + ATTR_HEADER_SIZE;
}

bool StunAttrMappedAddress::loadFromData(const uint8_t *buf, size_t len) {
    StunAttribute::loadHeader(buf);
    return true;
}

bool StunAttrMappedAddress::storeToData() {
    return true;
}

bool StunAttrUserName::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _username.assign((const char *)p, _length);
    return true;
}

bool StunAttrUserName::storeToData() {
    _length = _username.length();
    auto p = StunAttribute::storeHeader();
    memcpy(p, _username.data(), _username.length());
    return true;
}

bool StunAttrMessageIntegrity::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _hmac.assign((const char *)p, _length);
    return true;
}

bool StunAttrMessageIntegrity::storeToData() {
    _length = _hmac.size();
    auto p = StunAttribute::storeHeader();
    memcpy(p, _hmac.data(), _hmac.size());
    return true;
}

bool StunAttrErrorCode::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _error_code = (Code)(p[2] * 100 + p[3]);
    return true;
}

bool StunAttrErrorCode::storeToData() {
    _length = 4;
    auto p = StunAttribute::storeHeader();
    Byte::Set2Bytes(p, 0, 0); // reserved
    uint16_t code = (uint16_t)_error_code;
    p[2] = code / 100;
    p[3] = code % 100;
    return true;
}

bool StunAttrChannelNumber::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _channel_number = Byte::Get2Bytes(p, 0);
    return true;
}

bool StunAttrChannelNumber::storeToData() {
    _length = 4;
    auto p = StunAttribute::storeHeader();
    Byte::Set2Bytes(p, 0, _channel_number);
    Byte::Set2Bytes(p, 2, 0); // RFFU
    return true;
}

bool StunAttrLifeTime::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _lifetime = Byte::Get4Bytes(p, 0);
    return true;
}

bool StunAttrLifeTime::storeToData() {
    _length = 4;
    auto p = StunAttribute::storeHeader();
    Byte::Set4Bytes(p, 0, _lifetime);
    return true;
}

bool StunAttrXorPeerAddress::loadFromData(const uint8_t *buf, size_t len) {
    auto attrValue = StunAttribute::loadHeader(buf);
    memset(&_addr, 0, sizeof(_addr));
    uint8_t port[2], addr[16];
    port[0] = attrValue[2] ^ StunPacket::_magicCookie[0];
    port[1] = attrValue[3] ^ StunPacket::_magicCookie[1];
    addr[0] = attrValue[4] ^ StunPacket::_magicCookie[0];
    addr[1] = attrValue[5] ^ StunPacket::_magicCookie[1];
    addr[2] = attrValue[6] ^ StunPacket::_magicCookie[2];
    addr[3] = attrValue[7] ^ StunPacket::_magicCookie[3];
    auto protocol = attrValue[1];
    if (protocol == 0x01) {
        _addr.ss_family = AF_INET;
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)&_addr;
        ipv4->sin_port = ntohs(Byte::Get2Bytes(port, 0));
        std::memcpy((void *)&(reinterpret_cast<const sockaddr_in *>(&_addr))->sin_addr.s_addr, addr, 4);
    } else {
        _addr.ss_family = AF_INET6;
        for (int i=0; i < 12; ++i) {
            addr[i + 4] = attrValue[i + 8] ^ _transaction_id[i];
        }
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&_addr;
        ipv6->sin6_port = ntohs(Byte::Get2Bytes(port, 0));
        std::memcpy((void *)&(reinterpret_cast<const sockaddr_in6 *>(&_addr))->sin6_addr.s6_addr, addr, 16);
    }

    return true;
}

bool StunAttrXorPeerAddress::storeToData() {
    _length = (_addr.ss_family == AF_INET) ? 8 : 20;
    auto attrValue = StunAttribute::storeHeader();
    // Set first byte to 0.
    attrValue[0] = 0;
    if (_addr.ss_family == AF_INET) {
        // Set inet family.
        attrValue[1] = 1;
        // Set port and XOR it.
        std::memcpy(attrValue + 2, &(reinterpret_cast<const sockaddr_in *>(&_addr))->sin_port, 2);
        attrValue[2] ^= StunPacket::_magicCookie[0];
        attrValue[3] ^= StunPacket::_magicCookie[1];

        // Set address and XOR it.
        std::memcpy(attrValue + 4, &(reinterpret_cast<const sockaddr_in *>(&_addr))->sin_addr.s_addr, 4);
        attrValue[4] ^= StunPacket::_magicCookie[0];
        attrValue[5] ^= StunPacket::_magicCookie[1];
        attrValue[6] ^= StunPacket::_magicCookie[2];
        attrValue[7] ^= StunPacket::_magicCookie[3];
    } else if (_addr.ss_family == AF_INET6) {
        // Set inet family.
        attrValue[1] = 2;

        std::memcpy(attrValue + 2, &(reinterpret_cast<const sockaddr_in6 *>(&_addr))->sin6_port, 2);
        attrValue[2] ^= StunPacket::_magicCookie[0];
        attrValue[3] ^= StunPacket::_magicCookie[1];
        // Set address and XOR it.
        std::memcpy(attrValue + 4, &(reinterpret_cast<const sockaddr_in6 *>(&_addr))->sin6_addr.s6_addr, 16);
        attrValue[4] ^= StunPacket::_magicCookie[0];
        attrValue[5] ^= StunPacket::_magicCookie[1];
        attrValue[6] ^= StunPacket::_magicCookie[2];
        attrValue[7] ^= StunPacket::_magicCookie[3];
        for (int i=0; i < 12; ++i) {
            attrValue[8 + i] ^= _transaction_id[i];
        }
    }

    return true;
}

bool StunAttrData::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    setData((const char *)p, _length);
    return true;
}

bool StunAttrData::storeToData() {
    _length = _data_content.size();
    auto p = StunAttribute::storeHeader();
    memcpy(p, _data_content.data(), _data_content.size());
    return true;
}

bool StunAttrRealm::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _realm.assign((const char *)p, _length);
    return true;
}

bool StunAttrRealm::storeToData() {
    _length = _realm.size();
    auto p = StunAttribute::storeHeader();
    memcpy(p, _realm.data(), _realm.size());
    return true;
}

bool StunAttrNonce::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _nonce.assign((const char *)p, _length);
    return true;
}

bool StunAttrNonce::storeToData() {
    _length = _nonce.size();
    auto p = StunAttribute::storeHeader();
    memcpy(p, _nonce.data(), _nonce.size());
    return true;
}

bool StunAttrRequestedTransport::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _protocol = (Protocol)p[0];
    return true;
}

bool StunAttrRequestedTransport::storeToData() {
    _length = 4;
    auto p = StunAttribute::storeHeader();
    p[0] = (uint8_t)_protocol;
    return true;
}

bool StunAttrPriority::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _priority = Byte::Get4Bytes(p, 0);
    return true;
}

bool StunAttrPriority::storeToData() {
    _length = 4;
    auto p = StunAttribute::storeHeader();
    Byte::Set4Bytes(p, 0, _priority);
    return true;
}

bool StunAttrUseCandidate::loadFromData(const uint8_t *buf, size_t len) {
    StunAttribute::loadHeader(buf);
    return true;
}

bool StunAttrUseCandidate::storeToData() {
    _length = 0;
    StunAttribute::storeHeader();
    return true;
}

bool StunAttrFingerprint::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _fingerprint = Byte::Get4Bytes(p, 0);
    return true;
}

bool StunAttrFingerprint::storeToData() {
    _length = 4;
    auto p = StunAttribute::storeHeader();
    Byte::Set4Bytes(p, 0, _fingerprint);
    return true;
}

bool StunAttrIceControlled::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _tiebreaker = Byte::Get8Bytes(p, 0);
    return true;
}

bool StunAttrIceControlled::storeToData() {
    _length = 8;
    auto p = StunAttribute::storeHeader();
    Byte::Set8Bytes(p, 0, _tiebreaker);
    return true;
}

bool StunAttrIceControlling::loadFromData(const uint8_t *buf, size_t len) {
    auto p = StunAttribute::loadHeader(buf);
    _tiebreaker = Byte::Get8Bytes(p, 0);
    return true;
}

bool StunAttrIceControlling::storeToData() {
    _length = 8;
    auto p = StunAttribute::storeHeader();
    Byte::Set8Bytes(p, 0, _tiebreaker);
    return true;
}

///////////////////////////////////////////////////
// StunPacket

const uint8_t StunPacket::_magicCookie[] = { 0x21, 0x12, 0xA4, 0x42 };

/* Class methods. */
bool StunPacket::isStun(const uint8_t *data, size_t len) {
    // reference https://www.rfc-editor.org/rfc/rfc8489.html#section-6.3
    return
        // STUN headers are 20 bytes.
        (len >= 20) &&
        // checks that the first two bits are 0
        ((data[0] & 0xC0) == 0) &&
        // that the Magic Cookie field has the correct value
        (data[4] == StunPacket::_magicCookie[0]) && (data[5] == StunPacket::_magicCookie[1]) && (data[6] == StunPacket::_magicCookie[2])
        && (data[7] == StunPacket::_magicCookie[3]);
}

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
StunPacket::Class StunPacket::getClass(const uint8_t *data, size_t len) {
    return StunPacket::Class(((data[0] & 0x01) << 1) | ((data[1] & 0x10) >> 4));
}

StunPacket::Method StunPacket::getMethod(const uint8_t *data, size_t len) {
    uint16_t msgType = Byte::Get2Bytes(data, 0);
    return StunPacket::Method((msgType & 0x000F) | ((msgType & 0x00E0) >> 1) | ((msgType & 0x3E00) >> 2));
}

StunPacket::Ptr StunPacket::parse(const uint8_t *data, size_t len) {
    // TraceL;

    if (!StunPacket::isStun(data, len)) {
        return nullptr;
    }

    // Get length field.
    uint16_t msgLength = Byte::Get2Bytes(data, 2);

    // length field must be total size minus header's 20 bytes, and must be multiple of 4 Bytes.
    if ((static_cast<size_t>(msgLength) != len - 20) || ((msgLength & 0x03) != 0)) {
        WarnL << "length field + 20 does not match total size (or it is not multiple of 4 bytes), packet discarded";
        return nullptr;
    }

    auto msgMethod = getMethod(data, len);
    auto msgClass = getClass(data, len);

    auto packet = std::make_shared<StunPacket>(msgClass, msgMethod, (const char *)data + 8);
    packet->loadFromData(data, len);
    return packet;
}

std::string StunPacket::mappingClassEnum2Str(Class klass) {
    switch (klass) {
        case StunPacket::Class::REQUEST: return "REQUEST";
        case StunPacket::Class::INDICATION: return "INDICATION";
        case StunPacket::Class::SUCCESS_RESPONSE: return "SUCCESS_RESPONSE";
        case StunPacket::Class::ERROR_RESPONSE: return "ERROR_RESPONSE";
        default: break;
    }
    return "invalid";
}

std::string StunPacket::mappingMethodEnum2Str(Method method) {
    switch (method) {
        case StunPacket::Method::BINDING: return "BINDING";
        case StunPacket::Method::ALLOCATE: return "ALLOCATE";
        case StunPacket::Method::REFRESH: return "REFRESH";
        case StunPacket::Method::SEND: return "SEND";
        case StunPacket::Method::DATA: return "DATA";
        case StunPacket::Method::CREATEPERMISSION: return "CREATEPERMISSION";
        case StunPacket::Method::CHANNELBIND: return "CHANNELBIND";
        default: break;
    }
    return "invalid";
}

StunPacket::StunPacket(Class klass, Method method, const char* transId)
    : _klass(klass)
    , _method(method) {
    // TraceL;
    if (transId) {
        _transaction_id.assign(transId, 12);
    } else {
        refreshTransactionId();
    }
}

StunPacket::~StunPacket() {
    // TraceL;
}

std::string StunPacket::dumpString(bool transId) const {
    std::string ret = "class=" + getClassStr() + ", method=" + getMethodStr();
    if (transId) {
        ret += ", transaction=" + hexdump(_transaction_id.data(), _transaction_id.size());
    }
    return ret;
}

void StunPacket::addAttribute(StunAttribute::Ptr attr) {
    _attribute_map.emplace(attr->type(), std::move(attr));
}

void StunPacket::removeAttribute(StunAttribute::Type type) {
    _attribute_map.erase(type);
}

bool StunPacket::hasAttribute(StunAttribute::Type type) const {
    return _attribute_map.count(type) > 0;
}

StunAttribute::Ptr StunPacket::getAttribute(StunAttribute::Type type) const {
    auto it = _attribute_map.find(type);
    if (it != _attribute_map.end()) {
        return it->second;
    }
    return nullptr;
}

std::string StunPacket::getUsername() const {
    auto attr = getAttribute<StunAttrUserName>();
    return attr ? attr->getUsername() : "";
}

uint64_t StunPacket::getPriority() const {
    auto attr = getAttribute<StunAttrPriority>();
    return attr ? attr->getPriority() : 0;
}

StunAttrErrorCode::Code StunPacket::getErrorCode() const {
    auto attr = getAttribute<StunAttrErrorCode>();
    return attr ? attr->getErrorCode() : StunAttrErrorCode::Code::Invalid;
}

StunPacket::Authentication StunPacket::checkAuthentication(const std::string &ufrag, const std::string &password) const {
    // TraceL;
    auto attr_message_integrity = getAttribute<StunAttrMessageIntegrity>();
    switch (_klass) {
        case Class::REQUEST: {
            if (!attr_message_integrity) {
                return Authentication::UNAUTHORIZED;
            }

            if (getMethod() == Method::ALLOCATE || getMethod() == Method::REFRESH || 
                getMethod() == Method::CREATEPERMISSION || getMethod() == Method::CHANNELBIND) {
                // TURN认证：USERNAME应该等于ufrag
                std::string username = getUsername();
                if (username != ufrag) {
                    TraceL << "TURN USERNAME validation failed, expected: " << ufrag << ", got: " << username;
                    return Authentication::UNAUTHORIZED;
                }
            } else {
                // ICE认证：USERNAME格式为 local-ufrag:remote-ufrag（仅用于ICE BINDING请求）
                std::string username = getUsername();
                if (!username.empty()) {
                    size_t localUsernameLen = ufrag.length();
                    if (username.length() <= localUsernameLen || username.at(localUsernameLen) != ':' ||
                        (username.compare(0, localUsernameLen, ufrag) != 0)) {
                        DebugL << "ICE USERNAME format validation failed, expected format: " << ufrag << ":remote-ufrag, got: " << username;
                        return Authentication::UNAUTHORIZED;
                    }
                }
            }
            break;
        }
        // This method cannot check authentication in received responses (as we
        // are ICE-Lite and don't generate requests).
        case Class::INDICATION: return Authentication::OK;
        case Class::SUCCESS_RESPONSE:
        case Class::ERROR_RESPONSE: break;
    }

    if (attr_message_integrity) {
        // If there is FINGERPRINT it must be discarded for MESSAGE-INTEGRITY calculation,
        // so the header length field must be modified (and later restored).
        if (hasAttribute(StunAttribute::Type::FINGERPRINT)) {
            // Set the header length field: full size - header length (20) - FINGERPRINT length (8).
            Byte::Set2Bytes((uint8_t *)_data->data(), 2, _data->size() - HEADER_SIZE - 8);
        }

        auto attr_realm = getAttribute<StunAttrRealm>();
        auto attr_nonce = getAttribute<StunAttrNonce>();

        std::string key = password;
        if (attr_nonce && attr_realm) {
            // 使用长期凭证机制
            // 根据RFC 5389/5766标准：key = MD5(username ":" realm ":" password)
            auto realm = attr_realm->getRealm();
            std::string input = ufrag + ":" + std::string(realm.data(), realm.size()) + ":" + password;
            key = openssl_MD5(input.data(), input.size());

            // DebugL << "ufrag: " << ufrag;
            // DebugL << "realm: " << realm.data();
            // DebugL << "password: " << password;
            // DebugL << "input: " << input;
        }

        auto computedMessageIntegrity = openssl_HMACsha1(key.data(), key.size(), _data->data(), _message_integrity_data_len);

        // DebugL << "cal MessageIntegrity";
        // DebugL << "password: " << password;
        // DebugL << "key: " << toolkit::hexdump(key.data(), key.size());
        // DebugL << "data: " << toolkit::hexdump(_data->data(), _message_integrity_data_len);
        // DebugL << "_message_integrity_data_len: " << _message_integrity_data_len;
        // DebugL << "_hmac: " << toolkit::hexdump(attr_message_integrity->_hmac.data(), attr_message_integrity->_hmac.size());
        // DebugL << "cal: " << toolkit::hexdump(computedMessageIntegrity.data(), computedMessageIntegrity.size());

        if (attr_message_integrity->getHmac() != computedMessageIntegrity) {
            return Authentication::UNAUTHORIZED;
        }

        if (hasAttribute(StunAttribute::Type::FINGERPRINT)) {
            Byte::Set2Bytes((uint8_t*)_data->data(), 2, _data->size() - HEADER_SIZE);
        }
    }

    // FINGERPRINT验证
    if (hasAttribute(StunAttribute::Type::FINGERPRINT)) {
        auto attr_fingerprint = getAttribute<StunAttrFingerprint>();
        if (attr_fingerprint) {
            // 计算FINGERPRINT：对除FINGERPRINT属性外的整个包计算CRC32
            uint32_t computedFingerprint = getCRC32((uint8_t*)_data->data(), _data->size() - 8) ^ 0x5354554e;
            if (attr_fingerprint->getFingerprint() != computedFingerprint) {
                // DebugL << "FINGERPRINT verification failed, expected: " << std::hex << computedFingerprint 
                //        << ", got: " << attr_fingerprint->getFingerprint();
                return Authentication::UNAUTHORIZED;
            } else {
                // TraceL << "FINGERPRINT verification passed";
            }
        }
    }

    return Authentication::OK;
}

void StunPacket::serialize() {
    //TraceL;

    _data = BufferRaw::create();
    for (auto it : _attribute_map) {
        it.second->storeToData();
    }

    auto attr_size = getAttrSize();

    if (getClass() == StunPacket::Class::ERROR_RESPONSE) {
        setNeedFingerprint(false);
        setNeedMessageIntegrity(false);
    }

    if (getClass() == StunPacket::Class::INDICATION) {
        setNeedMessageIntegrity(false);
    }

    auto message_integrity_size = getNeedMessageIntegrity() ? 24 : 0;
    auto fingerprint_size = getNeedFingerprint() ? 8 : 0;

    auto packet_size = HEADER_SIZE + attr_size + message_integrity_size + fingerprint_size;
    _data->setCapacity(packet_size);
    _data->setSize(packet_size);

    // Merge class and method fields into type.
    uint16_t typeField = (static_cast<uint16_t>(_method) & 0x0f80) << 2;

    typeField |= (static_cast<uint16_t>(_method) & 0x0070) << 1;
    typeField |= (static_cast<uint16_t>(_method) & 0x000f);
    typeField |= (static_cast<uint16_t>(_klass) & 0x02) << 7;
    typeField |= (static_cast<uint16_t>(_klass) & 0x01) << 4;

    // Set type field.
    Byte::Set2Bytes((unsigned char *)_data->data(), 0, typeField);
    uint16_t initial_length = static_cast<uint16_t>(attr_size + message_integrity_size);
    Byte::Set2Bytes((unsigned char *)_data->data(), 2, initial_length);
    // Set magic cookie.
    std::memcpy(_data->data() + 4, StunPacket::_magicCookie, 4);
    // Set TransactionId field.
    std::memcpy(_data->data() + 8, _transaction_id.data(), 12);

    storeAttrMessage();
    if (message_integrity_size) {
        auto ufrag = _peer_ufrag;
        auto password = _peer_password;
        if (getClass() == StunPacket::Class::SUCCESS_RESPONSE || 
            getClass() == StunPacket::Class::ERROR_RESPONSE) {
            ufrag = _ufrag;
            password = _password;
        }

        // Add MESSAGE-INTEGRITY.
        auto attr_nonce = getAttribute<StunAttrNonce>();
        auto attr_realm = getAttribute<StunAttrRealm>();
        // FIXME: need use SASLprep(password) replace password
        //  根据RFC 5766标准：key = MD5(username ":" realm ":" SASLprep(password))
        std::string key = password;
        if (attr_nonce && attr_realm) {
            // 使用长期凭证机制
            // key = MD5(username ":" realm ":" password)
            auto realm = attr_realm->getRealm();
            std::string username = ufrag; // 对于response消息，使用ufrag作为username
            std::string input = username + ":" + std::string(realm.data(), realm.size()) + ":" + password;
            key = openssl_MD5(input.data(), input.size());

            // DebugL << "Long-term credential used for response:";
            // DebugL << "ufrag: " << ufrag;
            // DebugL << "realm: " << std::string(realm.data(), realm.size());
            // DebugL << "password: " << password;
            // DebugL << "input: " << input;
            // DebugL << "MD5 key: " << toolkit::hexdump(key.data(), key.size());
        }

        size_t mi_calc_len = HEADER_SIZE + attr_size;
        auto computedMessageIntegrity = openssl_HMACsha1(key.data(), key.size(), _data->data(), mi_calc_len);
        auto attr_message_integrity = std::make_shared<StunAttrMessageIntegrity>();
        attr_message_integrity->setHmac(computedMessageIntegrity);
        attr_message_integrity->storeToData();
        memcpy((unsigned char *)_data->data() + HEADER_SIZE + attr_size, attr_message_integrity->data(), attr_message_integrity->size());

        // DebugL << "Serialize MESSAGE-INTEGRITY:";
        // DebugL << "password: \"" << password << "\"";
        // DebugL << "key: " << toolkit::hexdump(key.data(), key.size());
        // DebugL << "hmac_calculated: " << toolkit::hexdump(computedMessageIntegrity.data(), computedMessageIntegrity.size());
    }

    if (fingerprint_size) {
        // Add FINGERPRINT.
        // Compute the CRC32 of the packet up to (but excluding) the FINGERPRINT
        uint16_t final_length = static_cast<uint16_t>(attr_size + message_integrity_size + fingerprint_size);
        Byte::Set2Bytes((unsigned char *)_data->data(), 2, final_length);
        size_t fp_calc_len = HEADER_SIZE + attr_size + message_integrity_size;
        uint32_t computedFingerprint = getCRC32((unsigned char *)_data->data(), fp_calc_len) ^ 0x5354554e;

        auto attr_fingerprint = std::make_shared<StunAttrFingerprint>();
        attr_fingerprint->setFingerprint(computedFingerprint);
        attr_fingerprint->storeToData();
        memcpy((unsigned char *)_data->data() + HEADER_SIZE + attr_size + message_integrity_size, attr_fingerprint->data(), attr_fingerprint->size());
    }
}

StunPacket::Ptr StunPacket::createSuccessResponse() const {
    // TraceL;
    CHECK(_klass == Class::REQUEST, "attempt to create a success response for a non Request STUN packet");

    auto packet = std::make_shared<StunPacket>(Class::SUCCESS_RESPONSE, _method, _transaction_id.c_str());

    // 复制认证相关属性到响应包中，用于MESSAGE-INTEGRITY计算
    auto attr_realm = getAttribute(StunAttribute::Type::REALM);
    if (attr_realm) {
        packet->addAttribute(attr_realm);
    }

    auto attr_nonce = getAttribute(StunAttribute::Type::NONCE);
    if (attr_nonce) {
        packet->addAttribute(attr_nonce);
        DebugL << "Copied NONCE attribute to response";
    }

    return packet;
}

StunPacket::Ptr StunPacket::createErrorResponse(StunAttrErrorCode::Code errorCode) const {
    TraceL;
    CHECK(_klass == Class::REQUEST, "attempt to create an error response for a non Request STUN packet");
    auto ret = std::make_shared<StunPacket>(Class::ERROR_RESPONSE, _method, _transaction_id.c_str());
    auto attr = std::make_shared<StunAttrErrorCode>();
    attr->setErrorCode(errorCode);
    ret->addAttribute(std::move(attr));
    return ret;
}

char *StunPacket::data() const {
    return _data ? _data->data() : nullptr;
}

size_t StunPacket::size() const {
    return _data ? _data->size() : 0;
}

bool StunPacket::loadFromData(const uint8_t *buf, size_t len) {
    if (HEADER_SIZE > len) {
        WarnL << "size too small " << len;
        return false;
    }

    _data = BufferRaw::create();
    _data->assign((const char *)(buf), len);

    _transaction_id.assign((const char *)buf + 8, 12);

    if (len == HEADER_SIZE) {
        return true;
    }

    return loadAttrMessage(buf + HEADER_SIZE, len - HEADER_SIZE);
}

bool StunPacket::loadAttrMessage(const uint8_t *buf, size_t len) {
    _attribute_map.clear();
    _message_integrity_data_len = HEADER_SIZE + len;

    uint8_t *ptr = const_cast<uint8_t*>(buf);
    StunAttribute::Ptr attr = nullptr;
    while (ptr < buf + len) {
        auto type = (StunAttribute::Type)Byte::Get2Bytes(ptr, 0);
        size_t length = Byte::Get2Bytes(ptr, 2);
        size_t lengthAlign = Byte::PadTo4Bytes((uint16_t)length);

        switch (type) {
            case StunAttribute::Type::MAPPED_ADDRESS: attr = std::make_shared<StunAttrMappedAddress>(); break;
            case StunAttribute::Type::USERNAME: attr = std::make_shared<StunAttrUserName>(); break;
            case StunAttribute::Type::MESSAGE_INTEGRITY:
                attr = std::make_shared<StunAttrMessageIntegrity>();
                _message_integrity_data_len = HEADER_SIZE + ptr - buf;
                break;
            case StunAttribute::Type::ERROR_CODE: attr = std::make_shared<StunAttrErrorCode>(); break;
            case StunAttribute::Type::CHANNEL_NUMBER: attr = std::make_shared<StunAttrChannelNumber>(); break;
            case StunAttribute::Type::LIFETIME: attr = std::make_shared<StunAttrLifeTime>(); break;
            case StunAttribute::Type::DATA: attr = std::make_shared<StunAttrData>(); break;
            case StunAttribute::Type::REALM: attr = std::make_shared<StunAttrRealm>(); break;
            case StunAttribute::Type::NONCE: attr = std::make_shared<StunAttrNonce>(); break;
            case StunAttribute::Type::REQUESTED_TRANSPORT: attr = std::make_shared<StunAttrRequestedTransport>(); break;
            case StunAttribute::Type::XOR_PEER_ADDRESS: attr = std::make_shared<StunAttrXorPeerAddress>(_transaction_id); break;
            case StunAttribute::Type::XOR_RELAYED_ADDRESS: attr = std::make_shared<StunAttrXorRelayedAddress>(_transaction_id); break;
            case StunAttribute::Type::XOR_MAPPED_ADDRESS: attr = std::make_shared<StunAttrXorMappedAddress>(_transaction_id); break;

            case StunAttribute::Type::PRIORITY: attr = std::make_shared<StunAttrPriority>(); break;
            case StunAttribute::Type::USE_CANDIDATE: attr = std::make_shared<StunAttrUseCandidate>(); break;
            case StunAttribute::Type::FINGERPRINT: attr = std::make_shared<StunAttrFingerprint>(); break;
            case StunAttribute::Type::ICE_CONTROLLED: attr = std::make_shared<StunAttrIceControlled>(); break;
            case StunAttribute::Type::ICE_CONTROLLING: attr = std::make_shared<StunAttrIceControlling>(); break;
            case StunAttribute::Type::GOOG_NETWORK_INFO:
            case StunAttribute::Type::SOFTWARE:
                break;
            default: WarnL << "not support Attribute " << (uint16_t)type << "," << toolkit::hexdump(ptr, 2); break;
        }

        if (attr) {
            if (ptr + lengthAlign + 4 > buf + len) {
                WarnL << "the attribute length exceeds the remaining size, packet discarded";
                return false;
            }

            if (attr->loadFromData(ptr, StunAttribute::ATTR_HEADER_SIZE + length)) {
                _attribute_map.emplace(type, std::move(attr));

            } else {
                if (StunAttribute::isComprehensionRequired(ptr, 4)) {
                    WarnL << "parse a Comprehension Required Stun Attribute failed,  type=" << (uint16_t)type << " len=" << length;
                    return false;
                }
                WarnL << "parse Stun Attribute failed type=" << (uint16_t)type << " len=" << length;
            }
            attr = nullptr;
        }

        ptr += lengthAlign + StunAttribute::ATTR_HEADER_SIZE;
    }
    return true;
}

bool StunPacket::storeAttrMessage() {
    uint8_t *buf = (uint8_t *)_data->data() + HEADER_SIZE;
    for (auto &pr : _attribute_map) {
        memcpy(buf, pr.second->data(), pr.second->size());
        buf += pr.second->size();
    }
    return true;
}

size_t StunPacket::getAttrSize() const {
    size_t size = 0;
    for (auto &pr : _attribute_map) {
        size += pr.second->size();
    }
    return size;
}

SuccessResponsePacket::SuccessResponsePacket(Method method, const std::string& transaction_id) :
    StunPacket(Class::SUCCESS_RESPONSE, method, transaction_id.c_str()) {
}

ErrorResponsePacket::ErrorResponsePacket(Method method, const std::string& transaction_id, StunAttrErrorCode::Code error_code) :
    StunPacket(Class::ERROR_RESPONSE, method, transaction_id.c_str()) {
    DebugL;
    auto attr = std::make_shared<StunAttrErrorCode>();
    attr->setErrorCode(error_code);
    addAttribute(std::move(attr));
}

} // namespace RTC
