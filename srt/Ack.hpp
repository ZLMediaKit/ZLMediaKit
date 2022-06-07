#ifndef ZLMEDIAKIT_SRT_ACK_H
#define ZLMEDIAKIT_SRT_ACK_H
#include "Packet.hpp"

namespace SRT {
/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+- SRT Header +-+-+-+-+-+-+-+-+-+-+-+-+-+
|1|        Control Type         |           Reserved            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Acknowledgement Number                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Destination Socket ID                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- CIF -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            Last Acknowledged Packet Sequence Number           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                              RTT                              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          RTT Variance                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Available Buffer Size                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Packets Receiving Rate                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Estimated Link Capacity                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Receiving Rate                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    Figure 13: ACK control packet
    https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html#name-ack-acknowledgment
*/
class ACKPacket : public ControlPacket {
public:
    using Ptr = std::shared_ptr<ACKPacket>;
    ACKPacket() = default;
    ~ACKPacket() = default;

    enum { ACK_CIF_SIZE = 7 * 4 };
    std::string dump();
    ///////ControlPacket override///////
    bool loadFromData(uint8_t *buf, size_t len) override;
    bool storeToData() override;

    uint32_t ack_number;

    uint32_t last_ack_pkt_seq_number;
    uint32_t rtt;
    uint32_t rtt_variance;
    uint32_t available_buf_size;
    uint32_t pkt_recv_rate;
    uint32_t estimated_link_capacity;
    uint32_t recv_rate;
};

class ACKACKPacket : public ControlPacket {
public:
    using Ptr = std::shared_ptr<ACKACKPacket>;
    ACKACKPacket() = default;
    ~ACKACKPacket() = default;
    ///////ControlPacket override///////
    bool loadFromData(uint8_t *buf, size_t len) override {
        if (len < ControlPacket::HEADER_SIZE) {
            return false;
        }
        _data = BufferRaw::create();
        _data->assign((char *)(buf), len);
        ControlPacket::loadHeader();
        ack_number = loadUint32(type_specific_info);
        return true;
    }
    bool storeToData() override {
        _data = BufferRaw::create();
        _data->setCapacity(HEADER_SIZE);
        _data->setSize(HEADER_SIZE);
        control_type = ControlPacket::ACKACK;
        sub_type = 0;

        storeUint32(type_specific_info, ack_number);
        storeToHeader();
        return true;
    }

    uint32_t ack_number;
};

} // namespace SRT
#endif // ZLMEDIAKIT_SRT_ACK_H