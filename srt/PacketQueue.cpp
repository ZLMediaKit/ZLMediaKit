#include "PacketQueue.hpp"

namespace SRT {

static inline bool isSeqEdge(uint32_t seq, uint32_t cap) {
    if (seq > (MAX_SEQ - cap)) {
        return true;
    }
    return false;
}

static inline bool isTSCycle(uint32_t first, uint32_t second) {
    uint32_t diff;
    if (first > second) {
        diff = first - second;
    } else {
        diff = second - first;
    }

    if (diff > (MAX_TS >> 1)) {
        return true;
    } else {
        return false;
    }
}

PacketQueue::PacketQueue(uint32_t max_size, uint32_t init_seq, uint32_t latency)
    : _pkt_cap(max_size)
    , _pkt_latency(latency)
    , _pkt_expected_seq(init_seq) {}

void PacketQueue::tryInsertPkt(DataPacket::Ptr pkt) {
    if (_pkt_expected_seq <= pkt->packet_seq_number) {
        auto diff = pkt->packet_seq_number - _pkt_expected_seq;
        if (diff >= (MAX_SEQ >> 1)) {
            TraceL << "drop packet too later for cycle "
                   << "expected seq=" << _pkt_expected_seq << " pkt seq=" << pkt->packet_seq_number;
            return;
        } else {
            _pkt_map.emplace(pkt->packet_seq_number, pkt);
        }
    } else {
        auto diff = _pkt_expected_seq - pkt->packet_seq_number;
        if (diff >= (MAX_SEQ >> 1)) {
            _pkt_map.emplace(pkt->packet_seq_number, pkt);
            TraceL << " cycle packet "
                   << "expected seq=" << _pkt_expected_seq << " pkt seq=" << pkt->packet_seq_number;
        } else {
            // TraceL << "drop packet too later "<< "expected seq=" << _pkt_expected_seq << " pkt seq=" <<
            // pkt->packet_seq_number;
        }
    }
}

bool PacketQueue::inputPacket(DataPacket::Ptr pkt, std::list<DataPacket::Ptr> &out) {
    tryInsertPkt(pkt);
    auto it = _pkt_map.find(_pkt_expected_seq);
    while (it != _pkt_map.end()) {
        out.push_back(it->second);
        _pkt_map.erase(it);
        _pkt_expected_seq = genExpectedSeq(_pkt_expected_seq + 1);
        it = _pkt_map.find(_pkt_expected_seq);
    }

    while (_pkt_map.size() > _pkt_cap) {
        // 防止回环
        it = _pkt_map.find(_pkt_expected_seq);
        if (it != _pkt_map.end()) {
            out.push_back(it->second);
            _pkt_map.erase(it);
        }
        _pkt_expected_seq = genExpectedSeq(_pkt_expected_seq + 1);
    }

    while (timeLatency() > _pkt_latency) {
        it = _pkt_map.find(_pkt_expected_seq);
        if (it != _pkt_map.end()) {
            out.push_back(it->second);
            _pkt_map.erase(it);
        }
        _pkt_expected_seq = genExpectedSeq(_pkt_expected_seq + 1);
    }

    return true;
}

bool PacketQueue::drop(uint32_t first, uint32_t last, std::list<DataPacket::Ptr> &out) {
    uint32_t end = genExpectedSeq(last + 1);
    decltype(_pkt_map.end()) it;
    for (uint32_t i = _pkt_expected_seq; i < end;) {
        it = _pkt_map.find(i);
        if (it != _pkt_map.end()) {
            out.push_back(it->second);
            _pkt_map.erase(it);
        }
        i = genExpectedSeq(i + 1);
    }
    _pkt_expected_seq = end;
    return true;
}

uint32_t PacketQueue::timeLatency() {
    if (_pkt_map.empty()) {
        return 0;
    }

    auto first = _pkt_map.begin()->second->timestamp;
    auto last = _pkt_map.rbegin()->second->timestamp;
    uint32_t dur;
    if (last > first) {
        dur = last - first;
    } else {
        dur = first - last;
    }

    if (dur > 0x80000000) {
        dur = MAX_TS - dur;
        WarnL << "cycle dur " << dur;
    }

    return dur;
}

std::list<PacketQueue::LostPair> PacketQueue::getLostSeq() {
    std::list<PacketQueue::LostPair> re;
    if (_pkt_map.empty()) {
        return re;
    }

    if (getExpectedSize() == getSize()) {
        return re;
    }

    uint32_t end = 0;
    uint32_t first, last;

    first = _pkt_map.begin()->second->packet_seq_number;
    last = _pkt_map.rbegin()->second->packet_seq_number;
    if ((last - first) > (MAX_SEQ >> 1)) {
        TraceL << " cycle seq first " << first << " last " << last << " size " << _pkt_map.size();
        end = first;
    } else {
        end = last;
    }
    PacketQueue::LostPair lost;
    lost.first = 0;
    lost.second = 0;

    uint32_t i = _pkt_expected_seq;
    bool finish = true;
    for (i = _pkt_expected_seq; i <= end;) {
        if (_pkt_map.find(i) == _pkt_map.end()) {
            if (finish) {
                finish = false;
                lost.first = i;
                lost.second = genExpectedSeq(i + 1);
            } else {
                lost.second = genExpectedSeq(i + 1);
            }
        } else {
            if (!finish) {
                finish = true;
                re.push_back(lost);
            }
        }
        i = genExpectedSeq(i + 1);
    }

    return re;
}

size_t PacketQueue::getSize() {
    return _pkt_map.size();
}

size_t PacketQueue::getExpectedSize() {
    if (_pkt_map.empty()) {
        return 0;
    }

    uint32_t max = _pkt_map.rbegin()->first;
    uint32_t min = _pkt_map.begin()->first;
    if ((max - min) >= (MAX_SEQ >> 1)) {
        TraceL << "cycle "
               << "expected seq " << _pkt_expected_seq << " min " << min << " max " << max << " size "
               << _pkt_map.size();
        return MAX_SEQ - _pkt_expected_seq + min + 1;
    } else {
        return max - _pkt_expected_seq + 1;
    }
}

size_t PacketQueue::getAvailableBufferSize() {
    auto size = getExpectedSize();
    if (_pkt_cap > size) {
        return _pkt_cap - size;
    }

    if (_pkt_cap > _pkt_map.size()) {
        return _pkt_cap - _pkt_map.size();
    }
    WarnL << " cap " << _pkt_cap << " expected size " << size << " map size " << _pkt_map.size();
    return _pkt_cap;
}

uint32_t PacketQueue::getExpectedSeq() {
    return _pkt_expected_seq;
}

std::string PacketQueue::dump() {
    _StrPrinter printer;
    if (_pkt_map.empty()) {
        printer << " expected seq :" << _pkt_expected_seq;
    } else {
        printer << " expected seq :" << _pkt_expected_seq << " size:" << _pkt_map.size()
                << " first:" << _pkt_map.begin()->second->packet_seq_number;
        printer << " last:" << _pkt_map.rbegin()->second->packet_seq_number;
        printer << " latency:" << timeLatency() / 1e3;
    }
    return std::move(printer);
}

} // namespace SRT