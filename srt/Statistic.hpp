#ifndef ZLMEDIAKIT_SRT_STATISTIC_H
#define ZLMEDIAKIT_SRT_STATISTIC_H
#include <map>

#include "Common.hpp"
#include "Packet.hpp"

namespace SRT {
class PacketRecvRateContext {
public:
    PacketRecvRateContext(TimePoint start);
    ~PacketRecvRateContext() = default;
    void inputPacket(TimePoint &ts,size_t len = 0);
    uint32_t getPacketRecvRate(uint32_t& bytesps);
    std::string dump();
    static const int SIZE = 16;
private:
    TimePoint _last_arrive_time;
    int64_t _ts_arr[SIZE];
    size_t _size_arr[SIZE];
    size_t _cur_idx;
    //std::map<int64_t, int64_t> _pkt_map;
};

class EstimatedLinkCapacityContext {
public:
    EstimatedLinkCapacityContext(TimePoint start);
    ~EstimatedLinkCapacityContext() = default;
    void setLastSeq(uint32_t seq){
        _last_seq = seq;
    }
    void inputPacket(TimePoint &ts,DataPacket::Ptr& pkt);
    uint32_t getEstimatedLinkCapacity();
    static const int SIZE = 64;
private:
    void probe1Arrival(TimePoint &ts,const DataPacket::Ptr& pkt, bool unordered);
    void probe2Arrival(TimePoint &ts,const DataPacket::Ptr& pkt);
private:
    TimePoint _start;
    TimePoint _ts_probe_time;
    int64_t _dur_probe_arr[SIZE];
    size_t _cur_idx;
    uint32_t _last_seq = 0;
    uint32_t _probe1_seq = SEQ_NONE;
    //std::map<int64_t, int64_t> _pkt_map;
};

/*
class RecvRateContext {
public:
    RecvRateContext(TimePoint start)
        : _start(start) {};
    ~RecvRateContext() = default;
    void inputPacket(TimePoint &ts, size_t size);
    uint32_t getRecvRate();

private:
    TimePoint _start;
    std::map<int64_t, size_t> _pkt_map;
};
*/
} // namespace SRT
#endif // ZLMEDIAKIT_SRT_STATISTIC_H