/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 * Copyright (c) 2019 火宣 <459502659@qq.com>
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

#include "H264RtpCodec.h"

namespace mediakit{


typedef struct {
    unsigned forbidden_zero_bit :1;
    unsigned nal_ref_idc :2;
    unsigned type :5;
} NALU;

typedef struct {
    unsigned S :1;
    unsigned E :1;
    unsigned R :1;
    unsigned type :5;
} FU;

static bool MakeNalu(uint8_t in, NALU &nal) {
    nal.forbidden_zero_bit = in >> 7;
    if (nal.forbidden_zero_bit) {
        return false;
    }
    nal.nal_ref_idc = (in & 0x60) >> 5;
    nal.type = in & 0x1f;
    return true;
}
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
    frame->buffer.clear();
    frame->iPrefixSize = 4;
    return frame;
}

bool H264RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    key_pos = decodeRtp(rtp);
    RtpCodec::inputRtp(rtp, key_pos);
    return key_pos;
}

bool H264RtpDecoder::decodeRtp(const RtpPacket::Ptr &rtppack) {
    /**
     * h264帧类型
     * Type==1:P/B frame
     * Type==5:IDR frame
     * Type==6:SEI frame
     * Type==7:SPS frame
     * Type==8:PPS frame
     */

    const uint8_t *frame = (uint8_t *) rtppack->payload + rtppack->offset;
    int length = rtppack->length - rtppack->offset;
    NALU nal;
    MakeNalu(*frame, nal);

    if (nal.type >= 0 && nal.type < 24) {
        //a full frame
        _h264frame->buffer.assign("\x0\x0\x0\x1", 4);
        _h264frame->buffer.append((char *)frame, length);
        _h264frame->type = nal.type;
        _h264frame->timeStamp = rtppack->timeStamp;
        _h264frame->sequence = rtppack->sequence;
        auto isIDR = _h264frame->type == H264Frame::NAL_IDR;
        onGetH264(_h264frame);
        return (isIDR); //i frame
    }

    switch (nal.type){
        case 24:{
            // 24 STAP-A   单一时间的组合包
            bool haveIDR = false;
            auto ptr = frame + 1;
            while(true){
                int off = ptr - frame;
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
                if(len >= 10){
                    //过小的帧丢弃
                    NALU nal;
                    MakeNalu(ptr[0], nal);
                    _h264frame->buffer.assign("\x0\x0\x0\x1", 4);
                    _h264frame->buffer.append((char *)ptr, len);
                    _h264frame->type = nal.type;
                    _h264frame->timeStamp = rtppack->timeStamp;
                    _h264frame->sequence = rtppack->sequence;
                    if(nal.type == H264Frame::NAL_IDR){
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
            if (fu.S == 1) {
                //FU-A start
                char tmp = (nal.forbidden_zero_bit << 7 | nal.nal_ref_idc << 5 | fu.type);
                _h264frame->buffer.assign("\x0\x0\x0\x1", 4);
                _h264frame->buffer.push_back(tmp);
                _h264frame->buffer.append((char *)frame + 2, length - 2);
                _h264frame->type = fu.type;
                _h264frame->timeStamp = rtppack->timeStamp;
                _h264frame->sequence = rtppack->sequence;
                return (_h264frame->type == H264Frame::NAL_IDR); //i frame
            }

            if (rtppack->sequence != (uint16_t)(_h264frame->sequence + 1)) {
                _h264frame->buffer.clear();
                WarnL << "丢包,帧废弃:" << rtppack->sequence << "," << _h264frame->sequence;
                return false;
            }
            _h264frame->sequence = rtppack->sequence;
            if (fu.E == 1) {
                //FU-A end
                _h264frame->buffer.append((char *)frame + 2, length - 2);
                _h264frame->timeStamp = rtppack->timeStamp;
                auto isIDR = _h264frame->type == H264Frame::NAL_IDR;
                onGetH264(_h264frame);
                return isIDR;
            }
            //FU-A mid
            _h264frame->buffer.append((char *)frame + 2, length - 2);
            return false;
        }

        default:{
            // 29 FU-B     单NAL单元B模式
            // 25 STAP-B   单一时间的组合包
            // 26 MTAP16   多个时间的组合包
            // 27 MTAP24   多个时间的组合包
            // 0 udef
            // 30 udef
            // 31 udef
            WarnL << "不支持的rtp类型:" << (int)nal.type << " " << rtppack->sequence;
            return false;
        }
    }
}

void H264RtpDecoder::onGetH264(const H264Frame::Ptr &frame) {
    //写入环形缓存
    auto lastSeq = _h264frame->sequence;
    RtpCodec::inputFrame(frame);
    _h264frame = obtainFrame();
    _h264frame->sequence = lastSeq;
}


////////////////////////////////////////////////////////////////////////

H264RtpEncoder::H264RtpEncoder(uint32_t ui32Ssrc,
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

void H264RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    RtpCodec::inputFrame(frame);

    GET_CONFIG_AND_REGISTER(uint32_t,cycleMS,Rtp::kCycleMS);
    auto pcData = frame->data() + frame->prefixSize();
    auto uiStamp = frame->stamp();
    auto iLen = frame->size() - frame->prefixSize();
    unsigned char naluType =  H264_TYPE(pcData[0]); //获取NALU的5bit 帧类型

    uiStamp %= cycleMS;
    int iSize = _ui32MtuSize - 2;
    if (iLen > iSize) { //超过MTU
        const unsigned char s_e_r_Start = 0x80;
        const unsigned char s_e_r_Mid = 0x00;
        const unsigned char s_e_r_End = 0x40;
        //获取帧头数据，1byte
        unsigned char nal_ref_idc = *((unsigned char *) pcData) & 0x60; //获取NALU的2bit 帧重要程度 00 可以丢 11不能丢
        //nal_ref_idc = 0x60;
        //组装FU-A帧头数据 2byte
        unsigned char f_nri_type = nal_ref_idc + 28;//F为0 1bit,nri上面获取到2bit,28为FU-A分片类型5bit
        unsigned char s_e_r_type = naluType;
        bool bFirst = true;
        bool mark = false;
        int nOffset = 1;
        while (!mark) {
            if (iLen < nOffset + iSize) {			//是否拆分结束
                iSize = iLen - nOffset;
                mark = true;
                s_e_r_type = s_e_r_End + naluType;
            } else  if (bFirst) {
                s_e_r_type = s_e_r_Start + naluType;
            } else {
                s_e_r_type = s_e_r_Mid + naluType;
            }
            memcpy(_aucSectionBuf, &f_nri_type, 1);
            memcpy(_aucSectionBuf + 1, &s_e_r_type, 1);
            memcpy(_aucSectionBuf + 2, (unsigned char *) pcData + nOffset, iSize);
            nOffset += iSize;
            makeH264Rtp(naluType,_aucSectionBuf, iSize + 2, mark,bFirst, uiStamp);
            bFirst = false;
        }
    } else {
        makeH264Rtp(naluType,pcData, iLen, true, true, uiStamp);
    }
}

void H264RtpEncoder::makeH264Rtp(int nal_type,const void* data, unsigned int len, bool mark, bool first_packet, uint32_t uiStamp) {
    RtpCodec::inputRtp(makeRtp(getTrackType(),data,len,mark,uiStamp),first_packet && nal_type == H264Frame::NAL_IDR);
}

}//namespace mediakit