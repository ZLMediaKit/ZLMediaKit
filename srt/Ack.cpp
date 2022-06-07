#include "Ack.hpp"
#include "Common.hpp"

namespace SRT {

bool ACKPacket::loadFromData(uint8_t *buf, size_t len) {
    if (len < ACK_CIF_SIZE + ControlPacket::HEADER_SIZE) {
        return false;
    }

    _data = BufferRaw::create();
    _data->assign((char *)(buf), len);
    ControlPacket::loadHeader();
    ack_number = loadUint32(type_specific_info);
    uint8_t *ptr = (uint8_t *)_data->data() + ControlPacket::HEADER_SIZE;

    last_ack_pkt_seq_number = loadUint32(ptr);
    ptr += 4;

    rtt = loadUint32(ptr);
    ptr += 4;

    rtt_variance = loadUint32(ptr);
    ptr += 4;

    available_buf_size = loadUint32(ptr);
    ptr += 4;

    pkt_recv_rate = loadUint32(ptr);
    ptr += 4;

    estimated_link_capacity = loadUint32(ptr);
    ptr += 4;

    recv_rate = loadUint32(ptr);
    ptr += 4;

    return true;
}

bool ACKPacket::storeToData() {
    _data = BufferRaw::create();
    _data->setCapacity(HEADER_SIZE + ACK_CIF_SIZE);
    _data->setSize(HEADER_SIZE + ACK_CIF_SIZE);
    control_type = ControlPacket::ACK;
    sub_type = 0;

    storeUint32(type_specific_info, ack_number);
    storeToHeader();

    uint8_t *ptr = (uint8_t *)_data->data() + ControlPacket::HEADER_SIZE;

    storeUint32(ptr, last_ack_pkt_seq_number);
    ptr += 4;

    storeUint32(ptr, rtt);
    ptr += 4;

    storeUint32(ptr, rtt_variance);
    ptr += 4;

    storeUint32(ptr, pkt_recv_rate);
    ptr += 4;

    storeUint32(ptr, available_buf_size);
    ptr += 4;

    storeUint32(ptr, estimated_link_capacity);
    ptr += 4;

    storeUint32(ptr, recv_rate);
    ptr += 4;

    return true;
}

std::string ACKPacket::dump() {
    _StrPrinter printer;
    printer << "last_ack_pkt_seq_number=" << last_ack_pkt_seq_number << " rtt=" << rtt
            << " rtt_variance=" << rtt_variance << " pkt_recv_rate=" << pkt_recv_rate
            << " available_buf_size=" << available_buf_size << " estimated_link_capacity=" << estimated_link_capacity
            << " recv_rate=" << recv_rate;
    return std::move(printer);
}
} // namespace SRT