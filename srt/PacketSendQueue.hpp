#ifndef ZLMEDIAKIT_SRT_PACKET_SEND_QUEUE_H
#define ZLMEDIAKIT_SRT_PACKET_SEND_QUEUE_H

#include "Packet.hpp"
#include <algorithm>
#include <list>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

namespace SRT {

class PacketSendQueue {
public:
    using Ptr = std::shared_ptr<PacketSendQueue>;
    using LostPair = std::pair<uint32_t, uint32_t>;

    PacketSendQueue(uint32_t max_size, uint32_t latency,uint32_t flag = 0xbf);
    ~PacketSendQueue() = default;

    bool drop(uint32_t num);
    bool inputPacket(DataPacket::Ptr pkt);
    std::list<DataPacket::Ptr> findPacketBySeq(uint32_t start, uint32_t end);

private:
    uint32_t timeLatency();
    bool TLPKTDrop();
private:
    uint32_t _srt_flag;
    uint32_t _pkt_cap;
    uint32_t _pkt_latency;
    std::list<DataPacket::Ptr> _pkt_cache;
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_PACKET_SEND_QUEUE_H