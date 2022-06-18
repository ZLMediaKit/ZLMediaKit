#ifndef ZLMEDIAKIT_SRT_PACKET_QUEUE_H
#define ZLMEDIAKIT_SRT_PACKET_QUEUE_H
#include "Packet.hpp"
#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace SRT {

class PacketQueueInterface {
public:
    using Ptr = std::shared_ptr<PacketQueueInterface>;
    using LostPair = std::pair<uint32_t, uint32_t>;

    PacketQueueInterface() = default;
    virtual ~PacketQueueInterface() = default;
    virtual bool inputPacket(DataPacket::Ptr pkt, std::list<DataPacket::Ptr> &out) = 0;

    virtual uint32_t timeLatency() = 0;
    virtual std::list<LostPair> getLostSeq() = 0;

    virtual size_t getSize() = 0;
    virtual size_t getExpectedSize() = 0;
    virtual size_t getAvailableBufferSize() = 0;
    virtual uint32_t getExpectedSeq() = 0;

    virtual std::string dump() = 0;
    virtual bool drop(uint32_t first, uint32_t last, std::list<DataPacket::Ptr> &out) = 0;
};
// for recv
class PacketQueue : public PacketQueueInterface {
public:
    using Ptr = std::shared_ptr<PacketQueue>;

    PacketQueue(uint32_t max_size, uint32_t init_seq, uint32_t latency);
    ~PacketQueue() = default;
    bool inputPacket(DataPacket::Ptr pkt, std::list<DataPacket::Ptr> &out);

    uint32_t timeLatency();
    std::list<LostPair> getLostSeq();

    size_t getSize();
    size_t getExpectedSize();
    size_t getAvailableBufferSize();
    uint32_t getExpectedSeq();

    std::string dump();
    bool drop(uint32_t first, uint32_t last, std::list<DataPacket::Ptr> &out);

private:
    void tryInsertPkt(DataPacket::Ptr pkt);

private:
    uint32_t _pkt_cap;
    uint32_t _pkt_latency;
    uint32_t _pkt_expected_seq;
    std::map<uint32_t, DataPacket::Ptr> _pkt_map;
};

class PacketRecvQueue : public PacketQueueInterface {
public:
    using Ptr = std::shared_ptr<PacketRecvQueue>;

    PacketRecvQueue(uint32_t max_size, uint32_t init_seq, uint32_t latency,uint32_t flag = 0xbf);
    ~PacketRecvQueue() = default;
    bool inputPacket(DataPacket::Ptr pkt, std::list<DataPacket::Ptr> &out);

    uint32_t timeLatency();
    std::list<LostPair> getLostSeq();

    size_t getSize();
    size_t getExpectedSize();
    size_t getAvailableBufferSize();
    uint32_t getExpectedSeq();

    std::string dump();
    bool drop(uint32_t first, uint32_t last, std::list<DataPacket::Ptr> &out);

private:
    void tryInsertPkt(DataPacket::Ptr pkt);
    void insertToCycleBuf(DataPacket::Ptr pkt, uint32_t diff);
    DataPacket::Ptr getFirst();
    DataPacket::Ptr getLast();
    bool TLPKTDrop();

private:
    uint32_t _pkt_cap;
    uint32_t _pkt_latency;
    uint32_t _pkt_expected_seq;

    uint32_t _srt_flag;

    std::vector<DataPacket::Ptr> _pkt_buf;
    uint32_t _start = 0;
    uint32_t _end = 0;
    size_t _size = 0;
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_PACKET_QUEUE_H