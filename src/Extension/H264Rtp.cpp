/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H264Rtp.h"

namespace mediakit{

typedef struct {
    unsigned S :1;
    unsigned E :1;
    unsigned R :1;
    unsigned type :5;
} FU;

static bool MakeFU(uint8_t in, FU &fu) {
    fu.S = in >> 7;
    fu.E = (in >> 6) & 0x01;
    fu.R = (in >> 5) & 0x01;
    fu.type = in & 0x1f;
    if (fu.R != 0) {
        return false;
    }
    return true;
}

H264RtpDecoder::H264RtpDecoder() {
    _h264frame = obtainFrame();
}

H264Frame::Ptr  H264RtpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<H264Frame>::obtainObj();
    frame->_buffer.clear();
    frame->_prefix_size = 4;
    return frame;
}

bool H264RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    return decodeRtp(rtp);
}

bool H264RtpDecoder::decodeRtp(const RtpPacket::Ptr &rtp) {
    /**
     * h264帧类型
     * Type==1:P/B frame
     * Type==5:IDR frame
     * Type==6:SEI frame
     * Type==7:SPS frame
     * Type==8:PPS frame
     */
    /*
    RTF3984 5.2节  Common Structure of the RTP Payload Format
    Table 1.  Summary of NAL unit types and their payload structures

       Type   Packet    Type name                        Section
       ---------------------------------------------------------
       0      undefined                                    -
       1-23   NAL unit  Single NAL unit packet per H.264   5.6
       24     STAP-A    Single-time aggregation packet     5.7.1
       25     STAP-B    Single-time aggregation packet     5.7.1
       26     MTAP16    Multi-time aggregation packet      5.7.2
       27     MTAP24    Multi-time aggregation packet      5.7.2
       28     FU-A      Fragmentation unit                 5.8
       29     FU-B      Fragmentation unit                 5.8
       30-31  undefined                                    -
    */
    auto frame = rtp->getPayload();
    auto length = rtp->getPayloadSize();
    auto stamp = rtp->getStampMS();
    auto seq = rtp->getSeq();

    int nal_type = *frame & 0x1F;
    int nal_suffix = *frame & (~0x1F);

    if (nal_type >= 0 && nal_type < 24) {
        //a full frame
        _h264frame->_buffer.assign("\x0\x0\x0\x1", 4);
        _h264frame->_buffer.append((char *) frame, length);
        _h264frame->_pts = stamp;
        auto key = _h264frame->keyFrame();
        onGetH264(_h264frame);
        return (key); //i frame
    }

    switch (nal_type){
        case 24:{
            // 24 STAP-A   单一时间的组合包
            bool haveIDR = false;
            auto ptr = frame + 1;
            while (true) {
                size_t off = ptr - frame;
                if (off >= length) {
                    break;
                }
                //获取当前nalu的大小
                uint16_t len = *ptr++;
                len <<= 8;
                len |= *ptr++;
                if (off + len > length) {
                    break;
                }
                if (len > 0) {
                    //有有效数据
                    _h264frame->_buffer.assign("\x0\x0\x0\x1", 4);
                    _h264frame->_buffer.append((char *) ptr, len);
                    _h264frame->_pts = stamp;
                    if ((ptr[0] & 0x1F) == H264Frame::NAL_IDR) {
                        haveIDR = true;
                    }
                    onGetH264(_h264frame);
                }
                ptr += len;
            }
            return haveIDR;
        }

        case 28:{
            //FU-A
            FU fu;
            MakeFU(frame[1], fu);
            if (fu.S) {
                //该帧的第一个rtp包  FU-A start
                _h264frame->_buffer.assign("\x0\x0\x0\x1", 4);
                _h264frame->_buffer.push_back(nal_suffix | fu.type);
                _h264frame->_buffer.append((char *) frame + 2, length - 2);
                _h264frame->_pts = stamp;
                //该函数return时，保存下当前sequence,以便下次对比seq是否连续
                _lastSeq = seq;
                return _h264frame->keyFrame();
            }

            if (seq != (uint16_t)(_lastSeq + 1) && seq != 0) {
                //中间的或末尾的rtp包，其seq必须连续(如果回环了则判定为连续)，否则说明rtp丢包，那么该帧不完整，必须得丢弃
                _h264frame->_buffer.clear();
                WarnL << "rtp丢包: " << seq << " != " << _lastSeq << " + 1,该帧被废弃";
                return false;
            }

            if (!fu.E) {
                //该帧的中间rtp包  FU-A mid
                _h264frame->_buffer.append((char *) frame + 2, length - 2);
                //该函数return时，保存下当前sequence,以便下次对比seq是否连续
                _lastSeq = seq;
                return false;
            }

            //该帧最后一个rtp包  FU-A end
            _h264frame->_buffer.append((char *) frame + 2, length - 2);
            _h264frame->_pts = stamp;
            onGetH264(_h264frame);
            return false;
        }

        default: {
            // 29 FU-B     单NAL单元B模式
            // 25 STAP-B   单一时间的组合包
            // 26 MTAP16   多个时间的组合包
            // 27 MTAP24   多个时间的组合包
            // 0 udef
            // 30 udef
            // 31 udef
            WarnL << "不支持的rtp类型:" << (int) nal_type << " " << seq;
            return false;
        }
    }
}

