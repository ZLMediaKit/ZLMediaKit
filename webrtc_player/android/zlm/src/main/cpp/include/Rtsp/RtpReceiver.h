/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPRECEIVER_H
#define ZLMEDIAKIT_RTPRECEIVER_H

#include <map>
#include <string>
#include <memory>
#include "Rtsp/Rtsp.h"
#include "Extension/Frame.h"
// for NtpStamp
#include "Common/Stamp.h"
#include "Util/TimeTicker.h"

namespace mediakit {

template<typename T, typename SEQ = uint16_t>
class PacketSortor {
public:
    static constexpr SEQ SEQ_MAX = (std::numeric_limits<SEQ>::max)();
    using iterator = typename std::map<SEQ, T>::iterator;

    virtual ~PacketSortor() = default;

    void setOnSort(std::function<void(SEQ seq, T packet)> cb) { _cb = std::move(cb); }

    /**
     * 清空状态
     */
    void clear() {
        _started = false;
        _ticker.resetTime();
        _pkt_sort_cache_map.clear();
    }

    /**
     * 获取排序缓存长度
     */
    size_t getJitterSize() const { return _pkt_sort_cache_map.size(); }

    /**
     * 输入并排序
     * @param seq 序列号
     * @param packet 包负载
     */
    void sortPacket(SEQ seq, T packet) {
        _latest_seq = seq;
        if (!_started) {
            // 记录第一个seq
            _started = true;
            _last_seq_out = seq - 1;
        }
        auto next_seq = static_cast<SEQ>(_last_seq_out + 1);
        if (seq == next_seq) {
            // 收到下一个seq
            output(seq, std::move(packet));
            // 清空连续包列表
            flushPacket();
            return;
        }

        if (seq < next_seq && !mayLooped(next_seq, seq)) {
            // 无回环风险, 过滤seq回退包
            return;
        }
        _pkt_sort_cache_map.emplace(seq, std::move(packet));

        if (needForceFlush(seq)) {
            forceFlush(next_seq);
        }
    }

    void flush() {
        if (!_pkt_sort_cache_map.empty()) {
            forceFlush(static_cast<SEQ>(_last_seq_out + 1));
            _pkt_sort_cache_map.clear();
        }
    }

    void setParams(size_t max_buffer_size, size_t max_buffer_ms, size_t max_distance) {
        _max_buffer_size = max_buffer_size;
        _max_buffer_ms = max_buffer_ms;
        _max_distance = max_distance;
    }

private:
    SEQ distance(SEQ seq) {
        SEQ ret;
        auto next_seq = static_cast<SEQ>(_last_seq_out + 1);
        if (seq > next_seq) {
            ret = seq - next_seq;
        } else {
            ret = next_seq - seq;
        }
        if (ret > SEQ_MAX >> 1) {
            return SEQ_MAX - ret;
        }
        return ret;
    }

    bool needForceFlush(SEQ seq) {
        return !_pkt_sort_cache_map.empty() && (_pkt_sort_cache_map.size() > _max_buffer_size ||
               distance(seq) > _max_distance || _ticker.elapsedTime() > _max_buffer_ms);
    }

    //外部调用代码确保_pkt_sort_cache_map不为空
    void forceFlush(SEQ next_seq) {
        // 寻找距离比next_seq大的最近的seq
        auto it = _pkt_sort_cache_map.lower_bound(next_seq);
        if (it == _pkt_sort_cache_map.end()) {
            // 没有比next_seq更大的seq，应该是回环时丢包导致
            it = _pkt_sort_cache_map.begin();
        }
        // 丢包无法恢复，把这个包当做next_seq
        popIterator(it);
        // 清空连续包列表
        flushPacket();
        // 删除距离next_seq太大的包
        for (auto it = _pkt_sort_cache_map.begin(); it != _pkt_sort_cache_map.end();) {
            if (distance(it->first) > _max_distance) {
                it = _pkt_sort_cache_map.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool mayLooped(SEQ last_seq, SEQ now_seq) { return last_seq > SEQ_MAX - _max_distance || now_seq < _max_distance; }

    void flushPacket() {
        if (_pkt_sort_cache_map.empty()) {
            return;
        }
        auto next_seq = static_cast<SEQ>(_last_seq_out + 1);
        auto it = _pkt_sort_cache_map.lower_bound(next_seq);
        if (!mayLooped(next_seq, next_seq)) {
            // 无回环风险, 清空 < next_seq的值
            it = _pkt_sort_cache_map.erase(_pkt_sort_cache_map.begin(), it);
        }

        while (it != _pkt_sort_cache_map.end()) {
            // 找到下一个包
            if (it->first == static_cast<SEQ>(_last_seq_out + 1)) {
                it = popIterator(it);
                continue;
            }
            break;
        }
    }

    iterator popIterator(iterator it) {
        output(it->first, std::move(it->second));
        return _pkt_sort_cache_map.erase(it);
    }

    void output(SEQ seq, T packet) {
        auto next_seq = static_cast<SEQ>(_last_seq_out + 1);
        if (seq != next_seq) {
            WarnL << "packet dropped: " << next_seq << " -> " << static_cast<SEQ>(seq - 1)
                  << ", latest seq: " << _latest_seq
                  << ", jitter buffer size: " << _pkt_sort_cache_map.size()
                  << ", jitter buffer ms: " << _ticker.elapsedTime();
        }
        _last_seq_out = seq;
        _cb(seq, std::move(packet));
        _ticker.resetTime();
    }

private:
    bool _started = false;
    // 排序缓存最大保存数据长度，单位毫秒
    size_t _max_buffer_ms = 1000;
    // 排序缓存最大保存数据个数
    size_t _max_buffer_size = 1024;
    // seq最大跳跃距离
    size_t _max_distance = 256;
    // 记录上次output至今的时间
    toolkit::Ticker _ticker;
    // 最近输入的seq
    SEQ _latest_seq = 0;
    // 下次应该输出的SEQ
    SEQ _last_seq_out = 0;
    // pkt排序缓存，根据seq排序
    std::map<SEQ, T> _pkt_sort_cache_map;
    // 回调
    std::function<void(SEQ seq, T packet)> _cb;
};

class RtpTrack : public PacketSortor<RtpPacket::Ptr> {
public:
    class BadRtpException : public std::invalid_argument {
    public:
        template<typename Type>
        BadRtpException(Type &&type) : invalid_argument(std::forward<Type>(type)) {}
    };

    RtpTrack();

    void clear();
    uint32_t getSSRC() const;
    RtpPacket::Ptr inputRtp(TrackType type, int sample_rate, uint8_t *ptr, size_t len);
    void setNtpStamp(uint32_t rtp_stamp, uint64_t ntp_stamp_ms);
    void setPayloadType(uint8_t pt);

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

    void setPayloadType(int index, uint8_t pt){
        assert(index < kCount && index >= 0);
        _track[index].setPayloadType(pt);
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
