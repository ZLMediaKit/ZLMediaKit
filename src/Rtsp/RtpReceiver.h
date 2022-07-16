/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPRECEIVER_H
#define ZLMEDIAKIT_RTPRECEIVER_H

#include <map>
#include <string>
#include <memory>
#include "RtpCodec.h"
#include "RtspMediaSource.h"
#include "Common/Stamp.h"

namespace mediakit {

template<typename T, typename SEQ = uint16_t, size_t kMax = 1024, size_t kMin = 32>
class PacketSortor {
public:
    PacketSortor() = default;
    ~PacketSortor() = default;

    void setOnSort(std::function<void(SEQ seq, T &packet)> cb) {
        _cb = std::move(cb);
    }

    /**
     * 清空状态
     */
    void clear() {
        _seq_cycle_count = 0;
        _pkt_sort_cache_map.clear();
        _next_seq_out = 0;
        _max_sort_size = kMin;
    }

    /**
     * 获取排序缓存长度
     */
    size_t getJitterSize() const{
        return _pkt_sort_cache_map.size();
    }

    /**
     * 获取seq回环次数
     */
    size_t getCycleCount() const{
        return _seq_cycle_count;
    }

    /**
     * 输入并排序
     * @param seq 序列号
     * @param packet 包负载
     */
    void sortPacket(SEQ seq, T packet) {
        if (seq < _next_seq_out) {
            if (_next_seq_out < seq + kMax) {
                //过滤seq回退包(回环包除外)
                return;
            }
        } else if (_next_seq_out && seq - _next_seq_out > ((std::numeric_limits<SEQ>::max)() >> 1)) {
            //过滤seq跳变非常大的包(防止回环时乱序时收到非常大的seq)
            return;
        }

        //放入排序缓存
        _pkt_sort_cache_map.emplace(seq, std::move(packet));
        //尝试输出排序后的包
        tryPopPacket();
    }

    void flush(){
        //清空缓存
        while (!_pkt_sort_cache_map.empty()) {
            popIterator(_pkt_sort_cache_map.begin());
        }
    }

private:
    void popPacket() {
        auto it = _pkt_sort_cache_map.begin();
        if (it->first >= _next_seq_out) {
            //过滤回跳包
            popIterator(it);
            return;
        }

        if (_next_seq_out - it->first > (0xFFFF >> 1)) {
            //产生回环了
            if (_pkt_sort_cache_map.size() < 2 * kMin) {
                //等足够多的数据后才处理回环, 因为后面还可能出现大的SEQ
                return;
            }
            ++_seq_cycle_count;
            //找到大的SEQ并清空掉，然后从小的SEQ重新开始排序
            auto hit = _pkt_sort_cache_map.upper_bound((SEQ) (_next_seq_out - _pkt_sort_cache_map.size()));
            while (hit != _pkt_sort_cache_map.end()) {
                //回环前，清空剩余的大的SEQ的数据
                _cb(hit->first, hit->second);
                hit = _pkt_sort_cache_map.erase(hit);
            }
            //下一个回环的数据
            popIterator(_pkt_sort_cache_map.begin());
            return;
        }
        //删除回跳的数据包
        _pkt_sort_cache_map.erase(it);
    }

    void popIterator(typename std::map<SEQ, T>::iterator it) {
        auto seq = it->first;
        auto data = std::move(it->second);
        _pkt_sort_cache_map.erase(it);
        _next_seq_out = seq + 1;
        _cb(seq, data);
    }

    void tryPopPacket() {
        int count = 0;
        while ((!_pkt_sort_cache_map.empty() && _pkt_sort_cache_map.begin()->first == _next_seq_out)) {
            //找到下个包，直接输出
            popPacket();
            ++count;
        }

        if (count) {
            setSortSize();
        } else if (_pkt_sort_cache_map.size() > _max_sort_size) {
            //排序缓存溢出，不再继续排序
            popPacket();
            setSortSize();
        }
    }

    void setSortSize() {
        _max_sort_size = kMin + _pkt_sort_cache_map.size();
        if (_max_sort_size > kMax) {
            _max_sort_size = kMax;
        }
    }

private:
    //下次应该输出的SEQ
    SEQ _next_seq_out = 0;
    //seq回环次数计数
    size_t _seq_cycle_count = 0;
    //排序缓存长度
    size_t _max_sort_size = kMin;
    //pkt排序缓存，根据seq排序
    std::map<SEQ, T> _pkt_sort_cache_map;
    //回调
    std::function<void(SEQ seq, T &packet)> _cb;
};

class RtpTrack : private PacketSortor<RtpPacket::Ptr>{
public:
    class BadRtpException : public std::invalid_argument {
    public:
        template<typename Type>
        BadRtpException(Type &&type) : invalid_argument(std::forward<Type>(type)) {}
        ~BadRtpException() = default;
    };

