/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
using namespace std;
using namespace toolkit;

namespace mediakit {

template<typename T, typename SEQ = uint16_t>
class PacketSortor {
public:
    PacketSortor() = default;
    ~PacketSortor() = default;

    /**
     * 设置参数
     * @param max_sort_size 最大排序缓存长度
     * @param clear_sort_size seq连续次数超过该值后，清空并关闭排序缓存
     */
    void setup(uint32_t max_sort_size, uint32_t clear_sort_size) {
        _max_sort_size = max_sort_size;
        _clear_sort_size = clear_sort_size;
    }

    void setOnSort(function<void(SEQ seq, const T &packet)> cb){
        _cb = std::move(cb);
    }

    /**
     * 清空状态
     */
    void clear() {
        _last_seq = 0;
        _seq_ok_count = 0;
        _sort_started = 0;
        _seq_cycle_count = 0;
        _rtp_sort_cache_map.clear();
    }

    /**
     * 获取排序缓存长度
     */
    int getJitterSize(){
        return _rtp_sort_cache_map.size();
    }

    /**
     * 获取seq回环次数
     */
    int getCycleCount(){
        return _seq_cycle_count;
    }

    /**
     * 输入并排序
     * @param seq 序列号
     * @param packet 包负载
     */
    void sortPacket(SEQ seq, const T &packet){
        if (seq != _last_seq + 1 && _last_seq != 0) {
            //包乱序或丢包
            _seq_ok_count = 0;
            _sort_started = true;
            if (_last_seq > seq && _last_seq - seq > 0xFF) {
                //sequence回环，清空所有排序缓存
                while (_rtp_sort_cache_map.size()) {
                    popPacket();
                }
                ++_seq_cycle_count;
            }
        } else {
            //正确序列的包
            _seq_ok_count++;
        }

        _last_seq = seq;

        //开始排序缓存
        if (_sort_started) {
            _rtp_sort_cache_map.emplace(seq, packet);
            if (_seq_ok_count >= _clear_sort_size) {
                //网络环境改善，需要清空排序缓存
                _seq_ok_count = 0;
                _sort_started = false;
                while (_rtp_sort_cache_map.size()) {
                    popPacket();
                }
            } else if (_rtp_sort_cache_map.size() >= _max_sort_size) {
                //排序缓存溢出
                popPacket();
            }
        } else {
            //正确序列
            onPacketSorted(seq, packet);
        }
    }

private:
    void popPacket() {
        auto it = _rtp_sort_cache_map.begin();
        onPacketSorted(it->first, it->second);
        _rtp_sort_cache_map.erase(it);
    }

    void onPacketSorted(SEQ seq, const T &packet) {
        _cb(seq, packet);
    }

private:
    //是否开始seq排序
    bool _sort_started = false;
    //上次seq
    SEQ _last_seq = 0;
    //seq连续次数计数
    uint32_t _seq_ok_count = 0;
    //seq回环次数计数
    uint32_t _seq_cycle_count = 0;
    //排序缓存长度
    uint32_t _max_sort_size;
    //seq连续次数超过该值后，清空并关闭排序缓存
    uint32_t _clear_sort_size;
    //rtp排序缓存，根据seq排序
    map<SEQ, T> _rtp_sort_cache_map;
    //回调
    function<void(SEQ seq, const T &packet)> _cb;
};

class RtpReceiver {
public:
    RtpReceiver();
    virtual ~RtpReceiver();

protected:
    /**
     * 输入数据指针生成并排序rtp包
     * @param track_index track下标索引
     * @param type track类型
     * @param samplerate rtp时间戳基准时钟，视频为90000，音频为采样率
     * @param rtp_raw_ptr rtp数据指针
     * @param rtp_raw_len rtp数据指针长度
     * @return 解析成功返回true
     */
    bool handleOneRtp(int track_index, TrackType type, int samplerate, unsigned char *rtp_raw_ptr, unsigned int rtp_raw_len);

    /**
     * rtp数据包排序后输出
     * @param rtp rtp数据包
     * @param track_index track索引
     */
    virtual void onRtpSorted(const RtpPacket::Ptr &rtp, int track_index){}

    void clear();
    void setPoolSize(int size);
    int getJitterSize(int track_index);
    int getCycleCount(int track_index);

private:
    void sortRtp(const RtpPacket::Ptr &rtp , int track_index);

private:
    uint32_t _ssrc[2] = { 0, 0 };
    //ssrc不匹配计数
    uint32_t _ssrc_err_count[2] = { 0, 0 };
    //rtp排序缓存，根据seq排序
    PacketSortor<RtpPacket::Ptr> _rtp_sortor[2];
    //rtp循环池
    RtspMediaSource::PoolType _rtp_pool;
};

}//namespace mediakit


#endif //ZLMEDIAKIT_RTPRECEIVER_H
