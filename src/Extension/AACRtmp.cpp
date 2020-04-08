/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AACRtmp.h"
#include "Rtmp/Rtmp.h"

namespace mediakit{

AACRtmpDecoder::AACRtmpDecoder() {
    _adts = obtainFrame();
}

AACFrame::Ptr AACRtmpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = ResourcePoolHelper<AACFrame>::obtainObj();
    frame->aac_frame_length = 7;
    frame->iPrefixSize = 7;
    return frame;
}

static string getAacCfg(const RtmpPacket &thiz) {
    string ret;
    if (thiz.getMediaType() != FLV_CODEC_AAC) {
        return ret;
    }
    if (!thiz.isCfgFrame()) {
        return ret;
    }
    if (thiz.strBuf.size() < 4) {
        WarnL << "bad aac cfg!";
        return ret;
    }
    ret = thiz.strBuf.substr(2, 2);
    return ret;
}

bool AACRtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt, bool key_pos) {
    if (pkt->isCfgFrame()) {
        _aac_cfg = getAacCfg(*pkt);
        return false;
    }
    if (!_aac_cfg.empty()) {
        onGetAAC(pkt->strBuf.data() + 2, pkt->strBuf.size() - 2, pkt->timeStamp);
    }
    return false;
}

void AACRtmpDecoder::onGetAAC(const char* pcData, int iLen, uint32_t ui32TimeStamp) {
    if(iLen + 7 > sizeof(_adts->buffer)){
        WarnL << "Illegal adts data, exceeding the length limit.";
        return;
    }
    //写adts结构头
    makeAdtsHeader(_aac_cfg,*_adts);

    //拷贝aac负载
    memcpy(_adts->buffer + 7, pcData, iLen);
    _adts->aac_frame_length = 7 + iLen;
    _adts->timeStamp = ui32TimeStamp;

    //adts结构头转成头7个字节
    writeAdtsHeader(*_adts, _adts->buffer);

    //写入环形缓存
    RtmpCodec::inputFrame(_adts);
    _adts = obtainFrame();
}
/////////////////////////////////////////////////////////////////////////////////////

AACRtmpEncoder::AACRtmpEncoder(const Track::Ptr &track) {
    _track = dynamic_pointer_cast<AACTrack>(track);
}

void AACRtmpEncoder::makeConfigPacket() {
    if (_track && _track->ready()) {
        //从track中和获取aac配置信息
        _aac_cfg = _track->getAacCfg();
    }

    if (!_aac_cfg.empty()) {
        makeAudioConfigPkt();
    }
}

void AACRtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    if (_aac_cfg.empty()) {
        if (frame->prefixSize() >= 7) {
            //包含adts头,从adts头获取aac配置信息
            _aac_cfg = makeAdtsConfig(reinterpret_cast<const uint8_t *>(frame->data()));
        }
        makeConfigPacket();
    }

    if(!_aac_cfg.empty()){
        RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
        rtmpPkt->strBuf.clear();

        //////////header
        uint8_t is_config = false;
        rtmpPkt->strBuf.push_back(_ui8AudioFlags);
        rtmpPkt->strBuf.push_back(!is_config);
        rtmpPkt->strBuf.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());

        rtmpPkt->bodySize = rtmpPkt->strBuf.size();
        rtmpPkt->chunkId = CHUNK_AUDIO;
        rtmpPkt->streamId = STREAM_MEDIA;
        rtmpPkt->timeStamp = frame->dts();
        rtmpPkt->typeId = MSG_AUDIO;
        RtmpCodec::inputRtmp(rtmpPkt, false);
    }

}

void AACRtmpEncoder::makeAudioConfigPkt() {
    makeAdtsHeader(_aac_cfg,*_adts);
    int iSampleRate , iChannel , iSampleBit = 16;
    getAACInfo(*_adts,iSampleRate,iChannel);

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
    uint8_t flvAudioType = FLV_CODEC_AAC;

    _ui8AudioFlags = (flvAudioType << 4) | (flvSampleRate << 2) | (flvSampleBit << 1) | flvStereoOrMono;

    RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
    rtmpPkt->strBuf.clear();

    //////////header
    uint8_t is_config = true;
    rtmpPkt->strBuf.push_back(_ui8AudioFlags);
    rtmpPkt->strBuf.push_back(!is_config);
    rtmpPkt->strBuf.append(_aac_cfg);

    rtmpPkt->bodySize = rtmpPkt->strBuf.size();
    rtmpPkt->chunkId = CHUNK_AUDIO;
    rtmpPkt->streamId = STREAM_MEDIA;
    rtmpPkt->timeStamp = 0;
    rtmpPkt->typeId = MSG_AUDIO;
    RtmpCodec::inputRtmp(rtmpPkt, false);
}

}//namespace mediakit