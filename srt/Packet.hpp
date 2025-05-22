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

static const size_t HDR_SIZE = 16; // packet header size = SRT_PH_E_SIZE * sizeof(uint32_t)

// Can also be calculated as: sizeof(struct ether_header) + sizeof(struct ip) + sizeof(struct udphdr).
static const size_t UDP_HDR_SIZE = 28; // 20 bytes IPv4 + 8 bytes of UDP { u16 sport, dport, len, csum }.

static const size_t SRT_DATA_HDR_SIZE = UDP_HDR_SIZE + HDR_SIZE;

// Maximum transmission unit size. 1500 in case of Ethernet II (RFC 1191).
static const size_t ETH_MAX_MTU_SIZE = 1500;

// Maximum payload size of an SRT packet.
static const size_t SRT_MAX_PAYLOAD_SIZE = ETH_MAX_MTU_SIZE - SRT_DATA_HDR_SIZE;

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
    bool reloadPayload(uint8_t *buf, size_t len);
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
    static uint16_t getSubType(uint8_t *buf, size_t len);
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

// REJ code,from libsrt
#define REJ_MAP(XX) \
XX(SRT_REJ_UNKNOWN,    1000, "Unknown or erroneous")                    \
XX(SRT_REJ_SYSTEM,     1001, "Error in system calls")                   \
XX(SRT_REJ_PEER,       1002, "Peer rejected connection")                \
XX(SRT_REJ_RESOURCE,   1003, "Resource allocation failure")             \
XX(SRT_REJ_ROGUE,      1004, "Rogue peer or incorrect parameters")      \
XX(SRT_REJ_BACKLOG,    1005, "Listener's backlog exceeded")             \
XX(SRT_REJ_IPE,        1006, "Internal Program Error")                  \
XX(SRT_REJ_CLOSE,      1007, "Socket is being closed")                  \
XX(SRT_REJ_VERSION,    1008, "Peer version too old")                    \
XX(SRT_REJ_RDVCOOKIE,  1009, "Rendezvous-mode cookie collision")        \
XX(SRT_REJ_BADSECRET,  1010, "Incorrect passphrase")                    \
XX(SRT_REJ_UNSECURE,   1011, "Password required or unexpected")         \
XX(SRT_REJ_MESSAGEAPI, 1012, "MessageAPI/StreamAPI collision")          \
XX(SRT_REJ_CONGESTION, 1013, "Congestion controller type collision")    \
XX(SRT_REJ_FILTER,     1014, "Packet Filter settings error")            \
XX(SRT_REJ_GROUP,      1015, "Group settings collision")                \
XX(SRT_REJ_TIMEOUT,    1016, "Connection timeout")                      \
XX(SRT_REJ_CRYPTO,     1017, "Crypto mode")

typedef enum {
#define XX(name, value, str) name = value,
    REJ_MAP(XX)
#undef XX
    SRT_REJ_E_SIZE
} SRT_REJECT_REASON;

std::string getRejectReason(SRT_REJECT_REASON code);

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
    void assignPeerIPBE(struct sockaddr_storage *addr);
    bool isReject() {
        return (handshake_type >= SRT_REJ_UNKNOWN && handshake_type < SRT_REJ_E_SIZE);
    }
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

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+- SRT Header +-+-+-+-+-+-+-+-+-+-+-+-+-+
|1|    Control Type = 0x7FFF    |            Subtype = 3/4      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Type-specific Information                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Destination Socket ID                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
the Control Type field of the SRT packet header is set to User-Defined Type (see Table 1), 
the Subtype field of the header is set to SRT_CMD_KMREQ for key-refresh request 
and SRT_CMD_KMRSP for key-refresh response (Table 5). The KM Refresh mechanism is described in Section 6.1.6.
https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-key-material
*/

class KeyMaterialPacket : public ControlPacket, public KeyMaterial {
public:
    using Ptr = std::shared_ptr<KeyMaterialPacket>;
    KeyMaterialPacket() = default;
    ~KeyMaterialPacket() = default;

    ///////ControlPacket override///////
    bool loadFromData(uint8_t *buf, size_t len) override {
        if (len < HEADER_SIZE) {
            WarnL << "data size" << len << " less " << HEADER_SIZE;
            return false;
        }
        _data = BufferRaw::create();
        _data->assign((char *)buf, len);
        loadHeader();
        assert(sub_type == HSExt::SRT_CMD_KMREQ || sub_type == HSExt::SRT_CMD_KMRSP);
        return KeyMaterial::loadFromData(buf + HEADER_SIZE, len - HEADER_SIZE);
    }

    bool storeToData() override {
        size_t content_size = ((KeyMaterial::getContentSize() + HEADER_SIZE) + 3) / 4 * 4;
        control_type = ControlPacket::USERDEFINEDTYPE;
        /* sub_type = HSExt::SRT_CMD_KMREQ; */
        /* sub_type = HSExt::SRT_CMD_KMRSP; */
        _data = BufferRaw::create();
        _data->setCapacity(content_size);
        _data->setSize(content_size);
        storeToHeader();
        return KeyMaterial::storeToData((uint8_t*)_data->data() + HEADER_SIZE, content_size - HEADER_SIZE);
    }
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_PACKET_H
