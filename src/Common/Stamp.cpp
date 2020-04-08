/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Stamp.h"

#define MAX_DELTA_STAMP 1000
#define MAX_CTS 500
#define ABS(x) ((x) > 0 ? (x) : (-x))

namespace mediakit {

int64_t DeltaStamp::deltaStamp(int64_t stamp) {
    if(!_last_stamp){
        //第一次计算时间戳增量,时间戳增量为0
        if(stamp){
            _last_stamp = stamp;
        }
        return 0;
    }

    int64_t ret = stamp - _last_stamp;
    if(ret >= 0){
        //时间戳增量为正，返回之
        _last_stamp = stamp;
        //在直播情况下，时间戳增量不得大于MAX_DELTA_STAMP
        return  ret < MAX_DELTA_STAMP ? ret : 0;
    }

    //时间戳增量为负，说明时间戳回环了或回退了
    _last_stamp = stamp;
    return 0;
}

void Stamp::setPlayBack(bool playback) {
    _playback = playback;
}

void Stamp::revise(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp) {
    if(!pts){
        //没有播放时间戳,使其赋值为解码时间戳
        pts = dts;
    }

    if(_playback){
        //这是点播
        dts_out = dts;
        pts_out = pts;
        _relativeStamp = dts_out;
        return;
    }

    //pts和dts的差值
    int pts_dts_diff = pts - dts;

    if(_last_dts != dts){
        //时间戳发生变更
        if(modifyStamp){
            _relativeStamp = _ticker.elapsedTime();
        }else{
            _relativeStamp += deltaStamp(dts);
        }
        _last_dts = dts;
    }
    dts_out = _relativeStamp;

    //////////////以下是播放时间戳的计算//////////////////
    if(ABS(pts_dts_diff) > MAX_CTS){
        //如果差值太大，则认为由于回环导致时间戳错乱了
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


bool DtsGenerator::getDts(uint32_t pts, uint32_t &dts){
    bool ret = false;
    if(pts == _last_pts){
        //pts未变，返回上次结果
        if(_last_dts){
            dts = _last_dts;
            ret = true;
        }
        return ret;
    }

    ret = getDts_l(pts,dts);
    if(ret){
        //保存本次结果
        _last_dts = dts;
    }
    //记录上次pts
    _last_pts = pts;
    return ret;
}

bool DtsGenerator::getDts_l(uint32_t pts, uint32_t &dts){
    if(_sorter_max_size == 1){
        //没有B帧
        dts = pts;
        return true;
    }

    if(!_sorter_max_size){
        if(pts > _last_max_pts){
            if(_frames_since_last_max_pts && _count_sorter_max_size++ > 0){
                _sorter_max_size = _frames_since_last_max_pts;
                _dts_pts_offset = (pts - _last_max_pts) / 2;
            }
            _frames_since_last_max_pts = 0;
            _last_max_pts = pts;
        }
        ++_frames_since_last_max_pts;
    }

    _pts_sorter.emplace(pts);
    if(_sorter_max_size && _pts_sorter.size() >  _sorter_max_size){
        auto it = _pts_sorter.begin();
        dts = *it + _dts_pts_offset;
        if(dts > pts){
            //dts不能大于pts(基本不可能到达这个逻辑)
            dts = pts;
        }
        _pts_sorter.erase(it);
        return true;
    }
    return false;
}

}//namespace mediakit