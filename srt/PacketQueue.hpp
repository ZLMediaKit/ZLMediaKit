#ifndef ZLMEDIAKIT_SRT_PACKET_QUEUE_H
#define ZLMEDIAKIT_SRT_PACKET_QUEUE_H
#include <memory>
#include <map>
#include <list>
#include <utility>
#include <tuple>

#include "Packet.hpp"

namespace SRT{

class PacketQueue
{
public:
    using Ptr = std::shared_ptr<PacketQueue>;
    using LostPair = std::pair<uint32_t,uint32_t>;

    PacketQueue(uint32_t max_size,uint32_t init_seq,uint32_t lantency);
    ~PacketQueue() = default;
    bool inputPacket(DataPacket::Ptr pkt);
    std::list<DataPacket::Ptr> tryGetPacket();
    uint32_t timeLantency();
    uint32_t timeLantencyFrom(TimePoint now);
    std::list<DataPacket::Ptr> tryGetPacketByNow(TimePoint now);
    std::list<LostPair> getLostSeq();

    size_t getSize();
    size_t getExpectedSize();
    size_t getAvailableBufferSize();
    uint32_t getExpectedSeq();

    bool dropForRecv(uint32_t first,uint32_t last);

    bool dropForSend(uint32_t num);

    DataPacket::Ptr findPacketBySeq(uint32_t seq);
    
    
private:
    std::map<uint32_t,DataPacket::Ptr> _pkt_map;

    uint32_t _pkt_expected_seq = 0;
    uint32_t _pkt_cap;
    uint32_t _pkt_lantency;

    TimePoint _last_pop_ts;
};

}

#endif //ZLMEDIAKIT_SRT_PACKET_QUEUE_H