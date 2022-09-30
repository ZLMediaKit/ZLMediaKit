#include <algorithm>
#include <math.h>
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

    int64_t min = median;
    int64_t min_size = 0;

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
        if(*p < min){
            min = *p;
            min_size = *bp;
        }
        ++p; // advance packet pointer
        ++bp; // advance bytes pointer
    }

    uint32_t max_ret = (uint32_t)ceil(1e6/min);
    uint32_t max_byteps = (uint32_t)ceil(1e6*min_size/min);

    if(count>(SIZE>>1)){
        bytesps = (uint32_t)ceil(1000000.0 / (double(sum) / double(bytes)));
        auto ret = (uint32_t)ceil(1000000.0 / (double(sum) / double(count)));
        //bytesps = max_byteps;

        return max_ret;
    }else{
        //TraceL<<max_ret<<" pkt/s "<<max_byteps<<" byte/s";
        bytesps = 0;
        return 0;
    }
    bytesps = 0;
    return 0;
    // claculate speed, or return 0 if not enough valid value

    
}

std::string PacketRecvRateContext::dump(){
    _StrPrinter printer;
    printer <<"dur array : ";
    for (size_t i = 0; i < SIZE; i++)
    {
        printer<<_ts_arr[i]<<" ";
    }
    printer <<"\r\n";

    printer <<"size array : ";
    for (size_t i = 0; i < SIZE; i++)
    {
        printer<<_size_arr[i]<<" ";
    }
    printer <<"\r\n";

    return std::move(printer);
}
EstimatedLinkCapacityContext::EstimatedLinkCapacityContext(TimePoint start) : _start(start) {
    for (size_t i = 0; i < SIZE; i++) {
        _dur_probe_arr[i] = 1000;
    }
    _cur_idx = 0;
};
void EstimatedLinkCapacityContext::inputPacket(TimePoint &ts,DataPacket::Ptr& pkt) {
    uint32_t  seq = pkt->packet_seq_number;
    auto diff  = seqCmp(seq,_last_seq);
    const bool retransmitted = pkt->R == 1;
    const bool unordered = diff<=0;
    uint32_t  one = seq&0xf;
    if(one == 0){
        probe1Arrival(ts,pkt,unordered || retransmitted);
    }
    if(diff>0){
        _last_seq = seq;
    }
    if(unordered || retransmitted){
        return;
    }
    if(one == 1){
        probe2Arrival(ts,pkt);
    }
}

/// Record the arrival time of the first probing packet.
void EstimatedLinkCapacityContext::probe1Arrival(TimePoint &ts, const DataPacket::Ptr &pkt, bool unordered) {
    if (unordered && pkt->packet_seq_number == _probe1_seq) {
        // Reset the starting probe into "undefined", when
        // a packet has come as retransmitted before the
        // measurement at arrival of 17th could be taken.
        _probe1_seq = SEQ_NONE;
        return;
    }

    _ts_probe_time = ts;
    _probe1_seq = pkt->packet_seq_number; // Record the sequence where 16th packet probe was taken
}

/// Record the arrival time of the second probing packet and the interval between packet pairs.

void EstimatedLinkCapacityContext::probe2Arrival(TimePoint &ts, const DataPacket::Ptr &pkt) {
    // Reject probes that don't refer to the very next packet
    // towards the one that was lately notified by probe1Arrival.
    // Otherwise the result can be stupid.

    // Simply, in case when this wasn't called exactly for the
    // expected packet pair, behave as if the 17th packet was lost.

    // no start point yet (or was reset) OR not very next packet
    if (_probe1_seq == SEQ_NONE || incSeq(_probe1_seq) != pkt->packet_seq_number)
        return;

    // Reset the starting probe to prevent checking if the
    // measurement was already taken.
    _probe1_seq = SEQ_NONE;

    // record the probing packets interval
    // Adjust the time for what a complete packet would have take
    const int64_t timediff = DurationCountMicroseconds(ts - _ts_probe_time);
    const int64_t timediff_times_pl_size = timediff * SRT_MAX_PAYLOAD_SIZE;

    // Let's take it simpler than it is coded here:
    // (stating that a packet has never zero size)
    //
    // probe_case = (now - previous_packet_time) * SRT_MAX_PAYLOAD_SIZE / pktsz;
    //
    // Meaning: if the packet is fully packed, probe_case = timediff.
    // Otherwise the timediff will be "converted" to a time that a fully packed packet "would take",
    // provided the arrival time is proportional to the payload size and skipping
    // the ETH+IP+UDP+SRT header part elliminates the constant packet delivery time influence.
    //
    const size_t pktsz = pkt->payloadSize();
    _dur_probe_arr[_cur_idx] = pktsz ? int64_t(timediff_times_pl_size / pktsz) : int64_t(timediff);

    // the window is logically circular
    _cur_idx = (_cur_idx + 1) % SIZE;
}

uint32_t EstimatedLinkCapacityContext::getEstimatedLinkCapacity() {
   int64_t tmp[SIZE];
   std::copy(_dur_probe_arr, _dur_probe_arr + SIZE , tmp);
   std::nth_element(tmp, tmp + (SIZE / 2), tmp + SIZE);
   int64_t median = tmp[SIZE / 2];

   int64_t count = 1;
   int64_t sum = median;
   int64_t upper = median << 3; // median*8
   int64_t lower = median >> 3; // median/8

   // median filtering
   const int64_t* p = _dur_probe_arr;
   for (int i = 0, n = SIZE; i < n; ++ i)
   {
      if ((*p < upper) && (*p > lower))
      {
         ++ count;
         sum += *p;
      }
      ++ p;
   }

   return (uint32_t)ceil(1000000.0 / (double(sum) / double(count)));
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