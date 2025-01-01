#ifndef ZLMEDIAKIT_SRT_CRYPTO_H
#define ZLMEDIAKIT_SRT_CRYPTO_H
#include <stdint.h>
#include <vector>

#include "Network/Buffer.h"
#include "Network/sockutil.h"
#include "Util/logger.h"

#include "Common.hpp"
#include "HSExt.hpp"
#include "Packet.hpp"

namespace SRT {

class CryptoContext : public std::enable_shared_from_this<CryptoContext> {
public:
    using Ptr = std::shared_ptr<CryptoContext>;
    CryptoContext(const std::string& passparase, uint8_t kk, KeyMaterial::Ptr packet = nullptr);
    virtual ~CryptoContext() = default;

    virtual void refresh();
    virtual std::string generateWarppedKey();

    virtual BufferLikeString::Ptr encrypt(uint32_t pkt_seq_no, const char *buf, int len) = 0;
    virtual BufferLikeString::Ptr decrypt(uint32_t pkt_seq_no, const char *buf, int len) = 0;
    virtual uint8_t getCipher() const = 0;

protected:
    virtual void loadFromKeyMaterial(KeyMaterial::Ptr packet);
    virtual bool generateKEK();
    BufferLikeString::Ptr generateIv(uint32_t pkt_seq_no);

private:

public:
    std::string _passparase;

    uint8_t _kk = SRT::KeyMaterial::KEY_BASED_ENCRYPTION_EVEN_SEK;

    BufferLikeString _kek;
    const uint32_t   _iter  = 2048;

    size_t           _slen = 16;
    BufferLikeString _salt;

    size_t           _klen = 16;
    BufferLikeString _sek;
};

class AesCtrCryptoContext : public CryptoContext {
public:
    using Ptr = std::shared_ptr<AesCtrCryptoContext>;
    AesCtrCryptoContext(const std::string& passparase, uint8_t kk, KeyMaterial::Ptr packet = nullptr);
    virtual ~AesCtrCryptoContext() = default;

    uint8_t getCipher() const  override {
        return KeyMaterial::CIPHER_AES_CTR;
    }

    BufferLikeString::Ptr encrypt(uint32_t pkt_seq_no, const char *buf, int len) override;
    BufferLikeString::Ptr decrypt(uint32_t pkt_seq_no, const char *buf, int len) override;

};


class Crypto : public std::enable_shared_from_this<Crypto>{
public:
    using Ptr = std::shared_ptr<Crypto>;
    Crypto(const std::string& passparase);
    virtual ~Crypto() = default;

    HSExtKeyMaterial::Ptr generateKeyMaterialExt(uint16_t extension_type);
    KeyMaterialPacket::Ptr takeAwayAnnouncePacket();

    bool loadFromKeyMaterial(KeyMaterial::Ptr packet);

    // for encryption
    std::string _passparase;

    //The recommended KM Refresh Period is after 2^25 packets encrypted with the same SEK are sent. 
    const uint32_t _refresh_period  = 1 <<25;
    const uint32_t _re_announcement_period = (1 <<25) - 4000;

    uint32_t _pkt_count = 0;
    KeyMaterialPacket::Ptr _re_announce_pkt;

    CryptoContext::Ptr  _ctx_pair[2];    /* Even(0)/Odd(1) crypto contexts */
    uint32_t _ctx_idx = 0;

    BufferLikeString::Ptr encrypt(DataPacket::Ptr pkt, const char *buf, int len);
    BufferLikeString::Ptr decrypt(DataPacket::Ptr pkt, const char *buf, int len);

private:

    CryptoContext::Ptr createCtx(int cipher, const std::string& passparase, uint8_t kk, KeyMaterial::Ptr packet = nullptr);
    KeyMaterialPacket::Ptr generateAnnouncePacket(CryptoContext::Ptr ctx);
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_CRYPTO_H
