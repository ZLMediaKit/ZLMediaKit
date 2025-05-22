#ifndef ZLMEDIAKIT_SRT_HS_EXT_H
#define ZLMEDIAKIT_SRT_HS_EXT_H

#include "Network/Buffer.h"

#include "Common.hpp"

namespace SRT {
using namespace toolkit;
class HSExt : public Buffer {
public:
    HSExt() = default;
    virtual ~HSExt() = default;

    enum {
        SRT_CMD_REJECT = 0,
        SRT_CMD_HSREQ = 1,
        SRT_CMD_HSRSP = 2,
        SRT_CMD_KMREQ = 3,
        SRT_CMD_KMRSP = 4,
        SRT_CMD_SID = 5,
        SRT_CMD_CONGESTION = 6,
        SRT_CMD_FILTER = 7,
        SRT_CMD_GROUP = 8,
        SRT_CMD_NONE = -1
    };

    using Ptr = std::shared_ptr<HSExt>;
    uint16_t extension_type;
    uint16_t extension_length;
    virtual bool loadFromData(uint8_t *buf, size_t len) = 0;
    virtual bool storeToData() = 0;
    virtual std::string dump() = 0;
    ///////Buffer override///////
    char *data() const override {
        if (_data) {
            return _data->data();
        }
        return nullptr;
    }
    size_t size() const override {
        if (_data) {
            return _data->size();
        }
        return 0;
    }

protected:
    void loadHeader() {
        uint8_t *ptr = (uint8_t *)_data->data();
        extension_type = loadUint16(ptr);
        ptr += 2;
        extension_length = loadUint16(ptr);
        ptr += 2;
    }
    void storeHeader() {
        uint8_t *ptr = (uint8_t *)_data->data();
        SRT::storeUint16(ptr, extension_type);
        ptr += 2;
        storeUint16(ptr, extension_length);
    }

protected:
    BufferRaw::Ptr _data;
};

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          SRT Version                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           SRT Flags                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      Receiver TSBPD Delay     |       Sender TSBPD Delay      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    Figure 6: Handshake Extension Message structure
    https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-handshake-extension-message

*/
class HSExtMessage : public HSExt {
public:
    using Ptr = std::shared_ptr<HSExtMessage>;
    enum {
        HS_EXT_MSG_TSBPDSND = 0x00000001,
        HS_EXT_MSG_TSBPDRCV = 0x00000002,
        HS_EXT_MSG_CRYPT = 0x00000004,
        HS_EXT_MSG_TLPKTDROP = 0x00000008,
        HS_EXT_MSG_PERIODICNAK = 0x00000010,
        HS_EXT_MSG_REXMITFLG = 0x00000020,
        HS_EXT_MSG_STREAM = 0x00000040,
        HS_EXT_MSG_PACKET_FILTER = 0x00000080
    };
    enum { HSEXT_MSG_SIZE = 16 };
    HSExtMessage() = default;
    ~HSExtMessage() = default;
    bool loadFromData(uint8_t *buf, size_t len) override;
    bool storeToData() override;
    std::string dump() override;
    uint32_t srt_version;
    uint32_t srt_flag;
    uint16_t recv_tsbpd_delay;
    uint16_t send_tsbpd_delay;
};

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                           Stream ID                           |
                               ...
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    Figure 7: Stream ID Extension Message
    https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-stream-id-extension-message
*/
class HSExtStreamID : public HSExt {
public:
    using Ptr = std::shared_ptr<HSExtStreamID>;
    HSExtStreamID() = default;
    ~HSExtStreamID() = default;
    bool loadFromData(uint8_t *buf, size_t len) override;
    bool storeToData() override;
    std::string dump() override;
    std::string streamid;
};

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|S|  V  |   PT  |              Sign             |   Resv1   | KK|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                              KEKI                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Cipher    |      Auth     |       SE      |     Resv2     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             Resv3             |     SLen/4    |     KLen/4    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                              Salt                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                          Wrapped Key                          +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Figure 11: Key Material Message structure
https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-key-material
*/
class KeyMaterial {
public:
    using Ptr = std::shared_ptr<KeyMaterial>;
    KeyMaterial() = default;
    virtual ~KeyMaterial() = default;
    bool loadFromData(uint8_t *buf, size_t len);
    bool storeToData(uint8_t *buf, size_t len);
    std::string dump();

protected:
    size_t getContentSize();

public:

    enum {
        PACKET_TYPE_RESERVED = 0b0000,
        PACKET_TYPE_MSMSG    = 0b0001,   // 1-Media Strem Message
        PACKET_TYPE_KMMSG    = 0b0010,   // 2-Keying Material Message
        PACKET_TYPE_MPEG_TS  = 0b0111,   // 7-MPEG-TS packet
    };

    enum {
        KEY_BASED_ENCRYPTION_NO_SEK   = 0b00,
        KEY_BASED_ENCRYPTION_EVEN_SEK = 0b01,
        KEY_BASED_ENCRYPTION_ODD_SEK  = 0b10,
        KEY_BASED_ENCRYPTION_BOTH_SEK = 0b11,
    };

    enum {
        CIPHER_NONE     = 0x00,
        CIPHER_AES_ECB  = 0x01, //reserved, not support
        CIPHER_AES_CTR  = 0x02,
        CIPHER_AES_CBC  = 0x03, //reserved, not support
        CIPHER_AES_GCM  = 0x04 
    };

    enum {
        AUTHENTICATION_NONE = 0x00,
        AUTH_AES_GCM        = 0x01,
    };

    enum {
        STREAM_ENCAPSUALTION_UNSPECIFIED  = 0x00,
        STREAM_ENCAPSUALTION_MPEG_TS_UDP  = 0x01,
        STREAM_ENCAPSUALTION_MPEG_TS_SRT  = 0x02,
    };

    uint8_t     _km_version = 0b001;
    uint8_t     _pt         = PACKET_TYPE_KMMSG;
    uint16_t    _sign       = 0x2029;
    uint8_t     _kk         = KEY_BASED_ENCRYPTION_EVEN_SEK;
    uint32_t    _keki       = 0;
    uint8_t     _cipher     = CIPHER_AES_CTR;
    uint8_t     _auth       = AUTHENTICATION_NONE;
    uint8_t     _se         = STREAM_ENCAPSUALTION_MPEG_TS_SRT;
    uint16_t    _slen       = 16;
    uint16_t    _klen       = 16;
    BufferLikeString _salt;
    BufferLikeString _warpped_key;
};


class HSExtKeyMaterial : public HSExt, public KeyMaterial {
public:
    using Ptr = std::shared_ptr<HSExtKeyMaterial>;
    HSExtKeyMaterial() = default;
    virtual ~HSExtKeyMaterial() = default;
    bool loadFromData(uint8_t *buf, size_t len) override;
    bool storeToData() override;
    std::string dump() override;
};

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           KM State                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Figure 7: KM Response Error
https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-key-material-extension-mess
*/
class HSExtKMResponseError : public HSExt {
public:
    using Ptr = std::shared_ptr<HSExtKMResponseError>;
    HSExtKMResponseError() = default;
    ~HSExtKMResponseError() = default;
    bool loadFromData(uint8_t *buf, size_t len) override;
    bool storeToData() override;
    std::string dump() override;
};

} // namespace SRT
#endif // ZLMEDIAKIT_SRT_HS_EXT_H
