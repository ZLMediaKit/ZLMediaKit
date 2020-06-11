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
#define AAC_MAX_FRAME_SIZE (2 * 1024)

namespace mediakit{

AACRtpEncoder::AACRtpEncoder(uint32_t ui32Ssrc,
                             uint32_t ui32MtuSize,
                             uint32_t ui32SampleRate,
                             uint8_t ui8PayloadType,
                             uint8_t ui8Interleaved) :
        RtpInfo(ui32Ssrc,
                ui32MtuSize,
                ui32SampleRate,
                ui8PayloadType,
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
    RtpCodec::inputRtp(makeRtp(getTrackType(), data, len, mark, uiStamp), false);
}

/////////////////////////////////////////////////////////////////////////////////////

AACRtpDecoder::AACRtpDecoder(const Track::Ptr &track) {
    auto aacTrack = dynamic_pointer_cast<AACTrack>(track);
    if (!aacTrack || !aacTrack->ready()) {
        WarnL << "该aac track无效!";
    } else {
        _aac_cfg = aacTrack->getAacCfg();
    }
    _frame = obtainFrame();
}

AACRtpDecoder::AACRtpDecoder() {
    _frame = obtainFrame();
}

AACFrame::Ptr AACRtpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<AACFrame>::obtainObj();
    frame->_prefix_size = 0;
    frame->_buffer.clear();
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

    while (ptr < end) {
        auto size = (uint32_t) (end - ptr);
        if (size > AAC_MAX_FRAME_SIZE) {
            size = AAC_MAX_FRAME_SIZE;
        }
        if (_frame->size() + size > AAC_MAX_FRAME_SIZE) {
            //数据太多了，先清空
            flushData();
        }
        //追加aac数据
        _frame->_buffer.append((char *) ptr, size);
        _frame->_dts = rtppack->timeStamp;
        ptr += size;
    }

    if (rtppack->mark) {
        //最后一个rtp分片
        flushData();
    }
    return false;
}

void AACRtpDecoder::flushData() {
    if (_frame->_buffer.empty()) {
        //没有有效数据
        return;
    }

    //插入adts头
    char adts_header[32] = {0};
    auto size = dumpAacConfig(_aac_cfg, _frame->_buffer.size(), (uint8_t *) adts_header, sizeof(adts_header));
    if (size > 0) {
        //插入adts头
        _frame->_buffer.insert(0, adts_header, size);
        _frame->_prefix_size = size;
    }
    RtpCodec::inputFrame(_frame);
    _frame = obtainFrame();
}


}//namespace mediakit



