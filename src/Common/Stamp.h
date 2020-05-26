/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_STAMP_H
#define ZLMEDIAKIT_STAMP_H

#include <set>
#include <cstdint>
#include "Util/TimeTicker.h"
using namespace toolkit;

namespace mediakit {

class DeltaStamp{
public:
    DeltaStamp() = default;
    ~DeltaStamp() = default;

    /**
     * 计算时间戳增量
     * @param stamp 绝对时间戳
     * @return 时间戳增量
     */
    int64_t deltaStamp(int64_t stamp);
private:
    int64_t _last_stamp = 0;
};

//该类解决时间戳回环、回退问题
//计算相对时间戳或者产生平滑时间戳
class Stamp : public DeltaStamp{
public:
    Stamp() = default;
    ~Stamp() = default;

    /**
     * 修正时间戳
     * @param dts 输入dts，如果为0则根据系统时间戳生成
     * @param pts 输入pts，如果为0则等于dts
     * @param dts_out 输出dts
     * @param pts_out 输出pts
     * @param modifyStamp 是否用系统时间戳覆盖
     */
    void revise(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp = false);

    /**
     * 再设置相对时间戳，用于seek用
     * @param relativeStamp 相对时间戳
     */
    void setRelativeStamp(int64_t relativeStamp);

    /**
     * 获取当前相对时间戳
     * @return
     */
    int64_t getRelativeStamp() const ;

    /**
     * 设置是否为回放模式，回放模式运行时间戳回退
     * @param playback 是否为回放模式
     */
    void setPlayBack(bool playback = true);

    /**
     * 音视频同步用，音频应该同步于视频(只修改音频时间戳)
     * 因为音频时间戳修改后不影响播放速度
     */
    void syncTo(Stamp &other);

private:
    void revise_l(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp = false);
private:
    int64_t _relativeStamp = 0;
    int64_t _last_relativeStamp = 0;
    int64_t _last_dts = 0;
    SmoothTicker _ticker;
    bool _playback = false;
    Stamp *_sync_master = nullptr;
    bool _sync_finished = true;
};

//dts生成器，
//pts排序后就是dts
class DtsGenerator{
public:
    DtsGenerator() = default;
    ~DtsGenerator() = default;
    bool getDts(uint32_t pts, uint32_t &dts);
private:
    bool getDts_l(uint32_t pts, uint32_t &dts);
private:
    uint32_t _dts_pts_offset = 0;
    uint32_t _last_dts = 0;
    uint32_t _last_pts = 0;
    uint32_t _last_max_pts = 0;
    int _frames_since_last_max_pts = 0;
    int _sorter_max_size = 0;
    int _count_sorter_max_size = 0;
    set<uint32_t> _pts_sorter;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_STAMP_H
