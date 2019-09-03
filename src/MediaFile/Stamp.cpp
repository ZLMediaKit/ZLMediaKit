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

void Stamp::revise(uint32_t dts, uint32_t pts, int64_t &dts_out, int64_t &pts_out) {
    if(!pts){
        //没有播放时间戳,使其赋值为解码时间戳
        pts = dts;
    }
    //pts和dts的差值
    int pts_dts_diff = pts - dts;

    if(_first){
        //记录第一次时间戳，后面好计算时间戳增量
        _start_dts = dts;
        _first = false;
        _ticker.resetTime();
    }
    if (!dts) {
        //没有解码时间戳，我们生成解码时间戳
        dts = _ticker.elapsedTime();
    }

    //相对时间戳
    dts_out = dts - _start_dts;
    if(dts_out < _dts_inc && !_playback){
        //本次相对时间戳竟然小于上次？
        if(dts_out < 0 || _dts_inc - dts_out > 0xFFFF){
            //时间戳回环,保证下次相对时间戳与本次相对合理增长
            _start_dts = dts - _dts_inc;
            //本次时间戳强制等于上次时间戳
            dts_out = _dts_inc;
        }else{
            //时间戳变小了？,那么取上次时间戳
            dts_out = _dts_inc;
        }
    }

    //保留这次相对时间戳，以便下次对比是否回环或乱序
    _dts_inc = dts_out;

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

void Stamp::setPlayBack(bool playback) {
    _playback = playback;
}

}//namespace mediakit