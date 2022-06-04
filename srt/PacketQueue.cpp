#include "PacketQueue.hpp"

namespace SRT {
PacketQueue::PacketQueue(uint32_t max_size, uint32_t init_seq, uint32_t lantency)
    : _pkt_expected_seq(init_seq)
    , _pkt_cap(max_size)
    , _pkt_lantency(lantency) {
    }

bool PacketQueue::inputPacket(DataPacket::Ptr pkt) {
    if (pkt->packet_seq_number < _pkt_expected_seq) {
        // TOO later drop this packet
        return false;
    }

    _pkt_map[pkt->packet_seq_number] = pkt;

    return true;
}

std::list<DataPacket::Ptr> PacketQueue::tryGetPacket() {
    std::list<DataPacket::Ptr> re;
    auto it = _pkt_map.find(_pkt_expected_seq);
    while ( it != _pkt_map.end()) {
        re.push_back(it->second);
        _last_pop_ts = it->second->get_ts;
        _pkt_map.erase(it);
        _pkt_expected_seq++;
        it = _pkt_map.find(_pkt_expected_seq);
    }

    while (_pkt_map.size() > _pkt_cap) {
        // force pop some packet
        it = _pkt_map.begin();
        re.push_back(it->second);
        _last_pop_ts = it->second->get_ts;
        _pkt_expected_seq = it->second->packet_seq_number + 1;
        _pkt_map.erase(it);
    }

    while (timeLantency() > _pkt_lantency) {
        auto it = _pkt_map.begin();
        re.push_back(it->second);
        _last_pop_ts = it->second->get_ts;
        _pkt_expected_seq = it->second->packet_seq_number + 1;
        _pkt_map.erase(it);
    }

    return re;
}


bool PacketQueue::dropForRecv(uint32_t first,uint32_t last){
    if(first >= last){
        return false;
    }

    if(_pkt_expected_seq <= last){
        for(uint32_t i =first;i<=last;++i){
            if(_pkt_map.find(i) != _pkt_map.end()){
                _pkt_map.erase(i);
            }
        }
         _pkt_expected_seq = last+1;
        return true;
    }

    return false;
}

bool PacketQueue::dropForSend(uint32_t num){
    if(num <= _pkt_expected_seq){
        return false;
    }
    decltype(_pkt_map.end()) it;
    for(uint32_t i =_pkt_expected_seq;i< num;++i){
            it = _pkt_map.find(i);
            if(it != _pkt_map.end()){
                _last_pop_ts = it->second->get_ts;
                _pkt_map.erase(it);
            }
    }
    _pkt_expected_seq = num;
    return true;
}

DataPacket::Ptr PacketQueue::findPacketBySeq(uint32_t seq){
    auto it = _pkt_map.find(seq);
    if(it != _pkt_map.end()){
        return it->second;
    }
    return nullptr;
}
uint32_t PacketQueue::timeLantencyFrom(TimePoint now){
    return DurationCountMicroseconds(now - _last_pop_ts);
}

std::list<DataPacket::Ptr> PacketQueue::tryGetPacketByNow(TimePoint now){
    std::list<DataPacket::Ptr> re;
    auto it = _pkt_map.begin();
    while(it !=_pkt_map.end()){
        if(DurationCountMicroseconds(now-it->second->get_ts)>=_pkt_lantency){
            re.push_back(it->second);
            _pkt_expected_seq = it->second->packet_seq_number+1;
            _last_pop_ts = it->second->get_ts;
            _pkt_map.erase(it);
        }
        it++;
    }
    return re;
}
uint32_t PacketQueue::timeLantency() {
    if (_pkt_map.empty()) {
        return 0;
    }

    auto first = _pkt_map.begin()->second;
    auto last = _pkt_map.rbegin()->second;

    return last->timestamp - first->timestamp;
}

std::list<PacketQueue::LostPair> PacketQueue::getLostSeq() {
    std::list<PacketQueue::LostPair> re;
    if(_pkt_map.empty()){
        return re;
    }
    
    if(getExpectedSize() == getSize()){
        return re;
    }

    PacketQueue::LostPair lost;
    lost.first = 0;
    lost.second = 0;

    uint32_t i = _pkt_expected_seq;
    bool finish = true;
    for(i = _pkt_expected_seq;i<=_pkt_map.rbegin()->first;++i){
        if(_pkt_map.find(i) == _pkt_map.end()){
            if(finish){
                finish = false;
                lost.first = i;
                lost.second = i+1;
            }else{
                lost.second = i+1;
            }
        }else{
            
            if(!finish){
                finish = true;
                re.push_back(lost);
            }
        }
    }

    return re;
}

size_t PacketQueue::getSize(){
    return _pkt_map.size();
}

size_t PacketQueue::getExpectedSize() {
    if(_pkt_map.empty()){
        return 0;
    }
    return _pkt_map.rbegin()->first - _pkt_expected_seq+1;
}

size_t PacketQueue::getAvailableBufferSize(){
    return _pkt_cap - getExpectedSize();
}

uint32_t PacketQueue::getExpectedSeq(){
    return _pkt_expected_seq;
}
} // namespace SRT