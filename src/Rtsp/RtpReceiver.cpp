/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Common/config.h"
#include "RtpReceiver.h"

#define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])

#define RTP_MAX_SIZE (10 * 1024)

namespace mediakit {

RtpReceiver::RtpReceiver() {
    int index = 0;
    for (auto &sortor : _rtp_sortor) {
        sortor.setOnSort([this, index](uint16_t seq, RtpPacket::Ptr &packet) {
            onRtpSorted(packet, index);
        });
        ++index;
    }
}
RtpReceiver::~RtpReceiver() {}

bool RtpReceiver::handleOneRtp(int track_index, TrackType type, int samplerate, unsigned char *rtp_raw_ptr, size_t rtp_raw_len) {
    if (rtp_raw_len < 12) {
        WarnL << "rtp包太小:" << rtp_raw_len;
        return false;
    }

    uint32_t version = rtp_raw_ptr[0] >> 6;
    uint8_t padding = 0;
    uint8_t ext = rtp_raw_ptr[0] & 0x10;
    uint8_t csrc = rtp_raw_ptr[0] & 0x0f;

    if (rtp_raw_ptr[0] & 0x20) {
        //获取padding大小
        padding = rtp_raw_ptr[rtp_raw_len - 1];
        //移除padding flag
        rtp_raw_ptr[0] &= ~0x20;
        //移除padding字节
        rtp_raw_len -= padding;
    }

    if (version != 2) {
        throw std::invalid_argument("非法的rtp，version != 2");
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

    if (!samplerate) {
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
        } else {
            //ssrc错误
            WarnL << "ssrc错误:" << rtp.ssrc << " != " << _ssrc[track_index];
            if (_ssrc_err_count[track_index]++ > 10) {
                //ssrc切换后清除老数据
                WarnL << "ssrc更换:" << _ssrc[track_index] << " -> " << rtp.ssrc;
                _rtp_sortor[track_index].clear();
                _ssrc[track_index] = rtp.ssrc;
            }
            return false;
        }
    }

    //ssrc匹配正确，不匹配计数清零
    _ssrc_err_count[track_index] = 0;

    //rtp 12个固定字节头
    rtp.offset = 12;
    //rtp有csrc
    rtp.offset += 4 * csrc;
    if (ext) {
        //rtp有ext
        uint16_t reserved = AV_RB16(rtp_raw_ptr + rtp.offset);
        uint16_t extlen = AV_RB16(rtp_raw_ptr + rtp.offset + 2) << 2;
        rtp.offset += extlen + 4;
    }

    if (rtp_raw_len <= rtp.offset) {
        //无有效负载的rtp包
        return false;
    }

    if (rtp_raw_len > RTP_MAX_SIZE) {
        WarnL << "超大的rtp包:" << rtp_raw_len << " > " << RTP_MAX_SIZE;
        return false;
    }

    //设置rtp负载长度
    rtp.setCapacity(rtp_raw_len + 4);
    rtp.setSize(rtp_raw_len + 4);
    uint8_t *payload_ptr = (uint8_t *) rtp.data();
    payload_ptr[0] = '$';
    payload_ptr[1] = rtp.interleaved;
    payload_ptr[2] = (rtp_raw_len >> 8) & 0xFF;
    payload_ptr[3] = rtp_raw_len & 0xFF;
    //添加rtp over tcp前4个字节的偏移量
    rtp.offset += 4;
    //拷贝rtp负载
    memcpy(payload_ptr + 4, rtp_raw_ptr, rtp_raw_len);
    //排序rtp
    auto seq = rtp_ptr->sequence;
    _rtp_sortor[track_index].sortPacket(seq, std::move(rtp_ptr));
    return true;
}

void RtpReceiver::clear() {
    CLEAR_ARR(_ssrc);
    CLEAR_ARR(_ssrc_err_count);
    for (auto &sortor : _rtp_sortor) {
        sortor.clear();
    }
}

void RtpReceiver::setPoolSize(size_t size) {
    _rtp_pool.setSize(size);
}

size_t RtpReceiver::getJitterSize(int track_index) const{
    return _rtp_sortor[track_index].getJitterSize();
}

size_t RtpReceiver::getCycleCount(int track_index) const{
    return _rtp_sortor[track_index].getCycleCount();
}

uint32_t RtpReceiver::getSSRC(int track_index) const{
    return _ssrc[track_index];
}

}//namespace mediakit
