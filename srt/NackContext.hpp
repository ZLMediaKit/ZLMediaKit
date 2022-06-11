#ifndef ZLMEDIAKIT_SRT_NACK_CONTEXT_H
#define ZLMEDIAKIT_SRT_NACK_CONTEXT_H
#include "Common.hpp"
#include "PacketQueue.hpp"
#include <list>

namespace SRT {
class NackContext {
public:
    NackContext() = default;
    ~NackContext() = default;
    void update(TimePoint now, std::list<PacketQueue::LostPair> &lostlist);
    void getLostList(TimePoint now, uint32_t rtt, uint32_t rtt_variance, std::list<PacketQueue::LostPair> &lostlist);
    void drop(uint32_t seq);

private:
    void mergeItem(TimePoint now, PacketQueue::LostPair &item);

private:
    class NackItem {
    public:
        bool _is_nack = false;
        TimePoint _ts; // send nak time
    };

    std::map<uint32_t, NackItem> _nack_map;
};

} // namespace SRT
#endif // ZLMEDIAKIT_SRT_NACK_CONTEXT_H