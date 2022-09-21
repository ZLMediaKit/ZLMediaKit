#include <algorithm>

#include "Statistic.hpp"

namespace SRT {

PacketRecvRateContext::PacketRecvRateContext(TimePoint start)
    : _last_arrive_time(start) {
    for (size_t i = 0; i < SIZE; i++) {
        _ts_arr[i] = 1000000;
        _size_arr[i] = SRT_MAX_PAYLOAD_SIZE;
    }
    _cur_idx = 0;
};

void PacketRecvRateContext::inputPacket(TimePoint &ts,size_t len) {
    auto tmp = DurationCountMicroseconds(ts - _last_arrive_time);
    _ts_arr[_cur_idx] =  tmp;
    _size_arr[_cur_idx] = len;
    _cur_idx = (1+_cur_idx)%SIZE;
    _last_arrive_time = ts;
}

uint32_t PacketRecvRateContext::getPacketRecvRate(uint32_t &bytesps) {
    int64_t tmp_arry[SIZE];
    std::copy(_ts_arr, _ts_arr + SIZE, tmp_arry);
    std::nth_element(tmp_arry, tmp_arry + (SIZE / 2), tmp_arry + SIZE);
    int64_t median = tmp_arry[SIZE / 2];

    unsigned count = 0;
    int sum = 0;
    int64_t upper = median << 3;
    int64_t lower = median >> 3;

    bytesps = 0;
    size_t bytes = 0;
    const size_t *bp = _size_arr;
    // median filtering
    const int64_t *p = _ts_arr;
    for (int i = 0, n = SIZE; i < n; ++i) {
        if ((*p < upper) && (*p > lower)) {
            ++count; // packet counter
            sum += *p; // usec counter
            bytes += *bp; // byte counter
        }
        ++p; // advance packet pointer
        ++bp; // advance bytes pointer
    }

    // claculate speed, or return 0 if not enough valid value

    bytesps = (unsigned long)ceil(1000000.0 / (double(sum) / double(bytes)));
    auto ret = (uint32_t)ceil(1000000.0 / (sum / count));
    if(_cur_idx == 0)
        TraceL << bytesps << " byte/sec  " << ret << " pkt/sec";
    return ret;
}

void EstimatedLinkCapacityContext::inputPacket(TimePoint &ts) {
    if (_pkt_map.size() > 16) {
        _pkt_map.erase(_pkt_map.begin());
    }
    auto tmp = DurationCountMicroseconds(ts - _start);
    _pkt_map.emplace(tmp, tmp);
}

uint32_t EstimatedLinkCapacityContext::getEstimatedLinkCapacity() {
    decltype(_pkt_map.begin()) next;
    std::vector<int64_t> tmp;

    for (auto it = _pkt_map.begin(); it != _pkt_map.end(); ++it) {
        next = it;
        ++next;
        if (next != _pkt_map.end()) {
            tmp.push_back(next->first - it->first);
        } else {
            break;
        }
    }
    std::sort(tmp.begin(), tmp.end());
    if (tmp.empty()) {
        return 1000;
    }

    if (tmp.size() < 16) {
        return 1000;
    }

    double dur = tmp[0] / 1e6;
    return (uint32_t)(1.0 / dur);
}

/*
void RecvRateContext::inputPacket(TimePoint &ts, size_t size) {
    if (_pkt_map.size() > 100) {
        _pkt_map.erase(_pkt_map.begin());
    }
    auto tmp = DurationCountMicroseconds(ts - _start);
    _pkt_map.emplace(tmp, size);
}

uint32_t RecvRateContext::getRecvRate() {
    if (_pkt_map.size() < 2) {
        return 0;
    }

    auto first = _pkt_map.begin();
    auto last = _pkt_map.rbegin();
    double dur = (last->first - first->first) / 1000000.0;

    size_t bytes = 0;
    for (auto it : _pkt_map) {
        bytes += it.second;
    }
    double rate = (double)bytes / dur;
    return (uint32_t)rate;
}
*/
} // namespace SRT