/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "G711Rtmp.h"

namespace mediakit{

G711RtmpDecoder::G711RtmpDecoder() {
    _adts = obtainFrame();
}

G711Frame::Ptr G711RtmpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<G711Frame>::obtainObj();
    frame->frameLength = 0;
    frame->iPrefixSize = 0;
    return frame;
}

bool G711RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt, bool key_pos) {
    onGetG711(pkt->strBuf.data() + 2, pkt->strBuf.size() - 2, pkt->timeStamp);
    return false;
}

void G711RtmpDecoder::onGetG711(const char* pcData, int iLen, uint32_t ui32TimeStamp) {
    if(iLen + 7 > sizeof(_adts->buffer)){
        WarnL << "Illegal adts data, exceeding the length limit.";
        return;
    }

    //拷贝aac负载
    memcpy(_adts->buffer, pcData, iLen);
    _adts->frameLength = iLen;
    _adts->timeStamp = ui32TimeStamp;

    //写入环形缓存
    RtmpCodec::inputFrame(_adts);
    _adts = obtainFrame();
}
/////////////////////////////////////////////////////////////////////////////////////

G711RtmpEncoder::G711RtmpEncoder(const Track::Ptr &track) {
    _track = dynamic_pointer_cast<G711Track>(track);
}

void G711RtmpEncoder::inputFrame(const Frame::Ptr& frame) {

    RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
    rtmpPkt->strBuf.clear();
    rtmpPkt->strBuf.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());

    rtmpPkt->bodySize = rtmpPkt->strBuf.size();
    rtmpPkt->chunkId = CHUNK_AUDIO;
    rtmpPkt->streamId = STREAM_MEDIA;
    rtmpPkt->timeStamp = frame->dts();
    rtmpPkt->typeId = MSG_AUDIO;
    RtmpCodec::inputRtmp(rtmpPkt, false);

}

}//namespace mediakit