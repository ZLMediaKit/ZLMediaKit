/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_STAMP_H
#define ZLMEDIAKIT_STAMP_H

#include <set>
#include <cstdint>
#include "Util/TimeTicker.h"

namespace mediakit {

class DeltaStamp {
public:
    DeltaStamp();
    virtual ~DeltaStamp() = default;

    /**
     * 计算时间戳增量
     * @param stamp 绝对时间戳
     * @param enable_rollback 是否允许相当时间戳回退
     * @return 时间戳增量
     * Calculate the timestamp increment
     * @param stamp Absolute timestamp
     * @param enable_rollback Whether to allow the timestamp to roll back
     * @return Timestamp increment
     
     * [AUTO-TRANSLATED:e8d21dcd]
     */
    int64_t deltaStamp(int64_t stamp, bool enable_rollback = true);
    int64_t relativeStamp(int64_t stamp, bool enable_rollback = true);
    int64_t relativeStamp();

    // 设置最大允许回退或跳跃幅度  [AUTO-TRANSLATED:e5b44ede]
    // Set the maximum allowed rollback or jump amplitude
    void setMaxDelta(size_t max_delta);

protected:
    virtual void needSync() {}

protected:
    int _max_delta;
    int64_t _last_stamp = 0;
    int64_t _relative_stamp = 0;
};

// 该类解决时间戳回环、回退问题  [AUTO-TRANSLATED:b442692c]
// This class solves the problem of timestamp loopback and rollback
// 计算相对时间戳或者产生平滑时间戳  [AUTO-TRANSLATED:0deabd6e]
// Calculate the relative timestamp or generate a smooth timestamp
class Stamp : public DeltaStamp{
public:
    /**
     * 求取相对时间戳,同时实现了音视频同步、限制dts回退等功能
     * @param dts 输入dts，如果为0则根据系统时间戳生成
     * @param pts 输入pts，如果为0则等于dts
     * @param dts_out 输出dts
     * @param pts_out 输出pts
     * @param modifyStamp 是否用系统时间戳覆盖
     * Get the relative timestamp, which also implements audio and video synchronization, limits dts rollback, etc.
     * @param dts Input dts, if it is 0, it will be generated according to the system timestamp
     * @param pts Input pts, if it is 0, it is equal to dts
     * @param dts_out Output dts
     * @param pts_out Output pts
     * @param modifyStamp Whether to overwrite with the system timestamp
     
     * [AUTO-TRANSLATED:0b939dc5]
     */
    void revise(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp = false);

    /**
     * 再设置相对时间戳，用于seek用
     * @param relativeStamp 相对时间戳
     * Set the relative timestamp again, used for seek
     * @param relativeStamp Relative timestamp
     
     * [AUTO-TRANSLATED:fc087399]
     */
    void setRelativeStamp(int64_t relativeStamp);

    /**
     * 获取当前相对时间戳
     * @return
     * Get the current relative timestamp
     * @return
     
     * [AUTO-TRANSLATED:7ca29fde]
     */
    int64_t getRelativeStamp() const ;

    /**
     * 设置是否为回放模式，回放模式运行时间戳回退
     * @param playback 是否为回放模式
     * Set whether it is playback mode, playback mode allows timestamp rollback
     * @param playback Whether it is playback mode
     
     * [AUTO-TRANSLATED:ffe5e40b]
     */
    void setPlayBack(bool playback = true);

    /**
     * 音视频同步用，音频应该同步于视频(只修改音频时间戳)
     * 因为音频时间戳修改后不影响播放速度
     * Used for audio and video synchronization, audio should be synchronized with video (only modify audio timestamp)
     * Because modifying the audio timestamp does not affect the playback speed
     
     * [AUTO-TRANSLATED:7ac41a76]
     */
    void syncTo(Stamp &other);

    /**
     * 是否允许时间戳回退
     * Whether to allow timestamp rollback
     
     * [AUTO-TRANSLATED:1d32f7e3]
     */
    void enableRollback(bool flag);

private:
    // 主要实现音视频时间戳同步功能  [AUTO-TRANSLATED:45863fce]
    // Mainly implements audio and video timestamp synchronization function
    void revise_l(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp = false);

    // 主要实现获取相对时间戳功能  [AUTO-TRANSLATED:4e042942]
    // Mainly implements the function of obtaining the relative timestamp
    void revise_l2(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp = false);

    void needSync() override;

private:
    bool _playback = false;
    bool _need_sync = false;
    // 默认不允许时间戳回滚  [AUTO-TRANSLATED:0163ff03]
    // Default does not allow timestamp rollback
    bool _enable_rollback = false;
    int64_t _relative_stamp = 0;
    int64_t _last_dts_in = 0;
    int64_t _last_dts_out = 0;
    int64_t _last_pts_out = 0;
    toolkit::SmoothTicker _ticker;
    Stamp *_sync_master = nullptr;
};

// dts生成器，  [AUTO-TRANSLATED:d8a794a2]
// dts generator,
// pts排序后就是dts  [AUTO-TRANSLATED:439ac368]
// pts after sorting is dts
class DtsGenerator{
public:
    bool getDts(uint64_t pts, uint64_t &dts);

private:
    bool getDts_l(uint64_t pts, uint64_t &dts);

private:
    uint64_t _dts_pts_offset = 0;
    uint64_t _last_dts = 0;
    uint64_t _last_pts = 0;
    uint64_t _last_max_pts = 0;
    size_t _frames_since_last_max_pts = 0;
    size_t _sorter_max_size = 0;
    size_t _count_sorter_max_size = 0;
    std::set<uint64_t> _pts_sorter;
};

class NtpStamp {
public:
    void setNtpStamp(uint32_t rtp_stamp, uint64_t ntp_stamp_ms);
    uint64_t getNtpStamp(uint32_t rtp_stamp, uint32_t sample_rate);

private:
    void update(uint32_t rtp_stamp, uint64_t ntp_stamp_us);
    uint64_t getNtpStampUS(uint32_t rtp_stamp, uint32_t sample_rate);

private:
    uint32_t _last_rtp_stamp = 0;
    uint64_t _last_ntp_stamp_us = 0;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_STAMP_H
