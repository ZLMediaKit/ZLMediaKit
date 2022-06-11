#include "NackContext.hpp"

namespace SRT {
void NackContext::update(TimePoint now, std::list<PacketQueue::LostPair> &lostlist) {
    for (auto item : lostlist) {
        mergeItem(now, item);
    }
}
void NackContext::getLostList(
    TimePoint now, uint32_t rtt, uint32_t rtt_variance, std::list<PacketQueue::LostPair> &lostlist) {
    lostlist.clear();
    std::list<uint32_t> tmp_list;

    for (auto it = _nack_map.begin(); it != _nack_map.end(); ++it) {
        if (!it->second._is_nack) {
            tmp_list.push_back(it->first);
            it->second._ts = now;
            it->second._is_nack = true;
        } else {
            if (DurationCountMicroseconds(now - it->second._ts) > rtt) {
                tmp_list.push_back(it->first);
                it->second._ts = now;
            }
        }
    }
    tmp_list.sort();

    if (tmp_list.empty()) {
        return;
    }

    uint32_t min = *tmp_list.begin();
    uint32_t max = *tmp_list.rbegin();

    if ((max - min) >= (MAX_SEQ >> 1)) {
        while ((max - tmp_list.front()) > (MAX_SEQ >> 1)) {
            tmp_list.push_back(tmp_list.front());
            tmp_list.pop_front();
        }
    }

    PacketQueue::LostPair lost;
    bool finish = true;
    for (auto cur = tmp_list.begin(); cur != tmp_list.end(); ++cur) {
        if (finish) {
            lost.first = *cur;
            lost.second = genExpectedSeq(*cur + 1);
            finish = false;
        } else {
            if (lost.second == *cur) {
                lost.second = genExpectedSeq(*cur + 1);
            } else {
                finish = true;
                lostlist.push_back(lost);
            }
        }
    }
}
void NackContext::drop(uint32_t seq) {
    if (_nack_map.empty())
        return;
    uint32_t min = _nack_map.begin()->first;
    uint32_t max = _nack_map.rbegin()->first;
    bool is_cycle = false;
    if ((max - min) >= (MAX_SEQ >> 1)) {
        is_cycle = true;
    }

    for (auto it = _nack_map.begin(); it != _nack_map.end();) {
        if (!is_cycle) {
            // 不回环
            if (it->first <= seq) {
                it = _nack_map.erase(it);
            } else {
                it++;
            }
        } else {
            if (it->first <= seq) {
                if ((seq - it->first) >= (MAX_SEQ >> 1)) {
                    WarnL << "cycle seq  " << seq << " " << it->first;
                    it++;
                } else {
                    it = _nack_map.erase(it);
                }
            } else {
                if ((it->first - seq) >= (MAX_SEQ >> 1)) {
                    it = _nack_map.erase(it);
                    WarnL << "cycle seq  " << seq << " " << it->first;
                } else {
                    it++;
                }
            }
        }
    }
}

void NackContext::mergeItem(TimePoint now, PacketQueue::LostPair &item) {
    for (uint32_t i = item.first; i < item.second; ++i) {
        auto it = _nack_map.find(i);
        if (it != _nack_map.end()) {
        } else {
            NackItem tmp;
            tmp._is_nack = false;
            _nack_map.emplace(i, tmp);
        }
    }
}
} // namespace SRT