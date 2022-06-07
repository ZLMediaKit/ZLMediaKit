#ifndef ZLMEDIAKIT_SRT_STATISTIC_H
#define ZLMEDIAKIT_SRT_STATISTIC_H
#include <map>

#include "Common.hpp"
#include "Packet.hpp"

namespace SRT {

class PacketRecvRateContext {
public:
    PacketRecvRateContext(TimePoint start)
        : _start(start) {};
    ~PacketRecvRateContext() = default;
    void inputPacket(TimePoint &ts);
    uint32_t getPacketRecvRate();

private:
    TimePoint _start;
    std::map<int64_t, int64_t> _pkt_map;
};

class EstimatedLinkCapacityContext {
public:
    EstimatedLinkCapacityContext(TimePoint start) : _start(start) {};
    ~EstimatedLinkCapacityContext() = default;
    void inputPacket(TimePoint &ts);
    uint32_t getEstimatedLinkCapacity();

private:
    TimePoint _start;
    std::map<int64_t, int64_t> _pkt_map;
};

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

} // namespace SRT
#endif // ZLMEDIAKIT_SRT_STATISTIC_H