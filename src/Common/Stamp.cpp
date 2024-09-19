/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Stamp.h"

// 时间戳最大允许跳变3秒，主要是防止网络抖动导致的跳变  [AUTO-TRANSLATED:144154de]
// Timestamp maximum allowable jump is 3 seconds, mainly to prevent network jitter caused by the jump
#define MAX_DELTA_STAMP (3 * 1000)
#define STAMP_LOOP_DELTA (60 * 1000)
#define MAX_CTS 500
#define ABS(x) ((x) > 0 ? (x) : (-x))

using namespace toolkit;

namespace mediakit {

DeltaStamp::DeltaStamp() {
    // 时间戳最大允许跳跃300ms  [AUTO-TRANSLATED:2458e61f]
    // Timestamp maximum allowable jump is 300ms
    _max_delta = 300;
}

int64_t DeltaStamp::relativeStamp(int64_t stamp, bool enable_rollback) {
    _relative_stamp += deltaStamp(stamp, enable_rollback);
    return _relative_stamp;
}

int64_t DeltaStamp::relativeStamp() {
    return _relative_stamp;
}

int64_t DeltaStamp::deltaStamp(int64_t stamp, bool enable_rollback) {
    if (!_last_stamp) {
        // 第一次计算时间戳增量,时间戳增量为0  [AUTO-TRANSLATED:32944bd3]
        // Calculate the timestamp increment for the first time, the timestamp increment is 0
        if (stamp) {
            _last_stamp = stamp;
        }
        return 0;
    }

    int64_t ret = stamp - _last_stamp;
    if (ret >= 0) {
        // 时间戳增量为正，返回之  [AUTO-TRANSLATED:308dfb22]
        // The timestamp increment is positive, return it
        _last_stamp = stamp;
        // 在直播情况下，时间戳增量不得大于MAX_DELTA_STAMP，否则强制相对时间戳加1  [AUTO-TRANSLATED:c78c40d3]
        // In the live broadcast case, the timestamp increment must not be greater than MAX_DELTA_STAMP, otherwise the relative timestamp is forced to add 1
        if (ret > _max_delta) {
            needSync();
            return 1;
        }
        return ret;
    }

    // 时间戳增量为负，说明时间戳回环了或回退了  [AUTO-TRANSLATED:fd825d50]
    // The timestamp increment is negative, indicating that the timestamp has looped or retreated
    _last_stamp = stamp;
    if (!enable_rollback || -ret > _max_delta) {
        // 不允许回退或者回退太多了, 强制时间戳加1  [AUTO-TRANSLATED:152f5ffa]
        // Not allowed to retreat or retreat too much, force the timestamp to add 1
        needSync();
        return 1;
    }
    return ret;
}

void DeltaStamp::setMaxDelta(size_t max_delta) {
    _max_delta = max_delta;
}

void Stamp::setPlayBack(bool playback) {
    _playback = playback;
}

void Stamp::syncTo(Stamp &other) {
    _need_sync = true;
    _sync_master = &other;
}

void Stamp::needSync() {
    _need_sync = true;
}

void Stamp::enableRollback(bool flag) {
    _enable_rollback = flag;
}

// 限制dts回退  [AUTO-TRANSLATED:6bc53b31]
// Limit dts retreat
void Stamp::revise(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out, bool modifyStamp) {
    revise_l(dts, pts, dts_out, pts_out, modifyStamp);
    if (_playback) {
        // 回放允许时间戳回退  [AUTO-TRANSLATED:5d822118]
        // Playback allows timestamp rollback
        return;
    }

    if (dts_out < _last_dts_out) {
        // WarnL << "dts回退:" << dts_out << " < " << _last_dts_out;  [AUTO-TRANSLATED:c36316f5]
        // WarnL << "dts rollback:" << dts_out << " < " << _last_dts_out;
        dts_out = _last_dts_out;
        pts_out = _last_pts_out;
        return;
    }
    _last_dts_out = dts_out;
    _last_pts_out = pts_out;
}

// 音视频时间戳同步  [AUTO-TRANSLATED:58f1e95c]
// Audio and video timestamp synchronization
void Stamp::revise_l(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out, bool modifyStamp) {
    revise_l2(dts, pts, dts_out, pts_out, modifyStamp);
    if (!_sync_master || modifyStamp || _playback) {
        // 自动生成时间戳或回放或同步完毕  [AUTO-TRANSLATED:a0b8f8bd]
        // Automatically generate timestamps or playback or synchronization is complete
        return;
    }

    // 需要同步时间戳  [AUTO-TRANSLATED:af93e8f8]
    // Need to synchronize timestamps
    if (_sync_master && _sync_master->_last_dts_in && (_need_sync || _sync_master->_need_sync)) {
        // 音视频dts当前时间差  [AUTO-TRANSLATED:716468a6]
        // Audio and video dts current time difference
        int64_t dts_diff = _last_dts_in - _sync_master->_last_dts_in;
        if (ABS(dts_diff) < 5000) {
            // 如果绝对时间戳小于5秒，那么说明他们的起始时间戳是一致的，那么强制同步  [AUTO-TRANSLATED:5d11ef6a]
            // If the absolute timestamp is less than 5 seconds, then it means that their starting timestamps are consistent, then force synchronization
            auto target_stamp = _sync_master->_relative_stamp + dts_diff;
            if (target_stamp > _relative_stamp || _enable_rollback) {
                // 强制同步后，时间戳增加跳跃了，或允许回退  [AUTO-TRANSLATED:805424a9]
                // After forced synchronization, the timestamp increases jump, or allows rollback
                TraceL << "Relative stamp changed: " << _relative_stamp << " -> " << target_stamp;
                _relative_stamp = target_stamp;
            } else {
                // 不允许回退, 则让另外一个Track的时间戳增长  [AUTO-TRANSLATED:428e8ce2]
                // Not allowed to rollback, then let the timestamp of the other Track increase
                target_stamp = _relative_stamp - dts_diff;
                TraceL << "Relative stamp changed: " << _sync_master->_relative_stamp << " -> " << target_stamp;
                _sync_master->_relative_stamp = target_stamp;
            }
        }
        _need_sync = false;
        _sync_master->_need_sync = false;
    }
}

// 求取相对时间戳  [AUTO-TRANSLATED:122da805]
// Obtain the relative timestamp
void Stamp::revise_l2(int64_t dts, int64_t pts, int64_t &dts_out, int64_t &pts_out, bool modifyStamp) {
    if (!pts) {
        // 没有播放时间戳,使其赋值为解码时间戳  [AUTO-TRANSLATED:9ee71899]
        // There is no playback timestamp, set it to the decoding timestamp
        pts = dts;
    }

    if (_playback) {
        // 这是点播  [AUTO-TRANSLATED:f11fd173]
        // This is on-demand
        dts_out = dts;
        pts_out = pts;
        _relative_stamp = dts_out;
        _last_dts_in = dts;
        return;
    }

    // pts和dts的差值  [AUTO-TRANSLATED:3b145073]
    // The difference between pts and dts
    int64_t pts_dts_diff = pts - dts;

    if (_last_dts_in != dts) {
        // 时间戳发生变更  [AUTO-TRANSLATED:7344315c]
        // Timestamp changed
        if (modifyStamp) {
            // 内部自己生产时间戳  [AUTO-TRANSLATED:fae889e0]
            // Internal production of timestamps
            _relative_stamp = _ticker.elapsedTime();
        } else {
            _relative_stamp += deltaStamp(dts, _enable_rollback);
        }
        _last_dts_in = dts;
    }
    dts_out = _relative_stamp;

    // ////////////以下是播放时间戳的计算//////////////////  [AUTO-TRANSLATED:6c4a56a7]
    // ////////////The following is the calculation of the playback timestamp//////////////////
    if (ABS(pts_dts_diff) > MAX_CTS) {
        // 如果差值太大，则认为由于回环导致时间戳错乱了  [AUTO-TRANSLATED:1a11b5f3]
        // If the difference is too large, it is considered that the timestamp is messed up due to looping
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

bool DtsGenerator::getDts(uint64_t pts, uint64_t &dts) {
    bool ret = false;
    if (pts == _last_pts) {
        // pts未变，说明dts也不会变，返回上次dts  [AUTO-TRANSLATED:dc0972e0]
        // pts does not change, indicating that dts will not change, return the last dts
        if (_last_dts) {
            dts = _last_dts;
            ret = true;
        }
    } else {
        // pts变了，尝试计算dts  [AUTO-TRANSLATED:f527d0f6]
        // pts changed, try to calculate dts
        ret = getDts_l(pts, dts);
        if (ret) {
            // 获取到了dts，保存本次结果  [AUTO-TRANSLATED:d6a5ce6d]
            // Get the dts, save the current result
            _last_dts = dts;
        }
    }

    if (!ret) {
        // pts排序列队长度还不知道，也就是不知道有没有B帧，  [AUTO-TRANSLATED:e5ad4327]
        // The pts sorting queue length is not yet known, that is, it is not known whether there is a B frame,
        // 那么先强制dts == pts，这样可能导致有B帧的情况下，起始画面有几帧回退  [AUTO-TRANSLATED:74c97de1]
        // Then force dts == pts first, which may cause the starting picture to have a few frames rollback in the case of B frames
        dts = pts;
    }

    // 记录上次pts  [AUTO-TRANSLATED:4ecd474b]
    // Record the last pts
    _last_pts = pts;
    return ret;
}

// 该算法核心思想是对pts进行排序，排序好的pts就是dts。  [AUTO-TRANSLATED:efb36e04]
// The core idea of this algorithm is to sort the pts, and the sorted pts is the dts.
// 排序有一定的滞后性，那么需要加上排序导致的时间戳偏移量  [AUTO-TRANSLATED:5ada843a]
// Sorting has a certain lag, so it is necessary to add the timestamp offset caused by sorting
bool DtsGenerator::getDts_l(uint64_t pts, uint64_t &dts) {
    if (_sorter_max_size == 1) {
        // 没有B帧，dts就等于pts  [AUTO-TRANSLATED:9cfae4ea]
        // There is no B frame, dts is equal to pts
        dts = pts;
        return true;
    }

    if (!_sorter_max_size) {
        // 尚未计算出pts排序列队长度(也就是P帧间B帧个数)  [AUTO-TRANSLATED:8bedb754]
        // The length of the pts sorting queue (that is, the number of B frames between P frames) has not been calculated yet
        if (pts > _last_max_pts) {
            // pts时间戳增加了，那么说明这帧画面不是B帧(说明是P帧或关键帧)  [AUTO-TRANSLATED:4c5ef2b8]
            // The pts timestamp has increased, which means that this frame is not a B frame (it means it is a P frame or a key frame)
            if (_frames_since_last_max_pts && _count_sorter_max_size++ > 0) {
                // 已经出现多次非B帧的情况，那么我们就能知道P帧间B帧的个数  [AUTO-TRANSLATED:fd747b3c]
                // There have been multiple non-B frames, so we can know the number of B frames between P frames
                _sorter_max_size = _frames_since_last_max_pts;
                // 我们记录P帧间时间间隔(也就是多个B帧时间戳增量累计)  [AUTO-TRANSLATED:66c0cc14]
                // We record the time interval between P frames (that is, the cumulative increment of multiple B frame timestamps)
                _dts_pts_offset = (pts - _last_max_pts);
                // 除以2，防止dts大于pts  [AUTO-TRANSLATED:52b5b3ab]
                // Divide by 2 to prevent dts from being greater than pts
                _dts_pts_offset /= 2;
            }
            // 遇到P帧或关键帧，连续B帧计数清零  [AUTO-TRANSLATED:537bb54d]
            // When encountering a P frame or a key frame, the continuous B frame count is cleared
            _frames_since_last_max_pts = 0;
            // 记录上次非B帧的pts时间戳(同时也是dts)，用于统计连续B帧时间戳增量  [AUTO-TRANSLATED:194f8cdb]
            // Record the pts timestamp of the last non-B frame (which is also dts), used to count the continuous B frame timestamp increment
            _last_max_pts = pts;
        }
        // 如果pts时间戳小于上一个P帧，那么断定这个是B帧,我们记录B帧连续个数  [AUTO-TRANSLATED:1a7e33e2]
        // If the pts timestamp is less than the previous P frame, then it is determined that this is a B frame, and we record the number of consecutive B frames
        ++_frames_since_last_max_pts;
    }

    // pts放入排序缓存列队，缓存列队最大等于连续B帧个数  [AUTO-TRANSLATED:ff598a97]
    // Put pts into the sorting cache queue, the maximum cache queue is equal to the number of consecutive B frames
    _pts_sorter.emplace(pts);

    if (_sorter_max_size && _pts_sorter.size() > _sorter_max_size) {
        // 如果启用了pts排序(意味着存在B帧)，并且pts排序缓存列队长度大于连续B帧个数，  [AUTO-TRANSLATED:002c0d03]
        // If pts sorting is enabled (meaning there are B frames), and the length of the pts sorting cache queue is greater than the number of consecutive B frames,
        // 意味着后续的pts都会比最早的pts大，那么说明可以取出最早的pts了，这个pts将当做该帧的dts基准  [AUTO-TRANSLATED:86b8f679]
        // It means that the subsequent pts will be larger than the earliest pts, which means that the earliest pts can be taken out, and this pts will be used as the dts baseline for this frame
        auto it = _pts_sorter.begin();

        // 由于该pts是前面偏移了个_sorter_max_size帧的pts(也就是那帧画面的dts),  [AUTO-TRANSLATED:eb3657aa]
        // Since this pts is the pts of the previous _sorter_max_size frames (that is, the dts of that frame),
        // 那么我们加上时间戳偏移量，基本等于该帧的dts  [AUTO-TRANSLATED:245aac6e]
        // Then we add the timestamp offset, which is basically equal to the dts of this frame
        dts = *it + _dts_pts_offset;
        if (dts > pts) {
            // dts不能大于pts(基本不可能到达这个逻辑)  [AUTO-TRANSLATED:847c4531]
            // dts cannot be greater than pts (it is basically impossible to reach this logic)
            dts = pts;
        }

        // pts排序缓存出列  [AUTO-TRANSLATED:8b580191]
        // pts sorting cache dequeue
        _pts_sorter.erase(it);
        return true;
    }

    // 排序缓存尚未满  [AUTO-TRANSLATED:3f502460]
    // The sorting cache is not full yet
    return false;
}

void NtpStamp::setNtpStamp(uint32_t rtp_stamp, uint64_t ntp_stamp_ms) {
    if (!ntp_stamp_ms || !rtp_stamp) {
        // 实测发现有些rtsp服务器发送的rtp时间戳和ntp时间戳一直为0  [AUTO-TRANSLATED:d3c200fc]
        // It has been found that some rtsp servers send rtp timestamps and ntp timestamps that are always 0
        WarnL << "Invalid sender report rtcp, ntp_stamp_ms = " << ntp_stamp_ms << ", rtp_stamp = " << rtp_stamp;
        return;
    }
    update(rtp_stamp, ntp_stamp_ms * 1000);
}

void NtpStamp::update(uint32_t rtp_stamp, uint64_t ntp_stamp_us) {
    _last_rtp_stamp = rtp_stamp;
    _last_ntp_stamp_us = ntp_stamp_us;
}

uint64_t NtpStamp::getNtpStamp(uint32_t rtp_stamp, uint32_t sample_rate) {
    if (rtp_stamp == _last_rtp_stamp) {
        return _last_ntp_stamp_us / 1000;
    }
    return getNtpStampUS(rtp_stamp, sample_rate) / 1000;
}

uint64_t NtpStamp::getNtpStampUS(uint32_t rtp_stamp, uint32_t sample_rate) {
    if (!_last_ntp_stamp_us) {
        // 尚未收到sender report rtcp包，那么赋值为本地系统时间戳吧  [AUTO-TRANSLATED:c9056069]
        // The sender report rtcp packet has not been received yet, so assign it to the local system timestamp
        update(rtp_stamp, getCurrentMicrosecond(true));
    }

    // rtp时间戳正增长  [AUTO-TRANSLATED:4d3c87d1]
    // The rtp timestamp is increasing
    if (rtp_stamp >= _last_rtp_stamp) {
        auto diff_us = static_cast<int64_t>((rtp_stamp - _last_rtp_stamp) / (sample_rate / 1000000.0f));
        if (diff_us < MAX_DELTA_STAMP * 1000) {
            // 时间戳正常增长  [AUTO-TRANSLATED:db60e84a]
            // The timestamp is increasing normally
            update(rtp_stamp, _last_ntp_stamp_us + diff_us);
            return _last_ntp_stamp_us;
        }

        // 时间戳大幅跳跃  [AUTO-TRANSLATED:c8585a51]
        // The timestamp jumps significantly
        uint64_t loop_delta_hz = STAMP_LOOP_DELTA * sample_rate / 1000;
        if (_last_rtp_stamp < loop_delta_hz && rtp_stamp > UINT32_MAX - loop_delta_hz) {
            // 应该是rtp时间戳溢出+乱序  [AUTO-TRANSLATED:13529fd6]
            // It should be rtp timestamp overflow + out of order
            uint64_t max_rtp_us = uint64_t(UINT32_MAX) * 1000000 / sample_rate;
            return _last_ntp_stamp_us + diff_us - max_rtp_us;
        }
        // 不明原因的时间戳大幅跳跃，直接返回上次值  [AUTO-TRANSLATED:952b769c]
        // The timestamp jumps significantly for unknown reasons, directly return the last value
        WarnL << "rtp stamp abnormal increased:" << _last_rtp_stamp << " -> " << rtp_stamp;
        update(rtp_stamp, _last_ntp_stamp_us);
        return _last_ntp_stamp_us;
    }

    // rtp时间戳负增长  [AUTO-TRANSLATED:54a7f797]
    // The rtp timestamp is decreasing
    auto diff_us = static_cast<int64_t>((_last_rtp_stamp - rtp_stamp) / (sample_rate / 1000000.0f));
    if (diff_us < MAX_DELTA_STAMP * 1000) {
        // 正常范围的时间戳回退，说明收到rtp乱序了  [AUTO-TRANSLATED:f691d5bf]
        // The timestamp retreats within the normal range, indicating that the rtp is out of order
        return _last_ntp_stamp_us - diff_us;
    }

    // 时间戳大幅度回退  [AUTO-TRANSLATED:0ad69100]
    // The timestamp retreats significantly
    uint64_t loop_delta_hz = STAMP_LOOP_DELTA * sample_rate / 1000;
    if (rtp_stamp < loop_delta_hz && _last_rtp_stamp > UINT32_MAX - loop_delta_hz) {
        // 确定是时间戳溢出  [AUTO-TRANSLATED:322274c3]
        // Determine if it is a timestamp overflow
        uint64_t max_rtp_us = uint64_t(UINT32_MAX) * 1000000 / sample_rate;
        update(rtp_stamp, _last_ntp_stamp_us + (max_rtp_us - diff_us));
        return _last_ntp_stamp_us;
    }
    // 不明原因的时间戳回退，直接返回上次值  [AUTO-TRANSLATED:c5105c14]
    // Timestamp rollback for unknown reasons, return the last value directly
    WarnL << "rtp stamp abnormal reduced:" << _last_rtp_stamp << " -> " << rtp_stamp;
    update(rtp_stamp, _last_ntp_stamp_us);
    return _last_ntp_stamp_us;
}

} // namespace mediakit