void H264RtpDecoder::onGetH264(const H264Frame::Ptr &frame) {
    //rtsp没有dts，那么根据pts排序算法生成dts
    _dts_generator.getDts(frame->_pts,frame->_dts);
    //写入环形缓存
    RtpCodec::inputFrame(frame);
    _h264frame = obtainFrame();
}


////////////////////////////////////////////////////////////////////////

H264RtpEncoder::H264RtpEncoder(uint32_t ui32Ssrc,
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

void H264RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = frame->data() + frame->prefixSize();
    auto len = frame->size() - frame->prefixSize();
    auto pts = frame->pts();
    auto nal_type = H264_TYPE(ptr[0]);
    auto max_size = getMaxSize() - 2;

    //超过MTU则按照FU-A模式打包
    if (len > max_size + 1) {
        //最高位bit为forbidden_zero_bit,
        //后面2bit为nal_ref_idc(帧重要程度),00:可以丢,11:不能丢
        //末尾5bit为nalu type，固定为28(FU-A)
        unsigned char nal_fu_a = (*((unsigned char *) ptr) & (~0x1F)) | 28;
        unsigned char s_e_r_flags;
        bool fu_a_start = true;
        bool mark_bit = false;
        size_t offset = 1;
        while (!mark_bit) {
            if (len <= offset + max_size) {
                //FU-A end
                mark_bit = true;
                max_size = len - offset;
                s_e_r_flags = (1 << 6) | nal_type;
            } else if (fu_a_start) {
                //FU-A start
                s_e_r_flags = (1 << 7) | nal_type;
            } else {
                //FU-A mid
                s_e_r_flags = nal_type;
            }

            {
                //传入nullptr先不做payload的内存拷贝
                auto rtp = makeRtp(getTrackType(), nullptr, max_size + 2, mark_bit, pts);
                //rtp payload 负载部分
                uint8_t *payload = rtp->getPayload();
                //FU-A 第1个字节
                payload[0] = nal_fu_a;
                //FU-A 第2个字节
                payload[1] = s_e_r_flags;
                //H264 数据
                memcpy(payload + 2, (unsigned char *) ptr + offset, max_size);
                //输入到rtp环形缓存
                RtpCodec::inputRtp(rtp, fu_a_start && nal_type == H264Frame::NAL_IDR);
            }
            offset += max_size;
            fu_a_start = false;
        }
    } else {
        //如果帧长度不超过mtu, 则按照Single NAL unit packet per H.264 方式打包
        makeH264Rtp(ptr, len, false, false, pts);
    }
}

void H264RtpEncoder::makeH264Rtp(const void* data, size_t len, bool mark, bool gop_pos, uint32_t uiStamp) {
    RtpCodec::inputRtp(makeRtp(getTrackType(), data, len, mark, uiStamp), gop_pos);
}

}//namespace mediakit
