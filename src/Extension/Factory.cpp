/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
#include "H264Rtmp.h"
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
        if(sps_pps.empty()){
            return std::make_shared<H264Track>();
        }
        string base64_SPS = FindField(sps_pps.data(), NULL, ",");
        string base64_PPS = FindField(sps_pps.data(), ",", NULL);
        auto sps = decodeBase64(base64_SPS);
        auto pps = decodeBase64(base64_PPS);
        return std::make_shared<H264Track>(sps,pps,0,0);
    }

    if (strcasecmp(track->_codec.data(), "h265") == 0) {
        //a=fmtp:96 sprop-sps=QgEBAWAAAAMAsAAAAwAAAwBdoAKAgC0WNrkky/AIAAADAAgAAAMBlQg=; sprop-pps=RAHA8vA8kAA=
        auto map = Parser::parseArgs(FindField(track->_fmtp.data()," ", nullptr),";","=");
        auto vps = decodeBase64(map["sprop-vps"]);
        auto sps = decodeBase64(map["sprop-sps"]);
        auto pps = decodeBase64(map["sprop-pps"]);
        return std::make_shared<H265Track>(vps,sps,pps,0,0,0);
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

