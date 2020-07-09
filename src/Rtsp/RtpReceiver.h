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
    //上次seq
    uint16_t _last_seq[2] = { 0 , 0 };
    //seq连续次数计数
    uint32_t _seq_ok_count[2] = { 0 , 0};
    //seq回环次数计数
    uint32_t _seq_cycle_count[2] = { 0 , 0};
    //是否开始seq排序
    bool _sort_started[2] = { 0 , 0};
    //rtp排序缓存，根据seq排序
    map<uint16_t , RtpPacket::Ptr> _rtp_sort_cache_map[2];
    //rtp循环池
    RtspMediaSource::PoolType _rtp_pool;
};

}//namespace mediakit


#endif //ZLMEDIAKIT_RTPRECEIVER_H
