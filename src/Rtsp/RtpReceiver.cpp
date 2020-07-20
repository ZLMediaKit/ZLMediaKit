/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Common/config.h"
#include "RtpReceiver.h"

#define POP_HEAD(trackidx) \
        auto it = _rtp_sort_cache_map[trackidx].begin(); \
        onRtpSorted(it->second, trackidx); \
        _rtp_sort_cache_map[trackidx].erase(it);

#define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])

#define RTP_MAX_SIZE (10 * 1024)

namespace mediakit {

RtpReceiver::RtpReceiver() {}
RtpReceiver::~RtpReceiver() {}

bool RtpReceiver::handleOneRtp(int track_index, TrackType type, int samplerate, unsigned char *rtp_raw_ptr, unsigned int rtp_raw_len) {
    if(rtp_raw_len < 12){
        WarnL << "rtp包太小:" << rtp_raw_len;
        return false;
    }

    uint8_t padding = 0;
    if (rtp_raw_ptr[0] & 0x20) {
        //获取padding大小
        padding = rtp_raw_ptr[rtp_raw_len - 1];
        //移除padding flag
        rtp_raw_ptr[0] &= ~0x20;
        //移除padding字节
        rtp_raw_len -= padding;
    }

    auto rtp_ptr = _rtp_pool.obtain();
    auto &rtp = *rtp_ptr;

    rtp.type = type;
    rtp.interleaved = 2 * type;
    rtp.mark = rtp_raw_ptr[1] >> 7;
    rtp.PT = rtp_raw_ptr[1] & 0x7F;

    //序列号,内存对齐
    memcpy(&rtp.sequence, rtp_raw_ptr + 2, 2);
    rtp.sequence = ntohs(rtp.sequence);

    //时间戳,内存对齐
    memcpy(&rtp.timeStamp, rtp_raw_ptr + 4, 4);
    rtp.timeStamp = ntohl(rtp.timeStamp);

    if(!samplerate){
        //无法把时间戳转换成毫秒
        return false;
    }
    //时间戳转换成毫秒
    rtp.timeStamp = rtp.timeStamp * 1000LL / samplerate;

    //ssrc,内存对齐
    memcpy(&rtp.ssrc, rtp_raw_ptr + 8, 4);
    rtp.ssrc = ntohl(rtp.ssrc);

    if (_ssrc[track_index] != rtp.ssrc) {
        if (_ssrc[track_index] == 0) {
            //保存SSRC至track对象
            _ssrc[track_index] = rtp.ssrc;
        }else{
            //ssrc错误
            WarnL << "ssrc错误:" << rtp.ssrc << " != " << _ssrc[track_index];
            if (_ssrc_err_count[track_index]++ > 10) {
                //ssrc切换后清除老数据
                WarnL << "ssrc更换:" << _ssrc[track_index] << " -> " << rtp.ssrc;
                _rtp_sort_cache_map[track_index].clear();
                _ssrc[track_index] = rtp.ssrc;
            }
            return false;
        }
    }

    //ssrc匹配正确，不匹配计数清零
    _ssrc_err_count[track_index] = 0;

    //获取rtp中媒体数据偏移量
    rtp.offset 	= 12 + 4;
    int csrc     	= rtp_raw_ptr[0] & 0x0f;
    int ext      	= rtp_raw_ptr[0] & 0x10;
    rtp.offset 	+= 4 * csrc;
    if (ext && rtp_raw_len >= rtp.offset) {
        /* calculate the header extension length (stored as number of 32-bit words) */
        ext = (AV_RB16(rtp_raw_ptr + rtp.offset - 2) + 1) << 2;
        rtp.offset += ext;
    }

    if(rtp_raw_len + 4 <= rtp.offset){
        WarnL << "无有效负载的rtp包:" << rtp_raw_len << " <= " << (int)rtp.offset;
        return false;
    }

    if(rtp_raw_len > RTP_MAX_SIZE){
        WarnL << "超大的rtp包:" << rtp_raw_len << " > " << RTP_MAX_SIZE;
        return false;
    }

    //设置rtp负载长度
    rtp.setCapacity(rtp_raw_len + 4);
    rtp.setSize(rtp_raw_len + 4);
    uint8_t *payload_ptr = (uint8_t *)rtp.data();
    payload_ptr[0] = '$';
    payload_ptr[1] = rtp.interleaved;
    payload_ptr[2] = rtp_raw_len >> 8;
    payload_ptr[3] = (rtp_raw_len & 0x00FF);
    //拷贝rtp负载
    memcpy(payload_ptr + 4, rtp_raw_ptr, rtp_raw_len);
    //排序rtp
    sortRtp(rtp_ptr,track_index);
    return true;
}

void RtpReceiver::sortRtp(const RtpPacket::Ptr &rtp,int track_index){
    if(rtp->sequence != _last_seq[track_index] + 1 && _last_seq[track_index] != 0){
        //包乱序或丢包
        _seq_ok_count[track_index] = 0;
        _sort_started[track_index] = true;
        if(_last_seq[track_index] > rtp->sequence && _last_seq[track_index] - rtp->sequence > 0xFF){
            //sequence回环，清空所有排序缓存
            while (_rtp_sort_cache_map[track_index].size()) {
                POP_HEAD(track_index)
            }
            ++_seq_cycle_count[track_index];
        }
    }else{
        //正确序列的包
        _seq_ok_count[track_index]++;
    }

    _last_seq[track_index] = rtp->sequence;

    //开始排序缓存
    if (_sort_started[track_index]) {
        _rtp_sort_cache_map[track_index].emplace(rtp->sequence, rtp);
        GET_CONFIG(uint32_t,clearCount,Rtp::kClearCount);
        GET_CONFIG(uint32_t,maxRtpCount,Rtp::kMaxRtpCount);
        if (_seq_ok_count[track_index] >= clearCount) {
            //网络环境改善，需要清空排序缓存
            _seq_ok_count[track_index] = 0;
            _sort_started[track_index] = false;
            while (_rtp_sort_cache_map[track_index].size()) {
                POP_HEAD(track_index)
            }
        } else if (_rtp_sort_cache_map[track_index].size() >= maxRtpCount) {
            //排序缓存溢出
            POP_HEAD(track_index)
        }
    }else{
        //正确序列
        onRtpSorted(rtp, track_index);
    }
}

void RtpReceiver::clear() {
    CLEAR_ARR(_last_seq);
    CLEAR_ARR(_ssrc);
    CLEAR_ARR(_ssrc_err_count);
    CLEAR_ARR(_seq_ok_count);
    CLEAR_ARR(_sort_started);
    CLEAR_ARR(_seq_cycle_count);

    _rtp_sort_cache_map[0].clear();
    _rtp_sort_cache_map[1].clear();
}

void RtpReceiver::setPoolSize(int size) {
    _rtp_pool.setSize(size);
}

int RtpReceiver::getJitterSize(int track_index){
    return _rtp_sort_cache_map[track_index].size();
}

int RtpReceiver::getCycleCount(int track_index){
    return _seq_cycle_count[track_index];
}


}//namespace mediakit
