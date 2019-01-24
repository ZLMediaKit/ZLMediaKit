/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Factory.h"
#include "RtmpMuxer/H264RtmpCodec.h"
#include "RtmpMuxer/AACRtmpCodec.h"
#include "RtspMuxer/H264RtpCodec.h"
#include "RtspMuxer/AACRtpCodec.h"
#include "RtspMuxer/H265RtpCodec.h"

namespace mediakit{

Sdp::Ptr Factory::getSdpByTrack(const Track::Ptr &track) {
    switch (track->getCodecId()){
        case CodecH264:{
            H264Track::Ptr h264Track = dynamic_pointer_cast<H264Track>(track);
            if(!h264Track){
                WarnL << "该Track不是H264Track类型";
                return nullptr;
            }
            if(!h264Track->ready()){
                WarnL << "该Track未准备好";
                return nullptr;
            }
            return std::make_shared<H264Sdp>(h264Track->getSps(),h264Track->getPps());
        }

        case CodecH265:{
            H265Track::Ptr h265Track = dynamic_pointer_cast<H265Track>(track);
            if(!h265Track){
                WarnL << "该Track不是H265Track类型";
                return nullptr;
            }
            if(!h265Track->ready()){
                WarnL << "该Track未准备好";
                return nullptr;
            }
            return std::make_shared<H265Sdp>(h265Track->getVps(),h265Track->getSps(),h265Track->getPps());
        }

        case CodecAAC:{
            AACTrack::Ptr aacTrack = dynamic_pointer_cast<AACTrack>(track);
            if(!aacTrack){
                WarnL << "该Track不是AACTrack类型";
                return nullptr;
            }
            if(!aacTrack->ready()){
                WarnL << "该Track未准备好";
                return nullptr;
            }
            return std::make_shared<AACSdp>(aacTrack->getAacCfg(),aacTrack->getAudioSampleRate());
        }
        default:
            WarnL << "暂不支持的CodecId:" << track->getCodecId();
            return nullptr;
    }
}


Track::Ptr Factory::getTrackBySdp(const SdpTrack::Ptr &track) {
    if (strcasecmp(track->_codec.data(), "mpeg4-generic") == 0) {
        string aac_cfg_str = FindField(track->_fmtp.c_str(), "config=", nullptr);
        if (aac_cfg_str.size() != 4) {
            aac_cfg_str = FindField(track->_fmtp.c_str(), "config=", ";");
        }
        if (aac_cfg_str.size() != 4) {
            //延后获取adts头
            return std::make_shared<AACTrack>();
        }
        string aac_cfg;

        unsigned int cfg1;
        sscanf(aac_cfg_str.substr(0, 2).c_str(), "%02X", &cfg1);
        cfg1 &= 0x00FF;
        aac_cfg.push_back(cfg1);

        unsigned int cfg2;
        sscanf(aac_cfg_str.substr(2, 2).c_str(), "%02X", &cfg2);
        cfg2 &= 0x00FF;
        aac_cfg.push_back(cfg2);

        return std::make_shared<AACTrack>(aac_cfg);
    }

    if (strcasecmp(track->_codec.data(), "h264") == 0) {
        string sps_pps = FindField(track->_fmtp.c_str(), "sprop-parameter-sets=", nullptr);
        if(sps_pps.empty()){
            return std::make_shared<H264Track>();
        }
        string base64_SPS = FindField(sps_pps.c_str(), NULL, ",");
        string base64_PPS = FindField(sps_pps.c_str(), ",", NULL);
        if(base64_PPS.back() == ';'){
            base64_PPS.pop_back();
        }

        auto sps = decodeBase64(base64_SPS);
        auto pps = decodeBase64(base64_PPS);
        return std::make_shared<H264Track>(sps,pps,0,0);
    }

    if (strcasecmp(track->_codec.data(), "h265") == 0) {
        //a=fmtp:96 sprop-sps=QgEBAWAAAAMAsAAAAwAAAwBdoAKAgC0WNrkky/AIAAADAAgAAAMBlQg=; sprop-pps=RAHA8vA8kAA=
        int pt;
        char sprop_vps[128] = {0},sprop_sps[128] = {0},sprop_pps[128] = {0};
        if (4 == sscanf(track->_fmtp.c_str(), "%d sprop-vps=%127[^;]; sprop-sps=%127[^;]; sprop-pps=%127[^;]", &pt, sprop_vps,sprop_sps, sprop_pps)) {
            auto vps = decodeBase64(sprop_vps);
            auto sps = decodeBase64(sprop_sps);
            auto pps = decodeBase64(sprop_pps);
            return std::make_shared<H265Track>(vps,sps,pps,0,0,0);
        }
        if (3 == sscanf(track->_fmtp.c_str(), "%d sprop-sps=%127[^;]; sprop-pps=%127[^;]", &pt,sprop_sps, sprop_pps)) {
            auto sps = decodeBase64(sprop_sps);
            auto pps = decodeBase64(sprop_pps);
            return std::make_shared<H265Track>("",sps,pps,0,0,0);
        }
        return std::make_shared<H265Track>();
    }


    WarnL << "暂不支持该sdp:" << track->_codec << " " << track->_fmtp;
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

RtpCodec::Ptr Factory::getRtpEncoderById(CodecId codecId,
                                         uint32_t ui32Ssrc,
                                         uint32_t ui32MtuSize,
                                         uint32_t ui32SampleRate,
                                         uint8_t ui8PlayloadType,
                                         uint8_t ui8Interleaved) {
    switch (codecId){
        case CodecH264:
            return std::make_shared<H264RtpEncoder>(ui32Ssrc,ui32MtuSize,ui32SampleRate,ui8PlayloadType,ui8Interleaved);
        case CodecH265:
            return std::make_shared<H265RtpEncoder>(ui32Ssrc,ui32MtuSize,ui32SampleRate,ui8PlayloadType,ui8Interleaved);
        case CodecAAC:
            return std::make_shared<AACRtpEncoder>(ui32Ssrc,ui32MtuSize,ui32SampleRate,ui8PlayloadType,ui8Interleaved);
        default:
            WarnL << "暂不支持该CodecId:" << codecId;
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
            WarnL << "暂不支持该CodecId:" << track->getCodecId();
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
        WarnL << "暂不支持该Amf:" << str;
        return CodecInvalid;
    }

    if (val.type() != AMF_NULL){
        auto type_id = val.as_integer();
        switch (type_id){
            case 7:{
                return CodecH264;
            }
            case 10:{
                return CodecAAC;
            }
            default:
                WarnL << "暂不支持该Amf:" << type_id;
                return CodecInvalid;
        }
    }else{
        WarnL << "Metedata不存在相应的Track";
    }

    return CodecInvalid;
}


RtmpCodec::Ptr Factory::getRtmpCodecByTrack(const Track::Ptr &track) {
    switch (track->getCodecId()){
        case CodecH264:
            return std::make_shared<H264RtmpEncoder>(track);
        case CodecAAC:
            return std::make_shared<AACRtmpEncoder>(track);
        default:
            WarnL << "暂不支持该CodecId:" << track->getCodecId();
            return nullptr;
    }
}

AMFValue Factory::getAmfByCodecId(CodecId codecId) {
    switch (codecId){
        case CodecAAC:{
            return AMFValue("mp4a");
        }
        case CodecH264:{
            return AMFValue("avc1");
        }
        default:
            return AMFValue(AMF_NULL);
    }
}


}//namespace mediakit

