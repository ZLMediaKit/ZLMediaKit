/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H265Rtp.h"

namespace mediakit{

//41
//42              0                   1
//43              0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//44             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//45             |F|   Type    |  LayerId  | TID |
//46             +-------------+-----------------+
//48                F       = 0
//49                Type    = 49 (fragmentation unit (FU))
//50                LayerId = 0
//51                TID     = 1
//56         /*
//57               create the FU header
//58
//59               0 1 2 3 4 5 6 7
//60              +-+-+-+-+-+-+-+-+
//61              |S|E|  FuType   |
//62              +---------------+
//63
//64                 S       = variable
//65                 E       = variable
//66                 FuType  = NAL unit type
//67

typedef struct {
    unsigned S :1;
    unsigned E :1;
    unsigned type :6;
} FU;

static void MakeFU(uint8_t in, FU &fu) {
    fu.S = in >> 7;
    fu.E = (in >> 6) & 0x01;
    fu.type = in & 0x3f;
}

H265RtpDecoder::H265RtpDecoder() {
    _h265frame = obtainFrame();
}

H265Frame::Ptr  H265RtpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<H265Frame>::obtainObj();
    frame->_buffer.clear();
    frame->_prefix_size = 4;
    return frame;
}

bool H265RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    return decodeRtp(rtp);
}

bool H265RtpDecoder::decodeRtp(const RtpPacket::Ptr &rtppack) {
    const uint8_t *frame = (uint8_t *) rtppack->data() + rtppack->offset;
    int length = rtppack->size() - rtppack->offset;
    int nal = H265_TYPE(frame[0]);

    if (nal > 50){
        WarnL << "不支持该类型的265 RTP包" << nal;
        return false; // packet discard, Unsupported (HEVC) NAL type
    }
    switch (nal) {
        case 50:
        case 48: // aggregated packet (AP) - with two or more NAL units
            WarnL << "不支持该类型的265 RTP包" << nal;
            return false;
        case 49: {
            // fragmentation unit (FU)
            FU fu;
            MakeFU(frame[2], fu);
            if (fu.S) {
                //该帧的第一个rtp包
                _h265frame->_buffer.assign("\x0\x0\x0\x1", 4);
                _h265frame->_buffer.push_back(fu.type << 1);
                _h265frame->_buffer.push_back(0x01);
                _h265frame->_buffer.append((char *) frame + 3, length - 3);
                _h265frame->_pts = rtppack->timeStamp;
                //该函数return时，保存下当前sequence,以便下次对比seq是否连续
                _lastSeq = rtppack->sequence;
                return (_h265frame->keyFrame()); //i frame
            }

            if (rtppack->sequence != _lastSeq + 1 && rtppack->sequence != 0) {
                //中间的或末尾的rtp包，其seq必须连续(如果回环了则判定为连续)，否则说明rtp丢包，那么该帧不完整，必须得丢弃
                _h265frame->_buffer.clear();
                WarnL << "rtp丢包: " << rtppack->sequence << " != " << _lastSeq << " + 1,该帧被废弃";
                return false;
            }

            if (!fu.E) {
                //该帧的中间rtp包
                _h265frame->_buffer.append((char *) frame + 3, length - 3);
                //该函数return时，保存下当前sequence,以便下次对比seq是否连续
                _lastSeq = rtppack->sequence;
                return false;
            }

            //该帧最后一个rtp包
            _h265frame->_buffer.append((char *) frame + 3, length - 3);
            _h265frame->_pts = rtppack->timeStamp;
            onGetH265(_h265frame);
            return false;
        }

        default: // 4.4.1. Single NAL Unit Packets (p24)
            //a full frame
            _h265frame->_buffer.assign("\x0\x0\x0\x1", 4);
            _h265frame->_buffer.append((char *)frame, length);
            _h265frame->_pts = rtppack->timeStamp;
            auto key = _h265frame->keyFrame();
            onGetH265(_h265frame);
            return key;
    }
}

void H265RtpDecoder::onGetH265(const H265Frame::Ptr &frame) {
    //rtsp没有dts，那么根据pts排序算法生成dts
    _dts_generator.getDts(frame->_pts,frame->_dts);
    //写入环形缓存
    RtpCodec::inputFrame(frame);
    _h265frame = obtainFrame();
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
    GET_CONFIG(uint32_t,cycleMS,Rtp::kCycleMS);
    uint8_t *pcData = (uint8_t*)frame->data() + frame->prefixSize();
    auto uiStamp = frame->pts();
    auto iLen = frame->size() - frame->prefixSize();
    unsigned char naluType = H265_TYPE(pcData[0]); //获取NALU的5bit 帧类型
    uiStamp %= cycleMS;

    int maxSize = _ui32MtuSize - 3;
    //超过MTU,按照FU方式打包
    if (iLen > maxSize) {
        //获取帧头数据，1byte
        unsigned char s_e_flags;
        bool bFirst = true;
        bool mark = false;
        int nOffset = 2;
        while (!mark) {
            if (iLen < nOffset + maxSize) {			//是否拆分结束
                maxSize = iLen - nOffset;
                mark = true;
                //FU end
                s_e_flags = (1 << 6) | naluType;
            } else if (bFirst) {
                //FU start
                s_e_flags = (1 << 7) | naluType;
            } else {
                //FU mid
                s_e_flags = naluType;
            }

            {
                //传入nullptr先不做payload的内存拷贝
                auto rtp = makeRtp(getTrackType(), nullptr, maxSize + 3, mark, uiStamp);
                //rtp payload 负载部分
                uint8_t *payload = (uint8_t*)rtp->data() + rtp->offset;
                //FU 第1个字节，表明为FU
                payload[0] = 49 << 1;
                //FU 第2个字节貌似固定为1
                payload[1] = 1;
                //FU 第3个字节
                payload[2] = s_e_flags;
                //H265 数据
                memcpy(payload + 3,pcData + nOffset, maxSize);
                //输入到rtp环形缓存
                RtpCodec::inputRtp(rtp,bFirst && H265Frame::isKeyFrame(naluType));
            }

            nOffset += maxSize;
            bFirst = false;
        }
    } else {
        makeH265Rtp(naluType,pcData, iLen, true, true, uiStamp);
    }
}

void H265RtpEncoder::makeH265Rtp(int nal_type,const void* data, unsigned int len, bool mark, bool first_packet, uint32_t uiStamp) {
    RtpCodec::inputRtp(makeRtp(getTrackType(),data,len,mark,uiStamp),first_packet && H265Frame::isKeyFrame(nal_type));
}

}//namespace mediakit