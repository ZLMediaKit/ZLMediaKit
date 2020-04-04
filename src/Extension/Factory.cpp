/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Factory.h"
#include "Rtmp/Rtmp.h"
#include "H264Rtmp.h"
#include "H265Rtmp.h"
#include "AACRtmp.h"
#include "H264Rtp.h"
#include "AACRtp.h"
#include "H265Rtp.h"
#include "Common/Parser.h"

namespace mediakit{

Track::Ptr Factory::getTrackBySdp(const SdpTrack::Ptr &track) {
    if (strcasecmp(track->_codec.data(), "mpeg4-generic") == 0) {
        string aac_cfg_str = FindField(track->_fmtp.data(), "config=", nullptr);
        if (aac_cfg_str.empty()) {
            aac_cfg_str = FindField(track->_fmtp.data(), "config=", ";");
        }
        if (aac_cfg_str.empty()) {
            //如果sdp中获取不到aac config信息，那么在rtp也无法获取，那么忽略该Track
            return nullptr;
        }
        string aac_cfg;

        unsigned int cfg1;
        sscanf(aac_cfg_str.substr(0, 2).data(), "%02X", &cfg1);
        cfg1 &= 0x00FF;
        aac_cfg.push_back(cfg1);

        unsigned int cfg2;
        sscanf(aac_cfg_str.substr(2, 2).data(), "%02X", &cfg2);
        cfg2 &= 0x00FF;
        aac_cfg.push_back(cfg2);

        return std::make_shared<AACTrack>(aac_cfg);
    }

    if (strcasecmp(track->_codec.data(), "h264") == 0) {
        //a=fmtp:96 packetization-mode=1;profile-level-id=42C01F;sprop-parameter-sets=Z0LAH9oBQBboQAAAAwBAAAAPI8YMqA==,aM48gA==
        auto map = Parser::parseArgs(FindField(track->_fmtp.data()," ", nullptr),";","=");
        auto sps_pps = map["sprop-parameter-sets"];
        string base64_SPS = FindField(sps_pps.data(), NULL, ",");
        string base64_PPS = FindField(sps_pps.data(), ",", NULL);
        auto sps = decodeBase64(base64_SPS);
        auto pps = decodeBase64(base64_PPS);
        if(sps.empty() || pps.empty()){
            //如果sdp里面没有sps/pps,那么可能在后续的rtp里面恢复出sps/pps
            return std::make_shared<H264Track>();
        }

        return std::make_shared<H264Track>(sps,pps,0,0);
    }

    if (strcasecmp(track->_codec.data(), "h265") == 0) {
        //a=fmtp:96 sprop-sps=QgEBAWAAAAMAsAAAAwAAAwBdoAKAgC0WNrkky/AIAAADAAgAAAMBlQg=; sprop-pps=RAHA8vA8kAA=
        auto map = Parser::parseArgs(FindField(track->_fmtp.data()," ", nullptr),";","=");
        auto vps = decodeBase64(map["sprop-vps"]);
        auto sps = decodeBase64(map["sprop-sps"]);
        auto pps = decodeBase64(map["sprop-pps"]);
        if(sps.empty() || pps.empty()){
            //如果sdp里面没有sps/pps,那么可能在后续的rtp里面恢复出sps/pps
            return std::make_shared<H265Track>();
        }
        return std::make_shared<H265Track>(vps,sps,pps,0,0,0);
    }

    WarnL << "暂不支持该sdp:" << track->getName();
    return nullptr;
}


Track::Ptr Factory::getTrackByCodecId(CodecId codecId) {
    switch (codecId){
        case CodecH264:{
            return std::make_shared<H264Track>();
        }
        case CodecH265:{
            return std::make_shared<H265Track>();
        }
        case CodecAAC:{
            return std::make_shared<AACTrack>();
        }
        default:
            WarnL << "暂不支持该CodecId:" << codecId;
            return nullptr;
    }
}

RtpCodec::Ptr Factory::getRtpEncoderBySdp(const Sdp::Ptr &sdp) {
    GET_CONFIG(uint32_t,audio_mtu,Rtp::kAudioMtuSize);
    GET_CONFIG(uint32_t,video_mtu,Rtp::kVideoMtuSize);
    // ssrc不冲突即可,可以为任意的32位整形
    static atomic<uint32_t> s_ssrc(0);
    uint32_t ssrc = s_ssrc++;
    if(!ssrc){
        //ssrc不能为0
        ssrc = 1;
    }
    if(sdp->getTrackType() == TrackVideo){
        //视频的ssrc是偶数，方便调试
        ssrc = 2 * ssrc;
    }else{
        //音频ssrc是奇数
        ssrc = 2 * ssrc + 1;
    }
    auto mtu = (sdp->getTrackType() == TrackVideo ? video_mtu : audio_mtu);
    auto sample_rate = sdp->getSampleRate();
    auto pt = sdp->getPlayloadType();
    auto interleaved = sdp->getTrackType() * 2;
    auto codec_id = sdp->getCodecId();
    switch (codec_id){
        case CodecH264:
            return std::make_shared<H264RtpEncoder>(ssrc,mtu,sample_rate,pt,interleaved);
        case CodecH265:
            return std::make_shared<H265RtpEncoder>(ssrc,mtu,sample_rate,pt,interleaved);
        case CodecAAC:
            return std::make_shared<AACRtpEncoder>(ssrc,mtu,sample_rate,pt,interleaved);
        default:
            WarnL << "暂不支持该CodecId:" << codec_id;
            return nullptr;
    }
}

RtpCodec::Ptr Factory::getRtpDecoderByTrack(const Track::Ptr &track) {
    switch (track->getCodecId()){
        case CodecH264:
            return std::make_shared<H264RtpDecoder>();
        case CodecH265:
            return std::make_shared<H265RtpDecoder>();
        case CodecAAC:
            return std::make_shared<AACRtpDecoder>(track->clone());
        default:
            WarnL << "暂不支持该CodecId:" << track->getCodecName();
            return nullptr;
    }
}

/////////////////////////////rtmp相关///////////////////////////////////////////

Track::Ptr Factory::getTrackByAmf(const AMFValue &amf) {
    CodecId codecId = getCodecIdByAmf(amf);
    if(codecId == CodecInvalid){
        return nullptr;
    }
    return getTrackByCodecId(codecId);
}


CodecId Factory::getCodecIdByAmf(const AMFValue &val){
    if (val.type() == AMF_STRING){
        auto str = val.as_string();
        if(str == "avc1"){
            return CodecH264;
        }
        if(str == "mp4a"){
            return CodecAAC;
        }
        if(str == "hev1" || str == "hvc1"){
            return CodecH265;
        }
        WarnL << "暂不支持该Amf:" << str;
        return CodecInvalid;
    }

    if (val.type() != AMF_NULL){
        auto type_id = val.as_integer();
        switch (type_id){
            case FLV_CODEC_H264: return CodecH264;
            case FLV_CODEC_AAC: return CodecAAC;
            case FLV_CODEC_H265: return CodecH265;
            default:
                WarnL << "暂不支持该Amf:" << type_id;
                return CodecInvalid;
        }
    }else{
        WarnL << "Metadata不存在相应的Track";
    }

    return CodecInvalid;
}


RtmpCodec::Ptr Factory::getRtmpCodecByTrack(const Track::Ptr &track) {
    switch (track->getCodecId()){
        case CodecH264:
            return std::make_shared<H264RtmpEncoder>(track);
        case CodecAAC:
            return std::make_shared<AACRtmpEncoder>(track);
        case CodecH265:
            return std::make_shared<H265RtmpEncoder>(track);
        default:
            WarnL << "暂不支持该CodecId:" << track->getCodecName();
            return nullptr;
    }
}

AMFValue Factory::getAmfByCodecId(CodecId codecId) {
    switch (codecId){
        case CodecAAC: return AMFValue("mp4a");
        case CodecH264: return AMFValue("avc1");
        case CodecH265: return AMFValue(FLV_CODEC_H265);
        default: return AMFValue(AMF_NULL);
    }
}


}//namespace mediakit

