#include "PacketQueue.hpp"

namespace SRT {

static inline bool isSeqEdge(uint32_t seq, uint32_t cap) {
    if (seq > (MAX_SEQ - cap)) {
        return true;
    }
    return false;
}

static inline bool isSeqCycle(uint32_t first, uint32_t second) {
    uint32_t diff;
    if (first > second) {
        diff = first - second;
    } else {
        diff = second - first;
    }

    if (diff > (MAX_SEQ >> 1)) {
        return true;
    } else {
        return false;
    }
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

//////////////////// PacketRecvQueue //////////////////////////////////

PacketRecvQueue::PacketRecvQueue(uint32_t max_size, uint32_t init_seq, uint32_t latency, uint32_t flag)
    : _pkt_cap(max_size)
    , _pkt_latency(latency)
    , _pkt_expected_seq(init_seq)
    , _srt_flag(flag)
    , _pkt_buf(max_size) {}

bool  PacketRecvQueue::TLPKTDrop(){
    return (_srt_flag&HSExtMessage::HS_EXT_MSG_TLPKTDROP) && (_srt_flag &HSExtMessage::HS_EXT_MSG_TSBPDRCV);
}
bool PacketRecvQueue::inputPacket(DataPacket::Ptr pkt, std::list<DataPacket::Ptr> &out) {
    // TraceL << dump() << " seq:" << pkt->packet_seq_number;
    while (_size > 0 && _start == _end) {
        if (_pkt_buf[_start]) {
            out.push_back(_pkt_buf[_start]);
            _size--;
            _pkt_buf[_start] = nullptr;
        }
        _start = (_start + 1) % _pkt_cap;
        _pkt_expected_seq = genExpectedSeq(_pkt_expected_seq + 1);
    }

    tryInsertPkt(pkt);

    DataPacket::Ptr it = _pkt_buf[_start];
    while (it) {
        out.push_back(it);
        _size--;
        _pkt_buf[_start] = nullptr;
        _pkt_expected_seq = genExpectedSeq(_pkt_expected_seq + 1);
        _start = (_start + 1) % _pkt_cap;
        it = _pkt_buf[_start];
    }
    while (timeLatency() > _pkt_latency && TLPKTDrop()) {
        it = _pkt_buf[_start];
        if (it) {
            _pkt_buf[_start] = nullptr;
            out.push_back(it);
            _size--;
        }
        _pkt_expected_seq = genExpectedSeq(_pkt_expected_seq + 1);
        _start = (_start + 1) % _pkt_cap;
    }
    return true;
}

uint32_t PacketRecvQueue::timeLatency() {
    if (_size <= 0) {
        return 0;
    }

    auto first = getFirst()->timestamp;
    auto last = getLast()->timestamp;

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
std::list<PacketQueueInterface::LostPair> PacketRecvQueue::getLostSeq() {
    std::list<PacketQueueInterface::LostPair> re;
    if (_size <= 0) {
        return re;
    }

    if (getExpectedSize() == getSize()) {
        return re;
    }

    LostPair lost;
    uint32_t steup = 0;
    bool finish = true;

    for (uint32_t i = _start; i != _end;) {
        if (!_pkt_buf[i]) {
            if (finish) {
                finish = false;
                lost.first = _pkt_expected_seq + steup;
                lost.second = genExpectedSeq(lost.first + 1);
            } else {
                lost.second = genExpectedSeq(_pkt_expected_seq + steup + 1);
            }
        } else {
            if (!finish) {
                finish = true;
                re.push_back(lost);
            }
        }
        i = (i + 1) % _pkt_cap;
        steup++;
    }
    return re;
}

size_t PacketRecvQueue::getSize() {
    return _size;
}
size_t PacketRecvQueue::getExpectedSize() {
    if (_size <= 0) {
        return 0;
    }

    uint32_t max, min;
    auto first = _pkt_expected_seq;
    auto last = getLast()->packet_seq_number;
    if (last >= first) {
        max = last;
        min = first;
    } else {
        max = first;
        min = last;
    }
    if ((max - min) >= (MAX_SEQ >> 1)) {
        TraceL << "cycle "
               << "expected seq " << _pkt_expected_seq << " min " << min << " max " << max << " size " << _size;
        return MAX_SEQ - _pkt_expected_seq + min + 1;
    } else {
        return max - _pkt_expected_seq + 1;
    }
}
size_t PacketRecvQueue::getAvailableBufferSize() {
    auto size = getExpectedSize();
    if (_pkt_cap > size) {
        return _pkt_cap - size;
    }

    if (_pkt_cap > _size) {
        return _pkt_cap - _size;
    }
    WarnL << " cap " << _pkt_cap << " expected size " << size << " map size " << _size;
    return _pkt_cap;
}
uint32_t PacketRecvQueue::getExpectedSeq() {
    return _pkt_expected_seq;
}

std::string PacketRecvQueue::dump() {
    _StrPrinter printer;
    if (_size <= 0) {
        printer << " expected seq :" << _pkt_expected_seq;
    } else {
        printer << " expected seq :" << _pkt_expected_seq << " size:" << _size
                << " first:" << getFirst()->packet_seq_number;
        printer << " last:" << getLast()->packet_seq_number;
        printer << " latency:" << timeLatency() / 1e3;
        printer << " start:" << _start;
        printer << " end:" << _end;
    }
    return std::move(printer);
}
bool PacketRecvQueue::drop(uint32_t first, uint32_t last, std::list<DataPacket::Ptr> &out) {
    uint32_t diff = 0;
    if (isSeqCycle(_pkt_expected_seq, last)) {
        if (last < _pkt_expected_seq) {
            diff = MAX_SEQ - _pkt_expected_seq + last + 1;
        } else {
            WarnL << "drop first " << first << " last " << last << " expected " << _pkt_expected_seq;
            return false;
        }
    } else {
        if (last < _pkt_expected_seq) {
            WarnL << "drop first " << first << " last " << last << " expected " << _pkt_expected_seq;
            return false;
        }
        diff = last - _pkt_expected_seq + 1;
    }

    if (diff > getExpectedSize()) {
        WarnL << " diff " << diff << " expected size " << getExpectedSize();
        return false;
    }

    for (uint32_t i = 0; i < diff; i++) {
        auto pos = (i + _start) % _pkt_cap;
        if (_pkt_buf[pos]) {
            out.push_back(_pkt_buf[pos]);
            _pkt_buf[pos] = nullptr;
            _size--;
        }
    }

    _pkt_expected_seq = genExpectedSeq(last + 1);
    _start = (diff + _start) % _pkt_cap;
    if (_size <= 0) {
        _end = _start;
        WarnL;
    }
    return true;
}

void PacketRecvQueue::insertToCycleBuf(DataPacket::Ptr pkt, uint32_t diff) {
    auto pos = (_start + diff) % _pkt_cap;

    if (!_pkt_buf[pos]) {
        _size++;
    } else {
        // WarnL << "repate packet " << pkt->packet_seq_number;
        return;
    }
    _pkt_buf[pos] = pkt;

    if (_start <= _end && pos >= _end) {
        _end = (pos + 1) % _pkt_cap;
        return;
    }

    if (_start <= _end && pos < _start) {
        _end = (pos + 1) % _pkt_cap;
        return;
    }

    if (_start > _end && _end <= pos && _start > pos) {
        _end = (pos + 1) % _pkt_cap;
        return;
    }
}
void PacketRecvQueue::tryInsertPkt(DataPacket::Ptr pkt) {
    if (_pkt_expected_seq <= pkt->packet_seq_number) {
        auto diff = pkt->packet_seq_number - _pkt_expected_seq;
        if (diff >= (MAX_SEQ >> 1)) {
            TraceL << "drop packet too later for cycle "
                   << "expected seq=" << _pkt_expected_seq << " pkt seq=" << pkt->packet_seq_number;
            return;
        } else {
            if (diff >= _pkt_cap) {
                WarnL << "too new "
                      << "expected seq=" << _pkt_expected_seq << " pkt seq=" << pkt->packet_seq_number << " cap "
                      << _pkt_cap;
                return;
            }

            insertToCycleBuf(pkt, diff);
        }
    } else {
        auto diff = _pkt_expected_seq - pkt->packet_seq_number;
        if (diff >= (MAX_SEQ >> 1)) {
            diff = MAX_SEQ - diff;
            if (diff >= _pkt_cap) {
                WarnL << "too new "
                      << "expected seq=" << _pkt_expected_seq << " pkt seq=" << pkt->packet_seq_number << " cap "
                      << _pkt_cap;
                return;
            }

            insertToCycleBuf(pkt, diff);

            TraceL << " cycle packet "
                   << "expected seq=" << _pkt_expected_seq << " pkt seq=" << pkt->packet_seq_number;
        } else {
            // TraceL << "drop packet too later "
            //<< "expected seq=" << _pkt_expected_seq << " pkt seq=" << pkt->packet_seq_number;
        }
    }
}
DataPacket::Ptr PacketRecvQueue::getFirst() {
    if (_size <= 0) {
        return nullptr;
    }

    uint32_t i = _start;
    while (1) {
        if (_pkt_buf[i]) {
            return _pkt_buf[i];
        }
        i = (i + 1) % _pkt_cap;
    }
}
DataPacket::Ptr PacketRecvQueue::getLast() {
    if (_size <= 0) {
        return nullptr;
    }
    uint32_t steup = 1;
    uint32_t i = (_end + _pkt_cap - steup) % _pkt_cap;
    /*
    while (1) {
        if (_pkt_buf[i]) {
            _end = (i + 1) % _pkt_cap;
            return _pkt_buf[i];
        }
        i = (_end + _pkt_cap - steup) % _pkt_cap;
        steup++;
    }
    */
    if (!_pkt_buf[i]) {
        WarnL << "start " << _start << " end" << _end << " size " << _size;
    }
    return _pkt_buf[i];
}
} // namespace SRT