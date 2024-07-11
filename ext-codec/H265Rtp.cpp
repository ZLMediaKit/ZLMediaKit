﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H265Rtp.h"
#include "Common/config.h"
namespace mediakit{

//https://datatracker.ietf.org/doc/rfc7798/
//H265 nalu 头两个字节的定义
/*
 0               1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |F|    Type   |  LayerId  | TID |
 +-------------+-----------------+
 Forbidden zero(F) : 1 bit
 NAL unit type(Type) : 6 bits
 NUH layer ID(LayerId) : 6 bits
 NUH temporal ID plus 1 (TID) : 3 bits
*/

H265RtpDecoder::H265RtpDecoder() {
    _frame = obtainFrame();
}

H265Frame::Ptr H265RtpDecoder::obtainFrame() {
    auto frame = FrameImp::create<H265Frame>();
    frame->_prefix_size = 4;
    return frame;
}

#define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])

#define CHECK_SIZE(total, size, ret) \
        if (total < size) {     \
            WarnL << "invalid rtp data size:" << total << " < " << size << ",rtp:\r\n" << rtp->dumpString(); _gop_dropped = true;  return ret; \
        }

// 4.4.2. Aggregation Packets (APs) (p25)
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          RTP Header                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      PayloadHdr (Type=48)     |           NALU 1 DONL         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           NALU 1 Size         |            NALU 1 HDR         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                         NALU 1 Data . . .                     |
|                                                               |
+     . . .     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|               |  NALU 2 DOND  |            NALU 2 Size        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          NALU 2 HDR           |                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            NALU 2 Data        |
|                                                               |
|         . . .                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :    ...OPTIONAL RTP padding    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool H265RtpDecoder::unpackAp(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp){
    bool have_key_frame = false;
    //忽略PayloadHdr
    CHECK_SIZE(size, 2, have_key_frame);
    ptr += 2;
    size -= 2;

    while (size) {
        if (_using_donl_field) {
            CHECK_SIZE(size, 2, have_key_frame);
            uint16_t donl = AV_RB16(ptr);
            size -= 2;
            ptr += 2;
        }
        CHECK_SIZE(size, 2, have_key_frame);
        uint16_t nalu_size = AV_RB16(ptr);
        size -= 2;
        ptr += 2;
        CHECK_SIZE(size, nalu_size, have_key_frame)
        if (singleFrame(rtp, ptr, nalu_size, stamp)) {
            have_key_frame = true;
        }
        size -= nalu_size;
        ptr += nalu_size;
    }
    return have_key_frame;
}

// 4.4.3. Fragmentation Units (p29)
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     PayloadHdr (Type=49)      |    FU header  |  DONL (cond)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
|  DONL (cond)  |                                               |
|-+-+-+-+-+-+-+-+                                               |
|                           FU payload                          |
|                                                               |
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :    ...OPTIONAL RTP padding    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|S|E|   FuType  |
+---------------+
*/

bool H265RtpDecoder::mergeFu(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp, uint16_t seq){
    CHECK_SIZE(size, 4, false);
    auto s_bit = ptr[2] >> 7;
    auto e_bit = (ptr[2] >> 6) & 0x01;
    auto type = ptr[2] & 0x3f;
    if (s_bit) {
        //该帧的第一个rtp包
        _frame->_buffer.assign("\x00\x00\x00\x01", 4);
        _frame->_buffer.push_back((type << 1) | (ptr[0] & 0x81));
        _frame->_buffer.push_back(ptr[1]);
        _frame->_pts = stamp;
        _fu_dropped = false;
    }

    if (_fu_dropped) {
        //该帧不完整
        return false;
    }

    if (!s_bit && seq != (uint16_t) (_last_seq + 1)) {
        //中间的或末尾的rtp包，其seq必须连续，否则说明rtp丢包，那么该帧不完整，必须得丢弃
        _fu_dropped = true;
        _frame->_buffer.clear();
        return false;
    }

    //跳过PayloadHdr +  FU header
    ptr += 3;
    size -= 3;
    if (_using_donl_field) {
        //DONL确保不少于2个字节
        CHECK_SIZE(size, 2, false);
        uint16_t donl = AV_RB16(ptr);
        size -= 2;
        ptr += 2;
    }

    CHECK_SIZE(size, 1, false);

    //后面追加数据
    _frame->_buffer.append((char *) ptr, size);

    if (!e_bit) {
        //非末尾包
        return s_bit ? (_frame->keyFrame() || _frame->configFrame()) : false;
    }

    //确保下一次fu必须收到第一个包
    _fu_dropped = true;
    //该帧最后一个rtp包
    outputFrame(rtp, _frame);
    return false;
}

bool H265RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool) {
    auto seq = rtp->getSeq();
    auto last_is_gop = _is_gop;
    _is_gop = decodeRtp(rtp);
    if (!_gop_dropped && seq != (uint16_t) (_last_seq + 1) && _last_seq) {
        _gop_dropped = true;
        WarnL << "start drop h265 gop, last seq:" << _last_seq << ", rtp:\r\n" << rtp->dumpString();
    }
    _last_seq = seq;
    // 确保有sps rtp的时候，gop从sps开始；否则从关键帧开始
    return _is_gop && !last_is_gop;
}

bool H265RtpDecoder::decodeRtp(const RtpPacket::Ptr &rtp) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= 0) {
        //无实际负载
        return false;
    }
    auto frame = rtp->getPayload();
    auto stamp = rtp->getStampMS();
    auto seq = rtp->getSeq();
    int nal = H265_TYPE(frame[0]);

    switch (nal) {
        case 48:
            // aggregated packet (AP) - with two or more NAL units
            return unpackAp(rtp, frame, payload_size, stamp);

        case 49:
            // fragmentation unit (FU)
            return mergeFu(rtp, frame, payload_size, stamp, seq);

        default: {
            if (nal < 48) {
                // Single NAL Unit Packets (p24)
                return singleFrame(rtp, frame, payload_size, stamp);
            }
            _gop_dropped = true;
            WarnL << "不支持该类型的265 RTP包, nal type" << nal << ", rtp:\r\n" << rtp->dumpString();
            return false;
        }
    }
}

