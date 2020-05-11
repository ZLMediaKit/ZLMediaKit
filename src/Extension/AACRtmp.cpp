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
    frame->_prefix_size = ADTS_HEADER_LEN;
    //预留7个字节的空位以便后续覆盖
    frame->_buffer.assign(ADTS_HEADER_LEN,(char)0);
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

bool AACRtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt, bool) {
    if (pkt->isCfgFrame()) {
        _aac_cfg = getAacCfg(*pkt);
        return false;
    }
    if (!_aac_cfg.empty()) {
        onGetAAC(pkt->strBuf.data() + 2, pkt->strBuf.size() - 2, pkt->timeStamp);
    }
    return false;
}

void AACRtmpDecoder::onGetAAC(const char* data, int len, uint32_t stamp) {
    _adts->_dts = stamp;
    //先追加数据
    _adts->_buffer.append(data, len);
    //覆盖adts头
    dumpAacConfig(_aac_cfg, _adts->size(), (uint8_t *) _adts->data());

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
            _aac_cfg = makeAacConfig((uint8_t *) (frame->data()));
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