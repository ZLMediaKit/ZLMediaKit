/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "G711Rtp.h"

namespace mediakit{

G711RtpEncoder::G711RtpEncoder(uint32_t ui32Ssrc,
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

void G711RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    GET_CONFIG(uint32_t, cycleMS, Rtp::kCycleMS);
    auto uiStamp = frame->dts();
    auto pcData = frame->data() + frame->prefixSize();
    auto iLen = frame->size() - frame->prefixSize();

    uiStamp %= cycleMS;
    char *ptr = (char *) pcData;
    int iSize = iLen;
    while (iSize > 0) {
        if (iSize <= _ui32MtuSize - 20) {
            makeG711Rtp(ptr, iSize, true, uiStamp);
            break;
        }
        makeG711Rtp(ptr, _ui32MtuSize - 20, false, uiStamp);
        ptr += (_ui32MtuSize - 20);
        iSize -= (_ui32MtuSize - 20);
    }
}

void G711RtpEncoder::makeG711Rtp(const void *data, unsigned int len, bool mark, uint32_t uiStamp) {
    RtpCodec::inputRtp(makeRtp(getTrackType(),data,len,mark,uiStamp), false);
}

/////////////////////////////////////////////////////////////////////////////////////

G711RtpDecoder::G711RtpDecoder(const Track::Ptr &track){
    auto aacTrack = dynamic_pointer_cast<G711Track>(track);
    _codecid = aacTrack->getCodecId();
    if(!aacTrack || !aacTrack->ready()){
        WarnL << "该g711 track无效!";
    }else{
        //_aac_cfg = aacTrack->getAacCfg();
    }
    _adts = obtainFrame();
}
G711RtpDecoder::G711RtpDecoder() {
    _adts = obtainFrame();
}

G711Frame::Ptr G711RtpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<G711Frame>::obtainObj();
    frame->frameLength = 0;
    frame->iPrefixSize = 0;
    return frame;
}

bool G711RtpDecoder::inputRtp(const RtpPacket::Ptr &rtppack, bool key_pos) {
	// 获取rtp数据长度
    int length = rtppack->size() - rtppack->offset;

	// 获取rtp数据
	const uint8_t *rtp_packet_buf = (uint8_t *)rtppack->data() + rtppack->offset;

    _adts->frameLength = length;
    memcpy(_adts->buffer, rtp_packet_buf, length);
    if (rtppack->mark == true) {
        _adts->timeStamp = rtppack->timeStamp;
        onGetG711(_adts);
    }
    return false;
}

void G711RtpDecoder::onGetG711(const G711Frame::Ptr &frame) {
    //写入环形缓存
    RtpCodec::inputFrame(frame);
    _adts = obtainFrame();
}


}//namespace mediakit



