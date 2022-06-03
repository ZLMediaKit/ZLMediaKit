#ifndef ZLMEDIAKIT_SRT_STATISTIC_H
#define ZLMEDIAKIT_SRT_STATISTIC_H
#include <map>

#include "Common.hpp"
#include "Packet.hpp"

namespace SRT {
class PacketRecvRateContext {
public:
    PacketRecvRateContext() = default;
    ~PacketRecvRateContext() = default;
    void inputPacket(TimePoint ts);
    uint32_t getPacketRecvRate();
private:
    std::map<TimePoint,TimePoint> _pkt_map;
    
};

class EstimatedLinkCapacityContext {
public:
    EstimatedLinkCapacityContext() = default;
    ~EstimatedLinkCapacityContext() = default;
    void inputPacket(TimePoint ts);
    uint32_t getEstimatedLinkCapacity();
private:
    std::map<TimePoint,TimePoint> _pkt_map;
};

class RecvRateContext {
public:
    RecvRateContext() = default;
    ~RecvRateContext() = default;
    void inputPacket(TimePoint ts,size_t size);
    uint32_t getRecvRate();
private:
    std::map<TimePoint,size_t> _pkt_map;
};



} // namespace SRT
#endif // ZLMEDIAKIT_SRT_STATISTIC_H