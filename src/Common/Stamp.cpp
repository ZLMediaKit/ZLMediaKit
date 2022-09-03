/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Stamp.h"

//时间戳最大允许跳变3秒，主要是防止网络抖动导致的跳变
#define MAX_DELTA_STAMP (3 * 1000)
#define STAMP_LOOP_DELTA (60 * 1000)
#define MAX_CTS 500
#define ABS(x) ((x) > 0 ? (x) : (-x))

using namespace toolkit;

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
    //如果时间戳回退不多，那么返回负值
    return -ret < MAX_CTS ? ret : 0;
}

void Stamp::setPlayBack(bool playback) {
    _playback = playback;
}

void Stamp::syncTo(Stamp &other){
    _sync_master = &other;
}

//限制dts回退
void Stamp::revise(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp) {
    revise_l(dts, pts, dts_out, pts_out, modifyStamp);
    if (_playback) {
        //回放允许时间戳回退
        return;
    }

    if (dts_out < _last_dts_out) {
//        WarnL << "dts回退:" << dts_out << " < " << _last_dts_out;
        dts_out = _last_dts_out;
        pts_out = _last_pts_out;
        return;
    }
    _last_dts_out = dts_out;
    _last_pts_out = pts_out;
}

//音视频时间戳同步
void Stamp::revise_l(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp) {
    revise_l2(dts, pts, dts_out, pts_out, modifyStamp);
    if (!_sync_master || modifyStamp || _playback) {
        //自动生成时间戳或回放或同步完毕
        return;
    }

    if (_sync_master && _sync_master->_last_dts_in) {
        //音视频dts当前时间差
        int64_t dts_diff = _last_dts_in - _sync_master->_last_dts_in;
        if (ABS(dts_diff) < 5000) {
            //如果绝对时间戳小于5秒，那么说明他们的起始时间戳是一致的，那么强制同步
            _relative_stamp = _sync_master->_relative_stamp + dts_diff;
        }
        //下次不用再强制同步
        _sync_master = nullptr;
    }
}

//求取相对时间戳
void Stamp::revise_l2(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out,bool modifyStamp) {
    if (!pts) {
        //没有播放时间戳,使其赋值为解码时间戳
        pts = dts;
    }

    if (_playback) {
        //这是点播
        dts_out = dts;
        pts_out = pts;
        _relative_stamp = dts_out;
        _last_dts_in = dts;
        return;
    }

    //pts和dts的差值
    int64_t pts_dts_diff = pts - dts;

    if (_last_dts_in != dts) {
        //时间戳发生变更
        if (modifyStamp) {
            //内部自己生产时间戳
            _relative_stamp = _ticker.elapsedTime();
        } else {
            _relative_stamp += deltaStamp(dts);
        }
        _last_dts_in = dts;
    }
    dts_out = _relative_stamp;

    //////////////以下是播放时间戳的计算//////////////////
    if (ABS(pts_dts_diff) > MAX_CTS) {
        //如果差值太大，则认为由于回环导致时间戳错乱了
        pts_dts_diff = 0;
    }

    pts_out = dts_out + pts_dts_diff;
}

void Stamp::setRelativeStamp(int64_t relativeStamp) {
    _relative_stamp = relativeStamp;
}

int64_t Stamp::getRelativeStamp() const {
    return _relative_stamp;
}

bool DtsGenerator::getDts(uint64_t pts, uint64_t &dts){
    bool ret = false;
    if (pts == _last_pts) {
        //pts未变，说明dts也不会变，返回上次dts
        if (_last_dts) {
            dts = _last_dts;
            ret = true;
        }
    } else {
        //pts变了，尝试计算dts
        ret = getDts_l(pts, dts);
        if (ret) {
            //获取到了dts，保存本次结果
            _last_dts = dts;
        }
    }

    if (!ret) {
        //pts排序列队长度还不知道，也就是不知道有没有B帧，
        //那么先强制dts == pts，这样可能导致有B帧的情况下，起始画面有几帧回退
        dts = pts;
    }

    //记录上次pts
    _last_pts = pts;
    return ret;
}

