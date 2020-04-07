/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AACRtp.h"
#define ADTS_HEADER_LEN 7

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
    GET_CONFIG(uint32_t, cycleMS, Rtp::kCycleMS);
    auto uiStamp = frame->dts();
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
    frame->aac_frame_length = ADTS_HEADER_LEN;
    frame->iPrefixSize = ADTS_HEADER_LEN;
    if(frame->syncword == 0 && !_aac_cfg.empty()) {
        makeAdtsHeader(_aac_cfg,*frame);
    }
    return frame;
}

bool AACRtpDecoder::inputRtp(const RtpPacket::Ptr &rtppack, bool key_pos) {
    //rtp数据开始部分
    uint8_t *ptr = (uint8_t *) rtppack->data() + rtppack->offset;
    //rtp数据末尾
    const uint8_t *end = (uint8_t *) rtppack->data() + rtppack->size();

    //首2字节表示Au-Header的个数，单位bit，所以除以16得到Au-Header个数
    const uint16_t au_header_count = ((ptr[0] << 8) | ptr[1]) >> 4;
    //忽略Au-Header区
    ptr += 2 + au_header_count * 2;

    static const uint32_t max_size = sizeof(AACFrame::buffer) - ADTS_HEADER_LEN;
    while (ptr < end) {
        auto size = (uint32_t) (end - ptr);
        if(size > max_size){
            size = max_size;
        }
        if (_adts->aac_frame_length + size > sizeof(AACFrame::buffer)) {
            //数据太多了，先清空
            flushData();
        }
        //追加aac数据
        memcpy(_adts->buffer + _adts->aac_frame_length, ptr, size);
        _adts->aac_frame_length += size;
        _adts->timeStamp = rtppack->timeStamp;
        ptr += size;
    }

    if (rtppack->mark) {
        //最后一个rtp分片
        flushData();
    }
    return false;
}


void AACRtpDecoder::flushData() {
    if(_adts->aac_frame_length == ADTS_HEADER_LEN){
        //没有有效数据
        return;
    }
    writeAdtsHeader(*_adts, _adts->buffer);
    RtpCodec::inputFrame(_adts);
    _adts = obtainFrame();
}


}//namespace mediakit



