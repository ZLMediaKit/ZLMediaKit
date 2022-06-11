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
} // namespace SRT
#endif // ZLMEDIAKIT_SRT_HS_EXT_H