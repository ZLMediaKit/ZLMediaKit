#ifndef ZLMEDIAKIT_SRT_PACKET_H
#define ZLMEDIAKIT_SRT_PACKET_H

#include <stdint.h>
#include <vector>

#include "Network/Buffer.h"
#include "Network/sockutil.h"
#include "Util/logger.h"

#include "Common.hpp"
#include "HSExt.hpp"

namespace SRT {

using namespace toolkit;
/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+- SRT Header +-+-+-+-+-+-+-+-+-+-+-+-+-+
|0|                    Packet Sequence Number                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|P P|O|K K|R|                   Message Number                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Destination Socket ID                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                              Data                             +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            Figure 3: Data packet structure
            reference https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-packet-structure
*/
class DataPacket : public Buffer {
public:
    using Ptr = std::shared_ptr<DataPacket>;
    DataPacket() = default;
    ~DataPacket() = default;

    static const size_t HEADER_SIZE = 16;
    static bool isDataPacket(uint8_t *buf, size_t len);
    static uint32_t getSocketID(uint8_t *buf, size_t len);
    bool loadFromData(uint8_t *buf, size_t len);
    bool storeToData(uint8_t *buf, size_t len);
    bool storeToHeader();

    ///////Buffer override///////
    char *data() const override;
    size_t size() const override;

    char *payloadData();
    size_t payloadSize();

    uint8_t f;
    uint32_t packet_seq_number;
    uint8_t PP;
    uint8_t O;
    uint8_t KK;
    uint8_t R;
    uint32_t msg_number;
    uint32_t timestamp;
    uint32_t dst_socket_id;

private:
    BufferRaw::Ptr _data;
};
/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+- SRT Header +-+-+-+-+-+-+-+-+-+-+-+-+-+
|1|         Control Type        |            Subtype            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Type-specific Information                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Destination Socket ID                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- CIF -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                   Control Information Field                   +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            Figure 4: Control packet structure
             reference https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-control-packets
*/
class ControlPacket : public Buffer {
public:
    using Ptr = std::shared_ptr<ControlPacket>;
    static const size_t HEADER_SIZE = 16;
    static bool isControlPacket(uint8_t *buf, size_t len);
    static uint16_t getControlType(uint8_t *buf, size_t len);
    static uint32_t getSocketID(uint8_t *buf, size_t len);

    ControlPacket() = default;
    virtual ~ControlPacket() = default;
    virtual bool loadFromData(uint8_t *buf, size_t len) = 0;
    virtual bool storeToData() = 0;

    bool loadHeader();
    bool storeToHeader();

    ///////Buffer override///////
    char *data() const override;
    size_t size() const override;

    enum {
        HANDSHAKE = 0x0000,
        KEEPALIVE = 0x0001,
        ACK = 0x0002,
        NAK = 0x0003,
        CONGESTIONWARNING = 0x0004,
        SHUTDOWN = 0x0005,
        ACKACK = 0x0006,
        DROPREQ = 0x0007,
        PEERERROR = 0x0008,
        USERDEFINEDTYPE = 0x7FFF
    };

    uint16_t sub_type;
    uint16_t control_type;
    uint8_t f;
    uint8_t type_specific_info[4];
    uint32_t timestamp;
    uint32_t dst_socket_id;

protected:
    BufferRaw::Ptr _data;
};

/**
  0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Version                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        Encryption Field       |        Extension Field        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Initial Packet Sequence Number                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Maximum Transmission Unit Size                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Maximum Flow Window Size                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Handshake Type                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         SRT Socket ID                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           SYN Cookie                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                                                               +
|                                                               |
+                        Peer IP Address                        +
|                                                               |
+                                                               +
|                                                               |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|         Extension Type        |        Extension Length       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                       Extension Contents                      +
|                                                               |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    Figure 5: Handshake packet structure
    https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-handshake
 */
class HandshakePacket : public ControlPacket {
public:
    using Ptr = std::shared_ptr<HandshakePacket>;
    enum { NO_ENCRYPTION = 0, AES_128 = 1, AES_196 = 2, AES_256 = 3 };
    static const size_t HS_CONTENT_MIN_SIZE = 48;
    enum {
        HS_TYPE_DONE = 0xFFFFFFFD,
        HS_TYPE_AGREEMENT = 0xFFFFFFFE,
        HS_TYPE_CONCLUSION = 0xFFFFFFFF,
        HS_TYPE_WAVEHAND = 0x00000000,
        HS_TYPE_INDUCTION = 0x00000001
    };

    enum { HS_EXT_FILED_HSREQ = 0x00000001, HS_EXT_FILED_KMREQ = 0x00000002, HS_EXT_FILED_CONFIG = 0x00000004 };

