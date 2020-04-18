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
    frame->buffer.clear();
    frame->_codecId = _codecId;
    return frame;
}

bool G711RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt, bool) {
    //拷贝G711负载
    _frame->buffer.assign(pkt->strBuf.data() + 2, pkt->strBuf.size() - 2);
    _frame->timeStamp = pkt->timeStamp;
    //写入环形缓存
    RtmpCodec::inputFrame(_frame);
    _frame = obtainFrame();
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////

G711RtmpEncoder::G711RtmpEncoder(const Track::Ptr &track) : G711RtmpDecoder(track->getCodecId()) {
    auto g711_track = dynamic_pointer_cast<AudioTrack>(track);
    if(!g711_track){
        WarnL << "无效的G711 track, 将忽略打包为RTMP";
        return;
    }

    auto iSampleRate = g711_track->getAudioSampleRate() ;
    auto iChannel =  g711_track->getAudioChannel();
    auto iSampleBit = g711_track->getAudioSampleBit();
    uint8_t flvStereoOrMono = (iChannel > 1);
    uint8_t flvSampleRate;
    switch (iSampleRate) {
        case 48000:
        case 44100:
            flvSampleRate = 3;
            break;
        case 24000:
        case 22050:
            flvSampleRate = 2;
            break;
        case 12000:
        case 11025:
            flvSampleRate = 1;
            break;
        default:
            flvSampleRate = 0;
            break;
    }
    uint8_t flvSampleBit = iSampleBit == 16;
    uint8_t flvAudioType ;
    switch (g711_track->getCodecId()){
        case CodecG711A : flvAudioType = FLV_CODEC_G711A; break;
        case CodecG711U : flvAudioType = FLV_CODEC_G711U; break;
        default: WarnL << "无效的G711 track, 将忽略打包为RTMP"; return ;
    }

    _g711_flags = (flvAudioType << 4) | (flvSampleRate << 2) | (flvSampleBit << 1) | flvStereoOrMono;
}

void G711RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    if(!_g711_flags){
        return;
    }
    RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
    rtmpPkt->strBuf.clear();
    //header
    uint8_t is_config = false;
    rtmpPkt->strBuf.push_back(_g711_flags);
    rtmpPkt->strBuf.push_back(!is_config);

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