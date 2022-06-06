#ifndef ZLMEDIAKIT_SRT_PACKET_QUEUE_H
#define ZLMEDIAKIT_SRT_PACKET_QUEUE_H
#include "Packet.hpp"
#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

namespace SRT {

// for recv
class PacketQueue {
public:
    using Ptr = std::shared_ptr<PacketQueue>;
    using LostPair = std::pair<uint32_t, uint32_t>;

    PacketQueue(uint32_t max_size, uint32_t init_seq, uint32_t lantency);
    ~PacketQueue() = default;
    bool inputPacket(DataPacket::Ptr pkt, std::list<DataPacket::Ptr> &out);

    uint32_t timeLantency();
    std::list<LostPair> getLostSeq();

    size_t getSize();
    size_t getExpectedSize();
    size_t getAvailableBufferSize();
    uint32_t getExpectedSeq();

    bool drop(uint32_t first, uint32_t last, std::list<DataPacket::Ptr> &out);

    std::string dump();

private:
    void tryInsertPkt(DataPacket::Ptr pkt);

private:
    std::map<uint32_t, DataPacket::Ptr> _pkt_map;

    uint32_t _pkt_expected_seq = 0;
    uint32_t _pkt_cap;
    uint32_t _pkt_lantency;
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_PACKET_QUEUE_H