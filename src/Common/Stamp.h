/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ZLMEDIAKIT_STAMP_H
#define ZLMEDIAKIT_STAMP_H

#include "Util/TimeTicker.h"
#include <cstdint>
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

    /**
     * 设置是否为回放模式，回放模式运行时间戳回退
     * @param playback 是否为回放模式
     */
    void setPlayBack(bool playback = true);
private:
    int64_t _last_stamp = 0;
    bool _playback = false;
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
private:
    int64_t _relativeStamp = 0;
    int64_t _last_dts = -1;
    SmoothTicker _ticker;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_STAMP_H