    RtpTrack();
    virtual ~RtpTrack() = default;

    void clear();
    uint32_t getSSRC() const;
    RtpPacket::Ptr inputRtp(TrackType type, int sample_rate, uint8_t *ptr, size_t len);
    void setNtpStamp(uint32_t rtp_stamp, uint64_t ntp_stamp_ms);
    void setPT(uint8_t pt);

protected:
    virtual void onRtpSorted(RtpPacket::Ptr rtp) {}
    virtual void onBeforeRtpSorted(const RtpPacket::Ptr &rtp) {}

private:
    bool _disable_ntp = false;
    uint8_t _pt = 0xFF;
    uint32_t _ssrc = 0;
    toolkit::Ticker _ssrc_alive;
    NtpStamp _ntp_stamp;
};

class RtpTrackImp : public RtpTrack{
public:
    using OnSorted = std::function<void(RtpPacket::Ptr)>;
    using BeforeSorted = std::function<void(const RtpPacket::Ptr &)>;

    RtpTrackImp() = default;
    ~RtpTrackImp() override = default;

    void setOnSorted(OnSorted cb);
    void setBeforeSorted(BeforeSorted cb);

protected:
    void onRtpSorted(RtpPacket::Ptr rtp) override;
    void onBeforeRtpSorted(const RtpPacket::Ptr &rtp) override;

private:
    OnSorted _on_sorted;
    BeforeSorted _on_before_sorted;
};

template<int kCount = 2>
class RtpMultiReceiver {
public:
    RtpMultiReceiver() {
        int index = 0;
        for (auto &track : _track) {
            track.setOnSorted([this, index](RtpPacket::Ptr rtp) {
                onRtpSorted(std::move(rtp), index);
            });
            track.setBeforeSorted([this, index](const RtpPacket::Ptr &rtp) {
                onBeforeRtpSorted(rtp, index);
            });
            ++index;
        }
    }

    virtual ~RtpMultiReceiver() = default;

    /**
     * 输入数据指针生成并排序rtp包
     * @param index track下标索引
     * @param type track类型
     * @param samplerate rtp时间戳基准时钟，视频为90000，音频为采样率
     * @param ptr rtp数据指针
     * @param len rtp数据指针长度
     * @return 解析成功返回true
     */
    bool handleOneRtp(int index, TrackType type, int sample_rate, uint8_t *ptr, size_t len) {
        assert(index < kCount && index >= 0);
        return _track[index].inputRtp(type, sample_rate, ptr, len).operator bool();
    }

    /**
     * 设置ntp时间戳，在收到rtcp sender report时设置
     * 如果rtp_stamp/sample_rate/ntp_stamp_ms都为0，那么采用rtp时间戳为ntp时间戳
     * @param index track下标索引
     * @param rtp_stamp rtp时间戳
     * @param ntp_stamp_ms ntp时间戳
     */
    void setNtpStamp(int index, uint32_t rtp_stamp, uint64_t ntp_stamp_ms) {
        assert(index < kCount && index >= 0);
        _track[index].setNtpStamp(rtp_stamp, ntp_stamp_ms);
    }

    void setPT(int index, uint8_t pt){
        assert(index < kCount && index >= 0);
        _track[index].setPT(pt);
    }

    void clear() {
        for (auto &track : _track) {
            track.clear();
        }
    }

    size_t getJitterSize(int index) const {
        assert(index < kCount && index >= 0);
        return _track[index].getJitterSize();
    }

    size_t getCycleCount(int index) const {
        assert(index < kCount && index >= 0);
        return _track[index].getCycleCount();
    }

    uint32_t getSSRC(int index) const {
        assert(index < kCount && index >= 0);
        return _track[index].getSSRC();
    }

protected:
    /**
     * rtp数据包排序后输出
     * @param rtp rtp数据包
     * @param track_index track索引
     */
    virtual void onRtpSorted(RtpPacket::Ptr rtp, int index) {}

    /**
     * 解析出rtp但还未排序
     * @param rtp rtp数据包
     * @param track_index track索引
     */
    virtual void onBeforeRtpSorted(const RtpPacket::Ptr &rtp, int index) {}

private:
    RtpTrackImp _track[kCount];
};

using RtpReceiver = RtpMultiReceiver<2>;

}//namespace mediakit


#endif //ZLMEDIAKIT_RTPRECEIVER_H
