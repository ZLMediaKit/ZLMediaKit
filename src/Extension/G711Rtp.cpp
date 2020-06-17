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

G711RtpDecoder::G711RtpDecoder(const Track::Ptr &track){
    _codecid = track->getCodecId();
    _frame = obtainFrame();
}

G711Frame::Ptr G711RtpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<G711Frame>::obtainObj();
    frame->_buffer.clear();
    frame->_codecid = _codecid;
    frame->_dts = 0;
    return frame;
}

bool G711RtpDecoder::inputRtp(const RtpPacket::Ptr &rtppack, bool) {
    // 获取rtp数据长度
    int length = rtppack->size() - rtppack->offset;
    // 获取rtp数据
    const char *rtp_packet_buf = rtppack->data() + rtppack->offset;

    if (rtppack->timeStamp != _frame->_dts) {
        //时间戳变更，清空上一帧
        onGetG711(_frame);
    }

    //追加数据
    _frame->_buffer.append(rtp_packet_buf, length);
    //赋值时间戳
    _frame->_dts = rtppack->timeStamp;

    if (rtppack->mark || _frame->_buffer.size() > 10 * 1024) {
        //标记为mark时，或者内存快溢出时，我们认为这是该帧最后一个包
        onGetG711(_frame);
    }
    return false;
}

void G711RtpDecoder::onGetG711(const G711Frame::Ptr &frame) {
    if(!frame->_buffer.empty()){
        //写入环形缓存
        RtpCodec::inputFrame(frame);
        _frame = obtainFrame();
    }
}

/////////////////////////////////////////////////////////////////////////////////////

G711RtpEncoder::G711RtpEncoder(uint32_t ui32Ssrc,
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
    RtpCodec::inputRtp(makeRtp(getTrackType(), data, len, mark, uiStamp), false);
}

}//namespace mediakit
