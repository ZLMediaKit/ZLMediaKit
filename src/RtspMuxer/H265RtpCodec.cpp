/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#include "H265RtpCodec.h"

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
    frame->buffer.clear();
    frame->iPrefixSize = 4;
    return frame;
}

bool H265RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    key_pos = decodeRtp(rtp);
    RtpCodec::inputRtp(rtp, key_pos);
    return key_pos;
}

bool H265RtpDecoder::decodeRtp(const RtpPacket::Ptr &rtppack) {
    const uint8_t *frame = (uint8_t *) rtppack->payload + rtppack->offset;
    int length = rtppack->length - rtppack->offset;
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
            if (fu.S == 1) {
                //FU-A start
                _h265frame->buffer.assign("\x0\x0\x0\x1", 4);
                _h265frame->buffer.push_back(fu.type << 1);
                _h265frame->buffer.push_back(0x01);
                _h265frame->buffer.append((char *) frame + 3, length - 3);
                _h265frame->type = fu.type;
                _h265frame->timeStamp = rtppack->timeStamp;
                _h265frame->sequence = rtppack->sequence;
                return (_h265frame->keyFrame()); //i frame
            }

            if (rtppack->sequence != (uint16_t) (_h265frame->sequence + 1)) {
                _h265frame->buffer.clear();
                WarnL << "丢包,帧废弃:" << rtppack->sequence << "," << _h265frame->sequence;
                return false;
            }
            _h265frame->sequence = rtppack->sequence;
            if (fu.E == 1) {
                //FU-A end
                _h265frame->buffer.append((char *) frame + 3, length - 3);
                _h265frame->timeStamp = rtppack->timeStamp;
                auto isIDR = _h265frame->keyFrame();
                onGetH265(_h265frame);
                return isIDR;
            }
            //FU-A mid
            _h265frame->buffer.append((char *) frame + 3, length - 3);
            return false;
        }

        default: // 4.4.1. Single NAL Unit Packets (p24)
            //a full frame
            _h265frame->buffer.assign("\x0\x0\x0\x1", 4);
            _h265frame->buffer.append((char *)frame, length);
            _h265frame->type = nal;
            _h265frame->timeStamp = rtppack->timeStamp;
            _h265frame->sequence = rtppack->sequence;
            auto isIDR = _h265frame->keyFrame();
            onGetH265(_h265frame);
            return (isIDR); //i frame
    }
}

void H265RtpDecoder::onGetH265(const H265Frame::Ptr &frame) {
    //写入环形缓存
    auto lastSeq = _h265frame->sequence;
    RtpCodec::inputFrame(frame);
    _h265frame = obtainFrame();
    _h265frame->sequence = lastSeq;
}


////////////////////////////////////////////////////////////////////////

H265RtpEncoder::H265RtpEncoder(uint32_t ui32Ssrc,
                               uint32_t ui32MtuSize,
                               uint32_t ui32SampleRate,
                               uint8_t ui8PlayloadType,
                               uint8_t ui8Interleaved) :
        RtpInfo(ui32Ssrc,
                ui32MtuSize,
                ui32SampleRate,
                ui8PlayloadType,
                ui8Interleaved) {
}

void H265RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    RtpCodec::inputFrame(frame);

    GET_CONFIG_AND_REGISTER(uint32_t,cycleMS,Rtp::kCycleMS);
    uint8_t *pcData = (uint8_t*)frame->data() + frame->prefixSize();
    auto uiStamp = frame->stamp();
    auto iLen = frame->size() - frame->prefixSize();
    unsigned char naluType = H265_TYPE(pcData[0]); //获取NALU的5bit 帧类型
    uiStamp %= cycleMS;

    int maxSize = _ui32MtuSize - 3;
    if (iLen > maxSize) { //超过MTU
        //获取帧头数据，1byte
        unsigned char s_e_type;
        bool bFirst = true;
        bool mark = false;
        int nOffset = 2;
        while (!mark) {
            if (iLen < nOffset + maxSize) {			//是否拆分结束
                maxSize = iLen - nOffset;
                mark = true;
                s_e_type = 1 << 6 | naluType;
            } else if (bFirst) {
                    s_e_type = 1 << 7 | naluType;
            } else {
                s_e_type = naluType;
            }

            //FU type
            _aucSectionBuf[0] = 49 << 1;
            _aucSectionBuf[1] = 1;
            _aucSectionBuf[2] = s_e_type;
            memcpy(_aucSectionBuf + 3, pcData + nOffset, maxSize);
            nOffset += maxSize;
            makeH265Rtp(naluType,_aucSectionBuf, maxSize + 3, mark,bFirst, uiStamp);
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