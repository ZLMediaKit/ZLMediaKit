/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H265Rtp.h"

namespace mediakit{

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
            WarnL << "数据不够:" << total << " " << size; return ret; \
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
bool H265RtpDecoder::unpackAp(const uint8_t *ptr, ssize_t size, uint32_t stamp){
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
        if (singleFrame(ptr, nalu_size, stamp)) {
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

bool H265RtpDecoder::mergeFu(const uint8_t *ptr, ssize_t size, uint16_t seq, uint32_t stamp){
    CHECK_SIZE(size, 4, false);
    auto s_bit = ptr[2] >> 7;
    auto e_bit = (ptr[2] >> 6) & 0x01;
    auto type = ptr[2] & 0x3f;
    if (s_bit) {
        //该帧的第一个rtp包
        _frame->_buffer.assign("\x00\x00\x00\x01", 4);
        //恢复nalu头两个字节
        _frame->_buffer.push_back((type << 1) | (ptr[0] & 0x81));
        _frame->_buffer.push_back(ptr[1]);
    }

    if (!s_bit && seq != (uint16_t) (_last_seq + 1) && seq != 0) {
        //中间的或末尾的rtp包，其seq必须连续，否则说明rtp丢包，那么该帧不完整，必须得丢弃
        _frame->_buffer.clear();
        WarnL << "rtp丢包: " << seq << " != " << _last_seq << " + 1,该帧被废弃";
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

    if (!e_bit) {
        //非末尾包
        _last_seq = seq;
        _frame->_buffer.append((char *) ptr, size);
        return s_bit ? _frame->keyFrame() : false;
    }

    //该帧最后一个rtp包
    _frame->_pts = stamp;
    _frame->_buffer.append((char *) ptr, size);
    outputFrame(_frame);
    return false;
}

bool H265RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool ) {
    auto frame = rtp->getPayload();
    auto length = rtp->getPayloadSize();
    auto stamp = rtp->getStampMS();
    auto seq = rtp->getSeq();
    int nal = H265_TYPE(frame[0]);

    if (nal > 50){
        // packet discard, Unsupported (HEVC) NAL type
        WarnL << "不支持该类型的265 RTP包" << nal;
        return false;
    }
    switch (nal) {
        case 50:
            //4.4.4. PACI Packets (p32)
            WarnL << "不支持该类型的265 RTP包" << nal;
            return false;
        case 48:
            // aggregated packet (AP) - with two or more NAL units
            return unpackAp(frame, length, stamp);
        case 49: 
            // fragmentation unit (FU)
            return mergeFu(frame, length, seq, stamp);
        default: 
            // 4.4.1. Single NAL Unit Packets (p24)
            return singleFrame(frame, length, stamp);
    }
}

bool H265RtpDecoder::singleFrame(const uint8_t *ptr, ssize_t size, uint32_t stamp){
    //a full frame
    _frame->_buffer.assign("\x00\x00\x00\x01", 4);
    _frame->_buffer.append((char *) ptr, size);
    _frame->_pts = stamp;
    auto key = _frame->keyFrame();
    outputFrame(_frame);
    return key;
}

void H265RtpDecoder::outputFrame(const H265Frame::Ptr &frame) {
    //rtsp没有dts，那么根据pts排序算法生成dts
    _dts_generator.getDts(frame->_pts,frame->_dts);
    //输出frame
    RtpCodec::inputFrame(frame);
    _frame = obtainFrame();
}

////////////////////////////////////////////////////////////////////////

H265RtpEncoder::H265RtpEncoder(uint32_t ui32Ssrc,
                               uint32_t ui32MtuSize,
                               uint32_t ui32SampleRate,
                               uint8_t ui8PayloadType,
                               uint8_t ui8Interleaved) :
        RtpInfo(ui32Ssrc,
                ui32MtuSize,
                ui32SampleRate,
                ui8PayloadType,
                ui8Interleaved) {
}

void H265RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = (uint8_t *) frame->data() + frame->prefixSize();
    auto len = frame->size() - frame->prefixSize();
    auto pts = frame->pts();
    auto nal_type = H265_TYPE(ptr[0]); //获取NALU的5bit 帧类型
    auto max_size = getMaxSize() - 3;

    //超过MTU,按照FU方式打包
    if (len > max_size + 2) {
        //获取帧头数据，1byte
        unsigned char s_e_flags;
        bool fu_start = true;
        bool mark_bit = false;
        size_t offset = 2;
        while (!mark_bit) {
            if (len <= offset + max_size) {
                //FU end
                mark_bit = true;
                max_size = len - offset;
                s_e_flags = (1 << 6) | nal_type;
            } else if (fu_start) {
                //FU start
                s_e_flags = (1 << 7) | nal_type;
            } else {
                //FU mid
                s_e_flags = nal_type;
            }

            {
                //传入nullptr先不做payload的内存拷贝
                auto rtp = makeRtp(getTrackType(), nullptr, max_size + 3, mark_bit, pts);
                //rtp payload 负载部分
                uint8_t *payload = rtp->getPayload();
                //FU 第1个字节，表明为FU
                payload[0] = 49 << 1;
                //FU 第2个字节貌似固定为1
                payload[1] = ptr[1];// 1;
                //FU 第3个字节
                payload[2] = s_e_flags;
                //H265 数据
                memcpy(payload + 3, ptr + offset, max_size);
                //输入到rtp环形缓存
                RtpCodec::inputRtp(rtp, fu_start && H265Frame::isKeyFrame(nal_type, frame->data() + frame->prefixSize()));
            }

            offset += max_size;
            fu_start = false;
        }
    } else {
        makeH265Rtp(nal_type, ptr, len, false, true, pts);
    }
}

void H265RtpEncoder::makeH265Rtp(int nal_type,const void* data, size_t len, bool mark, bool first_packet, uint32_t uiStamp) {
    RtpCodec::inputRtp(makeRtp(getTrackType(),data,len,mark,uiStamp),first_packet && H265Frame::isKeyFrame(nal_type, (const char*)data + prefixSize((const char*)data, len)));
}

}//namespace mediakit
