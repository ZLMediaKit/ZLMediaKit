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

#include "Stamp.h"

namespace mediakit {

int64_t DeltaStamp::deltaStamp(int64_t stamp) {
    if(!_last_stamp){
        //第一次计算时间戳增量,时间戳增量为0
        _last_stamp = stamp;
        return 0;
    }

    int64_t ret = stamp - _last_stamp;
    if(ret >= 0){
        //时间戳增量为正，返回之
        _last_stamp = stamp;
        return ret;
    }

    //时间戳增量为负，说明时间戳回环了或回退了
    _last_stamp = stamp;
    return _playback ? ret : 0;
}

void DeltaStamp::setPlayBack(bool playback) {
    _playback = playback;
}

void Stamp::revise(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp) {
    if(!pts){
        //没有播放时间戳,使其赋值为解码时间戳
        pts = dts;
    }

    //pts和dts的差值
    int pts_dts_diff = pts - dts;

    //相对时间戳
    _relativeStamp += deltaStamp(modifyStamp ? _ticker.elapsedTime() : dts);
    dts_out = _relativeStamp;

    //////////////以下是播放时间戳的计算//////////////////
    if(pts_dts_diff > 200 || pts_dts_diff < -200){
        //如果差值大于200毫秒，则认为由于回环导致时间戳错乱了
        pts_dts_diff = 0;
    }

    pts_out = dts_out + pts_dts_diff;
    if(pts_out < 0){
        //时间戳不能小于0
        pts_out = 0;
    }
}

void Stamp::setRelativeStamp(int64_t relativeStamp) {
    _relativeStamp = relativeStamp;
}

int64_t Stamp::getRelativeStamp() const {
    return _relativeStamp;
}


}//namespace mediakit