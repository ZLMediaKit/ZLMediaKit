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
    ret = thiz.strBuf.substr(2);
    return ret;
}

bool AACRtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt, bool) {
    if (pkt->isCfgFrame()) {
        _aac_cfg = getAacCfg(*pkt);
        onGetAAC(nullptr, 0, 0);
        return false;
    }

    if (!_aac_cfg.empty()) {
        onGetAAC(pkt->strBuf.data() + 2, pkt->strBuf.size() - 2, pkt->timeStamp);
    }
    return false;
}

void AACRtmpDecoder::onGetAAC(const char* data, int len, uint32_t stamp) {
    auto frame = ResourcePoolHelper<AACFrame>::obtainObj();
    //生成adts头
    char adts_header[32] = {0};
    auto size = dumpAacConfig(_aac_cfg, len, (uint8_t *) adts_header, sizeof(adts_header));
    if (size > 0) {
        frame->_buffer.assign(adts_header, size);
        frame->_prefix_size = size;
    } else {
        frame->_buffer.clear();
        frame->_prefix_size = 0;
    }

    if(len > 0){
        //追加负载数据
        frame->_buffer.append(data, len);
        frame->_dts = stamp;
    }

    if(size > 0 || len > 0){
        //有adts头或者实际aac负载
        RtmpCodec::inputFrame(frame);
    }
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
        if (frame->prefixSize()) {
            //包含adts头,从adts头获取aac配置信息
            _aac_cfg = makeAacConfig((uint8_t *) (frame->data()), frame->prefixSize());
        }
        makeConfigPacket();
    }

    if(!_aac_cfg.empty()){
        RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
        rtmpPkt->strBuf.clear();

        //header
        uint8_t is_config = false;
        rtmpPkt->strBuf.push_back(_audio_flv_flags);
        rtmpPkt->strBuf.push_back(!is_config);

        //aac data
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
    _audio_flv_flags = getAudioRtmpFlags(std::make_shared<AACTrack>(_aac_cfg));
    RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
    rtmpPkt->strBuf.clear();

    //header
    uint8_t is_config = true;
    rtmpPkt->strBuf.push_back(_audio_flv_flags);
    rtmpPkt->strBuf.push_back(!is_config);
    //aac config
    rtmpPkt->strBuf.append(_aac_cfg);

    rtmpPkt->bodySize = rtmpPkt->strBuf.size();
    rtmpPkt->chunkId = CHUNK_AUDIO;
    rtmpPkt->streamId = STREAM_MEDIA;
    rtmpPkt->timeStamp = 0;
    rtmpPkt->typeId = MSG_AUDIO;
    RtmpCodec::inputRtmp(rtmpPkt, false);
}

}//namespace mediakit