bool H265RtpDecoder::singleFrame(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp){
    _frame->_buffer.assign("\x00\x00\x00\x01", 4);
    _frame->_buffer.append((char *) ptr, size);
    _frame->_pts = stamp;
    auto key = _frame->keyFrame() || _frame->configFrame();
    outputFrame(rtp, _frame);
    return key;
}

void H265RtpDecoder::outputFrame(const RtpPacket::Ptr &rtp, const H265Frame::Ptr &frame) {
    if (frame->dropAble()) {
        //不参与dts生成
        frame->_dts = frame->_pts;
    } else {
        //rtsp没有dts，那么根据pts排序算法生成dts
        _dts_generator.getDts(frame->_pts, frame->_dts);
    }

    if (frame->keyFrame() && _gop_dropped) {
        _gop_dropped = false;
        InfoL << "new gop received, rtp:\r\n" << rtp->dumpString();
    }
    if (!_gop_dropped) {
        RtpCodec::inputFrame(frame);
    }
    _frame = obtainFrame();
}

////////////////////////////////////////////////////////////////////////

void H265RtpEncoder::packRtpFu(const char *ptr, size_t len, uint64_t pts, bool is_mark, bool gop_pos){
    auto max_size = getRtpInfo().getMaxSize() - 3;
    auto nal_type = H265_TYPE(ptr[0]); //获取NALU的5bit 帧类型
    unsigned char s_e_flags;
    bool fu_start = true;
    bool mark_bit = false;
    size_t offset = 2;
    while (!mark_bit) {
        if (len <= offset + max_size) {
            // FU end
            mark_bit = true;
            max_size = len - offset;
            s_e_flags = (1 << 6) | nal_type;
        } else if (fu_start) {
            // FU start
            s_e_flags = (1 << 7) | nal_type;
        } else {
            // FU mid
            s_e_flags = nal_type;
        }

        {
            // 传入nullptr先不做payload的内存拷贝
            auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, max_size + 3, mark_bit, pts);
            // rtp payload 负载部分
            uint8_t *payload = rtp->getPayload();
            // FU 第1个字节，表明为FU
            payload[0] = 49 << 1;
            // FU 第2个字节貌似固定为1
            payload[1] = ptr[1]; // 1;
            // FU 第3个字节
            payload[2] = s_e_flags;
            // H265 数据
            memcpy(payload + 3, ptr + offset, max_size);
            // 输入到rtp环形缓存
            RtpCodec::inputRtp(rtp, fu_start && gop_pos);
        }

        offset += max_size;
        fu_start = false;
    }
}

void H265RtpEncoder::packRtp(const char *ptr, size_t len, uint64_t pts, bool is_mark, bool gop_pos){
    if (len <= getRtpInfo().getMaxSize()) {
        //signal-nalu 
        RtpCodec::inputRtp(getRtpInfo().makeRtp(TrackVideo, ptr, len, is_mark, pts), gop_pos);
    } else {
        //FU-A模式
        packRtpFu(ptr, len, pts, is_mark, gop_pos);
    }
}
void H265RtpEncoder::insertConfigFrame(uint64_t pts){
     if (!_sps || !_pps || !_vps) {
        WarnL<<" not ok";
        return;
    }
    //gop缓存从vps 开始，vps ,sps、pps后面还有时间戳相同的关键帧，所以mark bit为false
    packRtp(_vps->data() + _vps->prefixSize(), _vps->size() - _vps->prefixSize(), pts, false, true);
    packRtp(_sps->data() + _sps->prefixSize(), _sps->size() - _sps->prefixSize(), pts, false, false);
    packRtp(_pps->data() + _pps->prefixSize(), _pps->size() - _pps->prefixSize(), pts, false, false);
    
}
bool H265RtpEncoder::inputFrame_l(const Frame::Ptr &frame, bool is_mark){
     if (frame->keyFrame()) {
        //保证每一个关键帧前都有SPS PPS VPS
        insertConfigFrame(frame->pts());
    }
    packRtp(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize(), frame->pts(), is_mark, false);
    return true;
}
bool H265RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = (uint8_t *) frame->data() + frame->prefixSize();
    auto nal_type = H265_TYPE(ptr[0]); //获取NALU的5bit 帧类型

    switch (nal_type) {
        case H265Frame::NAL_SPS: {
            _sps = Frame::getCacheAbleFrame(frame);
            return true;
        }
        case H265Frame::NAL_PPS: {
            _pps = Frame::getCacheAbleFrame(frame);
            return true;
        }
        case H265Frame::NAL_VPS:{
            _vps = Frame::getCacheAbleFrame(frame);
            return true;
        }
        default: break;
    }

    GET_CONFIG(int,lowLatency,Rtp::kLowLatency);
    if (lowLatency) { // 低延迟模式
        if (_last_frame) {
            flush();
        }
        inputFrame_l(frame, true);
    } else {
        if (_last_frame) {
            //如果时间戳发生了变化，那么markbit才置true
            inputFrame_l(_last_frame, _last_frame->pts() != frame->pts());
        }
        _last_frame = Frame::getCacheAbleFrame(frame);
    }
    return true;
}

void H265RtpEncoder::flush() {
    if (_last_frame) {
        // 如果时间戳发生了变化，那么markbit才置true
        inputFrame_l(_last_frame, true);
        _last_frame = nullptr;
    }
}

}//namespace mediakit
