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
#include "AACRtpCodec.h"

namespace mediakit{

AACRtpEncoder::AACRtpEncoder(uint32_t ui32Ssrc,
                             uint32_t ui32MtuSize,
                             uint32_t ui32SampleRate,
                             uint8_t ui8PlayloadType,
                             uint8_t ui8Interleaved) :
        RtpInfo(ui32Ssrc,
                ui32MtuSize,
                ui32SampleRate,
                ui8PlayloadType,
                ui8Interleaved){
}

void AACRtpEncoder::inputFrame(const Frame::Ptr &frame) {
    RtpCodec::inputFrame(frame);

    GET_CONFIG_AND_REGISTER(uint32_t, cycleMS, Rtp::kCycleMS);
    auto uiStamp = frame->stamp();
    auto pcData = frame->data() + frame->prefixSize();
    auto iLen = frame->size() - frame->prefixSize();

    uiStamp %= cycleMS;
    char *ptr = (char *) pcData;
    int iSize = iLen;
    while (iSize > 0) {
        if (iSize <= _ui32MtuSize - 20) {
            _aucSectionBuf[0] = 0;
            _aucSectionBuf[1] = 16;
            _aucSectionBuf[2] = iLen >> 5;
            _aucSectionBuf[3] = (iLen & 0x1F) << 3;
            memcpy(_aucSectionBuf + 4, ptr, iSize);
            makeAACRtp(_aucSectionBuf, iSize + 4, true, uiStamp);
            break;
        }
        _aucSectionBuf[0] = 0;
        _aucSectionBuf[1] = 16;
        _aucSectionBuf[2] = (iLen) >> 5;
        _aucSectionBuf[3] = (iLen & 0x1F) << 3;
        memcpy(_aucSectionBuf + 4, ptr, _ui32MtuSize - 20);
        makeAACRtp(_aucSectionBuf, _ui32MtuSize - 16, false, uiStamp);
        ptr += (_ui32MtuSize - 20);
        iSize -= (_ui32MtuSize - 20);
    }
}

void AACRtpEncoder::makeAACRtp(const void *data, unsigned int len, bool mark, uint32_t uiStamp) {
    RtpCodec::inputRtp(makeRtp(getTrackType(),data,len,mark,uiStamp), false);
}

/////////////////////////////////////////////////////////////////////////////////////

AACRtpDecoder::AACRtpDecoder(const Track::Ptr &track){
    auto aacTrack = dynamic_pointer_cast<AACTrack>(track);
    if(!aacTrack || !aacTrack->ready()){
        WarnL << "该aac track无效!";
    }else{
        _aac_cfg = aacTrack->getAacCfg();
    }
    _adts = obtainFrame();
}
AACRtpDecoder::AACRtpDecoder() {
    _adts = obtainFrame();
}

AACFrame::Ptr AACRtpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<AACFrame>::obtainObj();
    frame->aac_frame_length = 7;
    frame->iPrefixSize = 7;
    if(frame->syncword == 0 && !_aac_cfg.empty()) {
        makeAdtsHeader(_aac_cfg,*frame);
    }
    return frame;
}

bool AACRtpDecoder::inputRtp(const RtpPacket::Ptr &rtppack, bool key_pos) {
    RtpCodec::inputRtp(rtppack, false);

    int length = rtppack->length - rtppack->offset;
    if (_adts->aac_frame_length + length - 4 > sizeof(AACFrame::buffer)) {
        _adts->aac_frame_length = 7;
        WarnL << "aac负载数据太长";
        return false;
    }
    memcpy(_adts->buffer + _adts->aac_frame_length, rtppack->payload + rtppack->offset + 4, length - 4);
    _adts->aac_frame_length += (length - 4);
    if (rtppack->mark == true) {
        _adts->sequence = rtppack->sequence;
        _adts->timeStamp = rtppack->timeStamp;
        writeAdtsHeader(*_adts, _adts->buffer);
        onGetAAC(_adts);
    }
    return false;
}

void AACRtpDecoder::onGetAAC(const AACFrame::Ptr &frame) {
    //写入环形缓存
    RtpCodec::inputFrame(frame);
    _adts = obtainFrame();
}


}//namespace mediakit



