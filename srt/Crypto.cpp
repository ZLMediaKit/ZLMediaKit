#include <atomic>
#include "Util/MD5.h"
#include "Util/logger.h"

#include "Crypto.hpp"

#if defined(ENABLE_OPENSSL)
#include "openssl/evp.h"
#endif

using namespace toolkit;
using namespace std;
using namespace SRT;

namespace SRT {

#if defined(ENABLE_OPENSSL)
inline const EVP_CIPHER* aes_key_len_mapping_wrap_cipher(int key_len) {
    switch (key_len) {
        case 192/8: return EVP_aes_192_wrap();
        case 256/8: return EVP_aes_256_wrap();
        case 128/8:
        default:
            return EVP_aes_128_wrap();
    }
}

inline const EVP_CIPHER* aes_key_len_mapping_ctr_cipher(int key_len) {
    switch (key_len) {
        case 192/8: return EVP_aes_192_ctr();
        case 256/8: return EVP_aes_256_ctr();
        case 128/8:
        default:
            return EVP_aes_128_ctr();
    }
}
#endif

/**
 * @brief: aes_wrap 
 * @param [in]: in 待warp的数据
 * @param [in]: in_len 待warp的数据长度
 * @param [out]: out warp后输出的数据
 * @param [out]: outLen 加密后输出的数据长度
 * @param [in]: key 密钥
 * @param [in]: key_len 密钥长度
 * @return : true: 成功，false: 失败
**/
static bool aes_wrap(const uint8_t* in, int in_len, uint8_t* out, int* outLen, uint8_t* key, int key_len) {

#if defined(ENABLE_OPENSSL)
	EVP_CIPHER_CTX* ctx = NULL;

   *outLen = 0;

	do {
        if (!(ctx = EVP_CIPHER_CTX_new())) {
            WarnL << "EVP_CIPHER_CTX_new fail";
            break;
        }
        EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

        if (1 != EVP_EncryptInit_ex(ctx, aes_key_len_mapping_wrap_cipher(key_len), NULL, key, NULL)) {
            WarnL << "EVP_EncryptInit_ex fail";
			break;
		}

		int len1 = 0;
		if (1 != EVP_EncryptUpdate(ctx, (uint8_t*)out, &len1, (uint8_t*)in, in_len)) {
            WarnL << "EVP_EncryptUpdate fail";
			break;
		}

		int len2 = 0;
		if (1 != EVP_EncryptFinal_ex(ctx, (uint8_t*)out + len1, &len2)) {
            WarnL << "EVP_EncryptFinal_ex fail";
			break;
		}

		*outLen = len1 + len2;
	} while (0);

	if (ctx != NULL) {
		EVP_CIPHER_CTX_free(ctx);
	}

	return *outLen != 0;
#else
    return false;
#endif
}

/**
 * @brief: aes_unwrap 
 * @param [in]: in 待unwrap的数据
 * @param [in]: in_len 待unwrap的数据长度
 * @param [out]: out unwrap后输出的数据
 * @param [out]: outLen unwrap后输出的数据长度
 * @param [in]: key 密钥
 * @param [in]: key_len 密钥长度
 * @return : true: 成功，false: 失败
**/
static bool aes_unwrap(const uint8_t* in, int in_len, uint8_t* out, int* outLen, uint8_t* key, int key_len) {

#if defined(ENABLE_OPENSSL)
	EVP_CIPHER_CTX* ctx = NULL;

    *outLen = 0;

	do {

        if (!(ctx = EVP_CIPHER_CTX_new())) {
            WarnL << "EVP_CIPHER_CTX_new fail";
            break;
        }
        EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

        if (1 != EVP_DecryptInit_ex(ctx, aes_key_len_mapping_wrap_cipher(key_len), NULL, key, NULL)) {
            WarnL << "EVP_DecryptInit_ex fail";
			break;
		}

        //设置pkcs7padding
        if (1 != EVP_CIPHER_CTX_set_padding(ctx, 1)) {
            WarnL << "EVP_CIPHER_CTX_set_padding fail";
            break;
        }

		int len1 = 0;
		if (1 != EVP_DecryptUpdate(ctx, (uint8_t*)out, &len1, (uint8_t*)in, in_len)) {
            WarnL << "EVP_DecryptUpdate fail";
			break;
		}

		int len2 = 0;
		if (1 != EVP_DecryptFinal_ex(ctx, (uint8_t*)out + len1, &len2)) {
            WarnL << "EVP_DecryptFinal_ex fail";
			break;
		}

		*outLen = len1 + len2;
	} while (0);

	if (ctx != NULL) {
		EVP_CIPHER_CTX_free(ctx);
	}

	return *outLen != 0;

#else
    return false;
#endif
}

/**
 * @brief: aes ctr 加密
 * @param [in]: in 待加密的数据
 * @param [in]: in_len 待加密的数据长度
 * @param [out]: out 加密后输出的数据
 * @param [out]: outLen 加密后输出的数据长度
 * @param [in]: key 密钥
 * @param [in]: key_len 密钥长度
 * @param [in]: iv iv向量(16byte)
 * @return : true: 成功，false: 失败
**/
static bool aes_ctr_encrypt(const uint8_t* in, int in_len, uint8_t* out, int* outLen, uint8_t* key, int key_len, uint8_t* iv) {

#if defined(ENABLE_OPENSSL)
	EVP_CIPHER_CTX* ctx = NULL;

   *outLen = 0;

	do {
        if (!(ctx = EVP_CIPHER_CTX_new())) {
            WarnL << "EVP_CIPHER_CTX_new fail";
            break;
        }

        if (1 != EVP_EncryptInit_ex(ctx, aes_key_len_mapping_ctr_cipher(key_len), NULL, key, iv)) {
            WarnL << "EVP_EncryptInit_ex fail";
			break;
		}

		int len1 = 0;
		if (1 != EVP_EncryptUpdate(ctx, (uint8_t*)out, &len1, (uint8_t*)in, in_len)) {
            WarnL << "EVP_EncryptUpdate fail";
			break;
		}

		int len2 = 0;
		if (1 != EVP_EncryptFinal_ex(ctx, (uint8_t*)out + len1, &len2)) {
            WarnL << "EVP_EncryptFinal_ex fail";
			break;
		}

		*outLen = len1 + len2;
	} while (0);

	if (ctx != NULL) {
		EVP_CIPHER_CTX_free(ctx);
	}

	return *outLen != 0;
#else
    return false;
#endif
}


/**
 * @brief: aes ctr 解密
 * @param [in]: in 待解密的数据
 * @param [in]: in_len 待解密的数据长度
 * @param [out]: out 解密后输出的数据
 * @param [out]: outLen 解密后输出的数据长度
 * @param [in]: key 密钥
 * @param [in]: key_len 密钥长度
 * @param [in]: iv iv向量(16byte)
 * @return : true: 成功，false: 失败
**/
static bool aes_ctr_decrypt(const uint8_t* in, int in_len, uint8_t* out, int* outLen, uint8_t* key, int key_len, uint8_t* iv) {

#if defined(ENABLE_OPENSSL)
	EVP_CIPHER_CTX* ctx = NULL;

    *outLen = 0;

	do {

        if (!(ctx = EVP_CIPHER_CTX_new())) {
            WarnL << "EVP_CIPHER_CTX_new fail";
            break;
        }

		if (1 != EVP_DecryptInit_ex(ctx, aes_key_len_mapping_ctr_cipher(key_len), NULL, key, iv)) {
            WarnL << "EVP_DecryptInit_ex fail";
			break;
		}

		int len1 = 0;
		if (1 != EVP_DecryptUpdate(ctx, (uint8_t*)out, &len1, (uint8_t*)in, in_len)) {
            WarnL << "EVP_DecryptUpdate fail";
			break;
		}

		int len2 = 0;
		if (1 != EVP_DecryptFinal_ex(ctx, (uint8_t*)out + len1, &len2)) {
            WarnL << "EVP_DecryptFinal_ex fail";
			break;
		}

		*outLen = len1 + len2;
	} while (0);

	if (ctx != NULL) {
		EVP_CIPHER_CTX_free(ctx);
	}

	return *outLen != 0;

#else
    return false;
#endif
}


///////////////////////////////////////////////////
// CryptoContext
CryptoContext::CryptoContext(const std::string& passparase, uint8_t kk, KeyMaterial::Ptr packet) :
    _passparase(passparase), _kk(kk) {
    if (packet) {
        loadFromKeyMaterial(packet);
    } else {
        refresh();
    }
}

void CryptoContext::refresh() {
    if (_salt.empty()) {
        _salt = makeRandStr(_slen, false);
        generateKEK();
    }

    _sek = makeRandStr(_klen, false);
    return;
}

std::string CryptoContext::generateWarppedKey() {
    string warpped_key;
    int size = (_sek.size() + 15) /16 * 16 + 8;
    warpped_key.resize(size);
    auto res = aes_wrap((uint8_t*)_sek.data(), _sek.size(), (uint8_t*)warpped_key.data(), &size, (uint8_t*)_kek.data(), _kek.size());
    if (!res) {
        return "";
    }
    warpped_key.resize(size);
    return warpped_key;
}

void CryptoContext::loadFromKeyMaterial(KeyMaterial::Ptr packet) {

    _slen = packet->_slen;
    _klen = packet->_klen;
    _salt = packet->_salt;

    generateKEK();

    auto warpped_key = packet->_warpped_key;
    BufferLikeString sek;
    int size = warpped_key.size();
    sek.resize(size);
    auto ret = aes_unwrap((uint8_t*)warpped_key.data(), warpped_key.size(), (uint8_t*)sek.data(), &size, (uint8_t*)_kek.data(), _kek.size());
    if (!ret) {
        throw std::runtime_error(StrPrinter <<"warpped_key unwrap fail, password may mismatch");
    }

    sek.resize(size);
    if (packet->_kk == KeyMaterial::KEY_BASED_ENCRYPTION_BOTH_SEK) {
        if (_kk == KeyMaterial::KEY_BASED_ENCRYPTION_EVEN_SEK) {
            _sek = sek.substr(0, _slen);
        } else {
            _sek = sek.substr(_slen, _slen);
        }
    } else {
        _sek = sek;
    }
    return;
}

bool CryptoContext::generateKEK() {
    /**
        SEK  = PRNG(KLen)
        Salt = PRNG(128)
        KEK  = PBKDF2(passphrase, LSB(64,Salt), Iter, KLen)
    **/
    _kek.resize(_klen);
#if defined(ENABLE_OPENSSL)
    if (PKCS5_PBKDF2_HMAC(_passparase.data(), _passparase.length(), (uint8_t*)_salt.data() + _slen - 64/8, 64 /8, _iter, EVP_sha1(), _klen, (uint8_t*)_kek.data()) != 1) {
        return false;
    }
    return true;
#else
    return false;
#endif
}

BufferLikeString::Ptr CryptoContext::generateIv(uint32_t pkt_seq_no) {
    auto iv = std::make_shared<BufferLikeString>();
    iv->resize(128 /8);

    uint8_t* saltData = (uint8_t*)_salt.data();
    uint8_t* ivData = (uint8_t*)iv->data();
    memset((void*)ivData, 0, iv->size());
    memcpy((void*)(ivData + 10), (void*)&pkt_seq_no, 4);
    for (size_t i = 0; i < std::min<size_t>(_salt.size(), (size_t)112 /8); ++i) {
        ivData[i] ^= saltData[i];
    }
    return iv;
}

///////////////////////////////////////////////////
// AesCtrCryptoContext

AesCtrCryptoContext::AesCtrCryptoContext(const std::string& passparase, uint8_t kk, KeyMaterial::Ptr packet) :
    CryptoContext(passparase, kk, packet) {
}

BufferLikeString::Ptr AesCtrCryptoContext::encrypt(uint32_t pkt_seq_no, const char *buf, int len) {
    auto iv = generateIv(htonl(pkt_seq_no));
    auto payload = std::make_shared<BufferLikeString>();
    int size = (len + 15) /16 * 16 + 8;
    payload->resize(size);
    auto ret = aes_ctr_encrypt((const uint8_t*)buf, len, (uint8_t*)payload->data(), &size, (uint8_t*)_sek.data(), _sek.size(), (uint8_t*)iv->data());
    if (!ret) {
        return nullptr;
    }
    payload->resize(size);
    return payload;
}

BufferLikeString::Ptr AesCtrCryptoContext::decrypt(uint32_t pkt_seq_no, const char *buf, int len) {
    auto iv = generateIv(htonl(pkt_seq_no));
    auto payload = std::make_shared<BufferLikeString>();
    int size = len;
    payload->resize(size);
    auto ret = aes_ctr_decrypt((const uint8_t*)buf, len, (uint8_t*)payload->data(), &size, (uint8_t*)_sek.data(), _sek.size(), (uint8_t*)iv->data());
    if (!ret) {
        return nullptr;
    }
    payload->resize(size);
    return payload;
}

///////////////////////////////////////////////////
// Crypto

Crypto::Crypto(const std::string& passparase) :
    _passparase(passparase) {

#ifndef ENABLE_OPENSSL
    throw std::invalid_argument("openssl disable, please set ENABLE_OPENSSL when compile");
#endif

    _ctx_pair[0] = createCtx(KeyMaterial::CIPHER_AES_CTR, _passparase, KeyMaterial::KEY_BASED_ENCRYPTION_EVEN_SEK);
    _ctx_pair[1] = createCtx(KeyMaterial::CIPHER_AES_CTR, _passparase, KeyMaterial::KEY_BASED_ENCRYPTION_ODD_SEK);
    _ctx_idx = 0;
}

CryptoContext::Ptr Crypto::createCtx(int cipher, const std::string& passparase, uint8_t kk, KeyMaterial::Ptr packet) {
    switch (cipher){
        case KeyMaterial::CIPHER_AES_CTR:
            return std::make_shared<AesCtrCryptoContext>(passparase, kk, packet);
        case KeyMaterial::CIPHER_AES_ECB:
        case KeyMaterial::CIPHER_AES_CBC:
        case KeyMaterial::CIPHER_AES_GCM:
        default: 
            throw std::runtime_error(StrPrinter <<"not support cipher " << cipher);
    }
}

HSExtKeyMaterial::Ptr Crypto::generateKeyMaterialExt(uint16_t extension_type) {
    HSExtKeyMaterial::Ptr ext = std::make_shared<HSExtKeyMaterial>();
    ext->extension_type = extension_type;
    ext->_kk            = _ctx_pair[_ctx_idx]->_kk;
    ext->_cipher        = _ctx_pair[_ctx_idx]->getCipher();
    ext->_slen          = _ctx_pair[_ctx_idx]->_slen;
    ext->_klen          = _ctx_pair[_ctx_idx]->_klen;
    ext->_salt          = _ctx_pair[_ctx_idx]->_salt;
    ext->_warpped_key   = _ctx_pair[_ctx_idx]->generateWarppedKey();
    return ext;
}

KeyMaterialPacket::Ptr Crypto::generateAnnouncePacket(CryptoContext::Ptr ctx) {
    KeyMaterialPacket::Ptr pkt = std::make_shared<KeyMaterialPacket>();
    pkt->sub_type     = HSExt::SRT_CMD_KMREQ;
    pkt->_kk          = ctx->_kk;
    pkt->_cipher      = ctx->getCipher();
    pkt->_slen        = ctx->_slen;
    pkt->_klen        = ctx->_klen;
    pkt->_salt        = ctx->_salt;
    pkt->_warpped_key = ctx->generateWarppedKey();
    return pkt;
}

KeyMaterialPacket::Ptr Crypto::takeAwayAnnouncePacket() {
    auto pkt = _re_announce_pkt;
    _re_announce_pkt = nullptr;
    return pkt;
}

bool Crypto::loadFromKeyMaterial(KeyMaterial::Ptr packet) {
    try {
        if (packet->_kk == KeyMaterial::KEY_BASED_ENCRYPTION_EVEN_SEK) {
            _ctx_pair[0] = createCtx(packet->_cipher, _passparase, packet->_kk, packet);
        } else if (packet->_kk == KeyMaterial::KEY_BASED_ENCRYPTION_ODD_SEK) {
            _ctx_pair[1] = createCtx(packet->_cipher, _passparase, packet->_kk, packet);
        } else if (packet->_kk == KeyMaterial::KEY_BASED_ENCRYPTION_BOTH_SEK) {
            _ctx_pair[0] = createCtx(packet->_cipher, _passparase, KeyMaterial::KEY_BASED_ENCRYPTION_EVEN_SEK, packet);
            _ctx_pair[1] = createCtx(packet->_cipher, _passparase, KeyMaterial::KEY_BASED_ENCRYPTION_ODD_SEK, packet);
        }
    } catch (std::exception &ex) {
        WarnL << ex.what();
        return false;
    }
    return true;
}

BufferLikeString::Ptr Crypto::encrypt(DataPacket::Ptr pkt, const char *buf, int len) {
    _pkt_count++;

    //refresh
    if (_pkt_count == _re_announcement_period) {
        auto ctx = createCtx(KeyMaterial::CIPHER_AES_CTR, _passparase, _ctx_pair[!_ctx_idx]->_kk);
        _ctx_pair[!_ctx_idx] = ctx;
        _re_announce_pkt = generateAnnouncePacket(ctx);
    }

    if (_pkt_count > _refresh_period) {
        _pkt_count = 0;
        _ctx_idx = !_ctx_idx;
    }
 
    pkt->KK = _ctx_pair[_ctx_idx]->_kk;
    return _ctx_pair[_ctx_idx]->encrypt(pkt->packet_seq_number, buf, len);
}

BufferLikeString::Ptr Crypto::decrypt(DataPacket::Ptr pkt, const char *buf, int len) {
    CryptoContext::Ptr _ctx;
    if (pkt->KK == KeyMaterial::KEY_BASED_ENCRYPTION_NO_SEK) {
        auto payload = std::make_shared<BufferLikeString>();
        payload->assign(buf, len);
        return payload;
    } else if (pkt->KK == KeyMaterial::KEY_BASED_ENCRYPTION_EVEN_SEK) {
        _ctx = _ctx_pair[0];
    } else if (pkt->KK == KeyMaterial::KEY_BASED_ENCRYPTION_ODD_SEK) {
        _ctx = _ctx_pair[1];
    }

    if (!_ctx) {
        WarnL << "not has effective KeyMaterial with kk: " << pkt->KK;
        return nullptr;
    }

    return _ctx->decrypt(pkt->packet_seq_number, buf, len);
}

} // namespace SRT
