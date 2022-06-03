#include <algorithm>

#include "Statistic.hpp"
namespace SRT {
void PacketRecvRateContext::inputPacket(TimePoint ts) {
   if(_pkt_map.size()>100){
       _pkt_map.erase(_pkt_map.begin());
   }
   _pkt_map.emplace(ts,ts);
}
uint32_t PacketRecvRateContext::getPacketRecvRate() {
    if(_pkt_map.size()<2){
        return 0;
    }

    auto first = _pkt_map.begin();
    auto last = _pkt_map.rbegin();
    double dur = DurationCountMicroseconds(last->first - first->first)/1000000.0;
    double rate = _pkt_map.size()/dur;
    return (uint32_t)rate;
}

void EstimatedLinkCapacityContext::inputPacket(TimePoint ts) {
    if(_pkt_map.size()>16){
        _pkt_map.erase(_pkt_map.begin());
    }
     _pkt_map.emplace(ts,ts);
}
uint32_t EstimatedLinkCapacityContext::getEstimatedLinkCapacity() {
   decltype(_pkt_map.begin()) next;
   std::vector<SteadyClock::duration> tmp;

   for(auto it = _pkt_map.begin();it != _pkt_map.end();++it){
       next = it;
       ++next;
       if(next != _pkt_map.end()){
           tmp.push_back(next->first -it->first);
       }else{
           break;
       }
   }
   std::sort(tmp.begin(),tmp.end());
   if(tmp.empty()){
       return 0;
   }

   double dur =DurationCountMicroseconds(tmp[tmp.size()/2])/1e6;

   return  (uint32_t)(1.0/dur);

}

void RecvRateContext::inputPacket(TimePoint ts, size_t size ) {
    if (_pkt_map.size() > 100) {
        _pkt_map.erase(_pkt_map.begin());
    }
    _pkt_map.emplace(ts, size);
}
uint32_t RecvRateContext::getRecvRate() {
    if(_pkt_map.size()<2){
        return 0;
    }

    auto first = _pkt_map.begin();
    auto last = _pkt_map.rbegin();
    double dur = DurationCountMicroseconds(last->first - first->first)/1000000.0;

    size_t bytes = 0;
    for(auto it : _pkt_map){
        bytes += it.second;
    }
    double rate = (double)bytes/dur;
    return (uint32_t)rate;
}

} // namespace SRT