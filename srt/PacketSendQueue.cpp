#include "PacketSendQueue.hpp"

namespace SRT {

PacketSendQueue::PacketSendQueue(uint32_t max_size, uint32_t latency,uint32_t flag)
    : _srt_flag(flag)
    , _pkt_cap(max_size)
    , _pkt_latency(latency) {}

bool PacketSendQueue::drop(uint32_t num) {
    decltype(_pkt_cache.begin()) it;
    for (it = _pkt_cache.begin(); it != _pkt_cache.end(); ++it) {
        if ((*it)->packet_seq_number == num) {
            break;
        }
    }
    if (it != _pkt_cache.end()) {
        _pkt_cache.erase(_pkt_cache.begin(), it);
    }
    return true;
}

bool PacketSendQueue::inputPacket(DataPacket::Ptr pkt) {
    _pkt_cache.push_back(pkt);
    while (_pkt_cache.size() > _pkt_cap) {
        _pkt_cache.pop_front();
    }
    while (timeLatency() > _pkt_latency && TLPKTDrop()) {
        _pkt_cache.pop_front();
    }
    return true;
}

bool PacketSendQueue::TLPKTDrop(){
    return (_srt_flag&HSExtMessage::HS_EXT_MSG_TLPKTDROP) && (_srt_flag &HSExtMessage::HS_EXT_MSG_TSBPDSND);
}

std::list<DataPacket::Ptr> PacketSendQueue::findPacketBySeq(uint32_t start, uint32_t end) {
    std::list<DataPacket::Ptr> re;
    decltype(_pkt_cache.begin()) it;
    for (it = _pkt_cache.begin(); it != _pkt_cache.end(); ++it) {
        if ((*it)->packet_seq_number == start) {
            break;
        }
    }

    if (start == end) {
        if (it != _pkt_cache.end()) {
            re.push_back(*it);
        }
        return re;
    }

    for (; it != _pkt_cache.end(); ++it) {
        re.push_back(*it);
        if ((*it)->packet_seq_number == end) {
            break;
        }
    }
    return re;
}

uint32_t PacketSendQueue::timeLatency() {
    if (_pkt_cache.empty()) {
        return 0;
    }
    auto first = _pkt_cache.front()->timestamp;
    auto last = _pkt_cache.back()->timestamp;
    uint32_t dur;

    if (last > first) {
        dur = last - first;
    } else {
        dur = first - last;
    }
    if (dur > ((uint32_t)0x01 << 31)) {
        TraceL << "cycle timeLatency " << dur;
        dur = 0xffffffff - dur;
    }

    return dur;
}

} // namespace SRT