    HandshakePacket() = default;
    ~HandshakePacket() = default;

    static bool isHandshakePacket(uint8_t *buf, size_t len);
    static uint32_t getHandshakeType(uint8_t *buf, size_t len);
    static uint32_t getSynCookie(uint8_t *buf, size_t len);
    static uint32_t
    generateSynCookie(struct sockaddr_storage *addr, TimePoint ts, uint32_t current_cookie = 0, int correction = 0);
    std::string dump();
    void assignPeerIP(struct sockaddr_storage *addr);
    ///////ControlPacket override///////
    bool loadFromData(uint8_t *buf, size_t len) override;
    bool storeToData() override;

    uint32_t version;
    uint16_t encryption_field;
    uint16_t extension_field;
    uint32_t initial_packet_sequence_number;
    uint32_t mtu;
    uint32_t max_flow_window_size;
    uint32_t handshake_type;
    uint32_t srt_socket_id;
    uint32_t syn_cookie;
    uint8_t peer_ip_addr[16];

    std::vector<HSExt::Ptr> ext_list;

private:
    bool loadExtMessage(uint8_t *buf, size_t len);
    bool storeExtMessage();
    size_t getExtSize();
};
/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+- SRT Header +-+-+-+-+-+-+-+-+-+-+-+-+-+
|1|         Control Type        |            Reserved           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Type-specific Information                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Destination Socket ID                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    Figure 12: Keep-Alive control packet
    https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-keep-alive
*/
class KeepLivePacket : public ControlPacket {
public:
    using Ptr = std::shared_ptr<KeepLivePacket>;
    KeepLivePacket() = default;
    ~KeepLivePacket() = default;
    ///////ControlPacket override///////
    bool loadFromData(uint8_t *buf, size_t len) override;
    bool storeToData() override;
};

/*
An SRT NAK packet is formatted as follows:

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+- SRT Header +-+-+-+-+-+-+-+-+-+-+-+-+-+
|1|        Control Type         |           Reserved            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Type-specific Information                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Destination Socket ID                     |
+-+-+-+-+-+-+-+-+-+-+-+- CIF (Loss List) -+-+-+-+-+-+-+-+-+-+-+-+
|0|                 Lost packet sequence number                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|1|         Range of lost packets from sequence number          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|0|                    Up to sequence number                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|0|                 Lost packet sequence number                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    Figure 14: NAK control packet
    https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-nak-control-packet
*/
class NAKPacket : public ControlPacket {
public:
    using Ptr = std::shared_ptr<NAKPacket>;
    using LostPair = std::pair<uint32_t, uint32_t>;
    NAKPacket() = default;
    ~NAKPacket() = default;
    std::string dump();
    ///////ControlPacket override///////
    bool loadFromData(uint8_t *buf, size_t len) override;
    bool storeToData() override;

    std::list<LostPair> lost_list;
    static size_t getCIFSize(std::list<LostPair> &lost);
};

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+- SRT Header +-+-+-+-+-+-+-+-+-+-+-+-+-+
|1|      Control Type = 7       |         Reserved = 0          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Message Number                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Destination Socket ID                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 First Packet Sequence Number                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Last Packet Sequence Number                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    Figure 18: Drop Request control packet
    https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-message-drop-request
*/
class MsgDropReqPacket : public ControlPacket {
public:
    using Ptr = std::shared_ptr<MsgDropReqPacket>;
    MsgDropReqPacket() = default;
    ~MsgDropReqPacket() = default;
    ///////ControlPacket override///////
    bool loadFromData(uint8_t *buf, size_t len) override;
    bool storeToData() override;

    uint32_t first_pkt_seq_num;
    uint32_t last_pkt_seq_num;
};

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+- SRT Header +-+-+-+-+-+-+-+-+-+-+-+-+-+
|1|        Control Type         |           Reserved            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Type-specific Information                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Destination Socket ID                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    Figure 16: Shutdown control packet
    https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-shutdown

*/
class ShutDownPacket : public ControlPacket {
public:
    using Ptr = std::shared_ptr<ShutDownPacket>;
    ShutDownPacket() = default;
    ~ShutDownPacket() = default;

    ///////ControlPacket override///////
    bool loadFromData(uint8_t *buf, size_t len) override {
        if (len < HEADER_SIZE) {
            WarnL << "data size" << len << " less " << HEADER_SIZE;
            return false;
        }
        _data = BufferRaw::create();
        _data->assign((char *)buf, len);

        return loadHeader();
    }
    bool storeToData() override {
        control_type = ControlPacket::SHUTDOWN;
        sub_type = 0;
        _data = BufferRaw::create();
        _data->setCapacity(HEADER_SIZE);
        _data->setSize(HEADER_SIZE);
        return storeToHeader();
    }
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_PACKET_H