//该算法核心思想是对pts进行排序，排序好的pts就是dts。
//排序有一定的滞后性，那么需要加上排序导致的时间戳偏移量
bool DtsGenerator::getDts_l(uint64_t pts, uint64_t &dts){
    if(_sorter_max_size == 1){
        //没有B帧，dts就等于pts
        dts = pts;
        return true;
    }

    if(!_sorter_max_size){
        //尚未计算出pts排序列队长度(也就是P帧间B帧个数)
        if(pts > _last_max_pts){
            //pts时间戳增加了，那么说明这帧画面不是B帧(说明是P帧或关键帧)
            if(_frames_since_last_max_pts && _count_sorter_max_size++ > 0){
                //已经出现多次非B帧的情况，那么我们就能知道P帧间B帧的个数
                _sorter_max_size = _frames_since_last_max_pts;
                //我们记录P帧间时间间隔(也就是多个B帧时间戳增量累计)
                _dts_pts_offset = (pts - _last_max_pts);
                //除以2，防止dts大于pts
                _dts_pts_offset /= 2;
            }
            //遇到P帧或关键帧，连续B帧计数清零
            _frames_since_last_max_pts = 0;
            //记录上次非B帧的pts时间戳(同时也是dts)，用于统计连续B帧时间戳增量
            _last_max_pts = pts;
        }
        //如果pts时间戳小于上一个P帧，那么断定这个是B帧,我们记录B帧连续个数
        ++_frames_since_last_max_pts;
    }

    //pts放入排序缓存列队，缓存列队最大等于连续B帧个数
    _pts_sorter.emplace(pts);

    if(_sorter_max_size && _pts_sorter.size() >  _sorter_max_size){
        //如果启用了pts排序(意味着存在B帧)，并且pts排序缓存列队长度大于连续B帧个数，
        //意味着后续的pts都会比最早的pts大，那么说明可以取出最早的pts了，这个pts将当做该帧的dts基准
        auto it = _pts_sorter.begin();

        //由于该pts是前面偏移了个_sorter_max_size帧的pts(也就是那帧画面的dts),
        //那么我们加上时间戳偏移量，基本等于该帧的dts
        dts = *it + _dts_pts_offset;
        if(dts > pts){
            //dts不能大于pts(基本不可能到达这个逻辑)
            dts = pts;
        }

        //pts排序缓存出列
        _pts_sorter.erase(it);
        return true;
    }

    //排序缓存尚未满
    return false;
}

void NtpStamp::setNtpStamp(uint32_t rtp_stamp, uint64_t ntp_stamp_ms) {
    update(rtp_stamp, ntp_stamp_ms);
}

void NtpStamp::update(uint32_t rtp_stamp, uint64_t ntp_stamp_ms) {
    if (ntp_stamp_ms == 0) {
        //实测发现有些rtsp服务器发送的rtp时间戳和ntp时间戳一直为0
        return;
    }
    _last_rtp_stamp = rtp_stamp;
    _last_ntp_stamp_ms = ntp_stamp_ms;
}

uint64_t NtpStamp::getNtpStamp(uint32_t rtp_stamp, uint32_t sample_rate) {
    if (rtp_stamp == _last_rtp_stamp) {
        return _last_ntp_stamp_ms;
    }
    return getNtpStamp_l(rtp_stamp, sample_rate);
}

uint64_t NtpStamp::getNtpStamp_l(uint32_t rtp_stamp, uint32_t sample_rate) {
    if (!_last_ntp_stamp_ms) {
        //尚未收到sender report rtcp包，那么赋值为本地系统时间戳吧
        update(rtp_stamp, getCurrentMillisecond(true));
    }

    //rtp时间戳正增长
    if (rtp_stamp >= _last_rtp_stamp) {
        auto diff = static_cast<int>((rtp_stamp - _last_rtp_stamp) / (sample_rate / 1000.0f));
        if (diff < MAX_DELTA_STAMP) {
            //时间戳正常增长
            update(rtp_stamp, _last_ntp_stamp_ms + diff);
            return _last_ntp_stamp_ms;
        }

        //时间戳大幅跳跃
        uint64_t loop_delta = STAMP_LOOP_DELTA * sample_rate / 1000;
        if (_last_rtp_stamp < loop_delta && rtp_stamp > UINT32_MAX - loop_delta) {
            //应该是rtp时间戳溢出+乱序
            uint64_t max_rtp_ms = uint64_t(UINT32_MAX) * 1000 / sample_rate;
            return _last_ntp_stamp_ms + diff - max_rtp_ms;
        }
        //不明原因的时间戳大幅跳跃，直接返回上次值
        WarnL << "rtp stamp abnormal increased:" << _last_rtp_stamp << " -> " << rtp_stamp;
        update(rtp_stamp, _last_ntp_stamp_ms);
        return _last_ntp_stamp_ms;
    }

    //rtp时间戳负增长
    auto diff = static_cast<int>((_last_rtp_stamp - rtp_stamp) / (sample_rate / 1000.0f));
    if (diff < MAX_DELTA_STAMP) {
        //正常范围的时间戳回退，说明收到rtp乱序了
        return _last_ntp_stamp_ms - diff;
    }

    //时间戳大幅度回退
    uint64_t loop_delta = STAMP_LOOP_DELTA * sample_rate / 1000;
    if (rtp_stamp < loop_delta && _last_rtp_stamp > UINT32_MAX - loop_delta) {
        //确定是时间戳溢出
        uint64_t max_rtp_ms = uint64_t(UINT32_MAX) * 1000 / sample_rate;
        update(rtp_stamp, _last_ntp_stamp_ms + (max_rtp_ms - diff));
        return _last_ntp_stamp_ms;
    }
    //不明原因的时间戳回退，直接返回上次值
    WarnL << "rtp stamp abnormal reduced:" << _last_rtp_stamp << " -> " << rtp_stamp;
    update(rtp_stamp, _last_ntp_stamp_ms);
    return _last_ntp_stamp_ms;
}

}//namespace mediakit
