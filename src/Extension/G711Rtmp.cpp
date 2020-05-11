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

G711RtmpDecoder::G711RtmpDecoder(CodecId codecId) {
    _frame = obtainFrame();
    _codecId = codecId;
}

G711Frame::Ptr G711RtmpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<G711Frame>::obtainObj();
    frame->_buffer.clear();
    frame->_codecid = _codecId;
    return frame;
}

bool G711RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt, bool) {
    //拷贝G711负载
    _frame->_buffer.assign(pkt->strBuf.data() + 1, pkt->strBuf.size() - 1);
    _frame->_dts = pkt->timeStamp;
    //写入环形缓存
    RtmpCodec::inputFrame(_frame);
    _frame = obtainFrame();
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////

G711RtmpEncoder::G711RtmpEncoder(const Track::Ptr &track) : G711RtmpDecoder(track->getCodecId()) {
    _audio_flv_flags = getAudioRtmpFlags(track);
}

void G711RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    if(!_audio_flv_flags){
        return;
    }
    RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
    rtmpPkt->strBuf.clear();
    //header
    rtmpPkt->strBuf.push_back(_audio_flv_flags);

    //g711 data
    rtmpPkt->strBuf.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
    rtmpPkt->bodySize = rtmpPkt->strBuf.size();
    rtmpPkt->chunkId = CHUNK_AUDIO;
    rtmpPkt->streamId = STREAM_MEDIA;
    rtmpPkt->timeStamp = frame->dts();
    rtmpPkt->typeId = MSG_AUDIO;
    RtmpCodec::inputRtmp(rtmpPkt, false);
}

}//namespace mediakit