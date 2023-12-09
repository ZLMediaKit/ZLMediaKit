/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Factory.h"
#include "Rtmp/Rtmp.h"
#include "H264Rtmp.h"
#include "H265Rtmp.h"
#include "AACRtmp.h"
#include "CommonRtmp.h"
#include "H264Rtp.h"
#include "JPEGRtp.h"
#include "AACRtp.h"
#include "H265Rtp.h"
#include "CommonRtp.h"
#include "G711Rtp.h"
#include "Opus.h"
#include "G711.h"
#include "L16.h"
#include "JPEG.h"
#include "Util/base64.h"
#include "Common/Parser.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

Track::Ptr Factory::getTrackBySdp(const SdpTrack::Ptr &track) {
    auto codec = getCodecId(track->_codec);
    if (codec == CodecInvalid) {
        //根据传统的payload type 获取编码类型以及采样率等信息
        codec = RtpPayload::getCodecId(track->_pt);
    }
    switch (codec) {
        case CodecG711A:
        case CodecG711U: return std::make_shared<G711Track>(codec, track->_samplerate, track->_channel, 16);
        case CodecL16:  return std::make_shared<L16Track>(track->_samplerate, track->_channel);
        case CodecOpus : return std::make_shared<OpusTrack>();

        case CodecAAC : {
            string aac_cfg_str = findSubString(track->_fmtp.data(), "config=", ";");
            if (aac_cfg_str.empty()) {
                aac_cfg_str = findSubString(track->_fmtp.data(), "config=", nullptr);
            }
            if (aac_cfg_str.empty()) {
                //如果sdp中获取不到aac config信息，那么在rtp也无法获取，那么忽略该Track
                return nullptr;
            }
            string aac_cfg;
            for (size_t i = 0; i < aac_cfg_str.size() / 2; ++i) {
                unsigned int cfg;
                sscanf(aac_cfg_str.substr(i * 2, 2).data(), "%02X", &cfg);
                cfg &= 0x00FF;
                aac_cfg.push_back((char) cfg);
            }
            return std::make_shared<AACTrack>(aac_cfg);
        }

        case CodecH264 : {
            //a=fmtp:96 packetization-mode=1;profile-level-id=42C01F;sprop-parameter-sets=Z0LAH9oBQBboQAAAAwBAAAAPI8YMqA==,aM48gA==
            auto map = Parser::parseArgs(track->_fmtp, ";", "=");
            auto sps_pps = map["sprop-parameter-sets"];
            string base64_SPS = findSubString(sps_pps.data(), NULL, ",");
            string base64_PPS = findSubString(sps_pps.data(), ",", NULL);
            auto sps = decodeBase64(base64_SPS);
            auto pps = decodeBase64(base64_PPS);
            if (sps.empty() || pps.empty()) {
                //如果sdp里面没有sps/pps,那么可能在后续的rtp里面恢复出sps/pps
                return std::make_shared<H264Track>();
            }
            return std::make_shared<H264Track>(sps, pps, 0, 0);
        }

        case CodecH265: {
            //a=fmtp:96 sprop-sps=QgEBAWAAAAMAsAAAAwAAAwBdoAKAgC0WNrkky/AIAAADAAgAAAMBlQg=; sprop-pps=RAHA8vA8kAA=
            auto map = Parser::parseArgs(track->_fmtp, ";", "=");
            auto vps = decodeBase64(map["sprop-vps"]);
            auto sps = decodeBase64(map["sprop-sps"]);
            auto pps = decodeBase64(map["sprop-pps"]);
            if (sps.empty() || pps.empty()) {
                //如果sdp里面没有sps/pps,那么可能在后续的rtp里面恢复出sps/pps
                return std::make_shared<H265Track>();
            }
            return std::make_shared<H265Track>(vps, sps, pps, 0, 0, 0);
        }

        case CodecJPEG : return std::make_shared<JPEGTrack>();

        default: {
            //其他codec不支持
            WarnL << "暂不支持该rtsp编码类型:" << track->getName();
            return nullptr;
        }
    }
}

Track::Ptr Factory::getTrackByAbstractTrack(const Track::Ptr& track) {
    auto codec = track->getCodecId();
    switch (codec) {
        case CodecG711A:
        case CodecG711U: {
            auto audio_track = dynamic_pointer_cast<AudioTrackImp>(track);
            return std::make_shared<G711Track>(codec, audio_track->getAudioSampleRate(), audio_track->getAudioChannel(), 16);
        }
        case CodecL16: {
            auto audio_track = dynamic_pointer_cast<AudioTrackImp>(track);
            return std::make_shared<L16Track>(audio_track->getAudioSampleRate(), audio_track->getAudioChannel());
        }
        case CodecAAC: return std::make_shared<AACTrack>();
        case CodecOpus: return std::make_shared<OpusTrack>();
        case CodecH265: return std::make_shared<H265Track>();
        case CodecH264: return std::make_shared<H264Track>();
        case CodecJPEG: return std::make_shared<JPEGTrack>();

        default: {
            //其他codec不支持
            WarnL << "暂不支持该该编码类型创建Track:" << track->getCodecName();
            return nullptr;
        }
    }
}

RtpCodec::Ptr Factory::getRtpEncoderByCodecId(CodecId codec_id, uint8_t pt) {
    switch (codec_id) {
        case CodecH264: return std::make_shared<H264RtpEncoder>();
        case CodecH265: return std::make_shared<H265RtpEncoder>();
        case CodecAAC: return std::make_shared<AACRtpEncoder>();
        case CodecL16:
        case CodecOpus: return std::make_shared<CommonRtpEncoder>();
        case CodecG711A:
        case CodecG711U: {
            if (pt == Rtsp::PT_PCMA || pt == Rtsp::PT_PCMU) {
                return std::make_shared<G711RtpEncoder>(codec_id, 1);
            }
            return std::make_shared<CommonRtpEncoder>();
        }
        case CodecJPEG: return std::make_shared<JPEGRtpEncoder>();
        default: WarnL << "暂不支持该CodecId:" << codec_id; return nullptr;
    }
}

RtpCodec::Ptr Factory::getRtpDecoderByTrack(const Track::Ptr &track) {
    switch (track->getCodecId()){
        case CodecH264 : return std::make_shared<H264RtpDecoder>();
        case CodecH265 : return std::make_shared<H265RtpDecoder>();
        case CodecAAC : return std::make_shared<AACRtpDecoder>();
        case CodecL16 :
        case CodecOpus :
        case CodecG711A :
        case CodecG711U : return std::make_shared<CommonRtpDecoder>(track->getCodecId());
        case CodecJPEG: return std::make_shared<JPEGRtpDecoder>();
        default : WarnL << "暂不支持该CodecId:" << track->getCodecName(); return nullptr;
    }
}

/////////////////////////////rtmp相关///////////////////////////////////////////

static CodecId getVideoCodecIdByAmf(const AMFValue &val){
    if (val.type() == AMF_STRING) {
        auto str = val.as_string();
        if (str == "avc1") {
            return CodecH264;
        }
        if (str == "hev1" || str == "hvc1") {
            return CodecH265;
        }
        WarnL << "暂不支持该视频Amf:" << str;
        return CodecInvalid;
    }

    if (val.type() != AMF_NULL) {
        auto type_id = (RtmpVideoCodec)val.as_integer();
        switch (type_id) {
            case RtmpVideoCodec::h264: return CodecH264;
            case RtmpVideoCodec::fourcc_hevc:
            case RtmpVideoCodec::h265: return CodecH265;
            case RtmpVideoCodec::fourcc_av1: return CodecAV1;
            case RtmpVideoCodec::fourcc_vp9: return CodecVP9;
            default: WarnL << "暂不支持该视频Amf:" << (int)type_id; return CodecInvalid;
        }
    }
    return CodecInvalid;
}

Track::Ptr Factory::getTrackByCodecId(CodecId codecId, int sample_rate, int channels, int sample_bit) {
    switch (codecId){
        case CodecH264 : return std::make_shared<H264Track>();
        case CodecH265 : return std::make_shared<H265Track>();
        case CodecAAC : return std::make_shared<AACTrack>();
        case CodecOpus: return std::make_shared<OpusTrack>();
        case CodecG711A :
        case CodecG711U : return (sample_rate && channels && sample_bit) ? std::make_shared<G711Track>(codecId, sample_rate, channels, sample_bit) : nullptr;
        case CodecJPEG : return std::make_shared<JPEGTrack>();
        default : WarnL << "暂不支持该CodecId:" << codecId; return nullptr;
    }
}

Track::Ptr Factory::getVideoTrackByAmf(const AMFValue &amf) {
    CodecId codecId = getVideoCodecIdByAmf(amf);
    if(codecId == CodecInvalid){
        return nullptr;
    }
    return getTrackByCodecId(codecId);
}

static CodecId getAudioCodecIdByAmf(const AMFValue &val) {
    if (val.type() == AMF_STRING) {
        auto str = val.as_string();
        if (str == "mp4a") {
            return CodecAAC;
        }
        WarnL << "暂不支持该音频Amf:" << str;
        return CodecInvalid;
    }

    if (val.type() != AMF_NULL) {
        auto type_id = (RtmpAudioCodec)val.as_integer();
        switch (type_id) {
            case RtmpAudioCodec::aac : return CodecAAC;
            case RtmpAudioCodec::g711a : return CodecG711A;
            case RtmpAudioCodec::g711u : return CodecG711U;
            case RtmpAudioCodec::opus : return CodecOpus;
            default : WarnL << "暂不支持该音频Amf:" << (int)type_id; return CodecInvalid;
        }
    }

    return CodecInvalid;
}

Track::Ptr Factory::getAudioTrackByAmf(const AMFValue& amf, int sample_rate, int channels, int sample_bit){
    CodecId codecId = getAudioCodecIdByAmf(amf);
    if (codecId == CodecInvalid) {
        return nullptr;
    }
    return getTrackByCodecId(codecId, sample_rate, channels, sample_bit);
}

RtmpCodec::Ptr Factory::getRtmpDecoderByTrack(const Track::Ptr &track) {
    switch (track->getCodecId()){
        case CodecH264 : return std::make_shared<H264RtmpDecoder>(track);
        case CodecAAC : return std::make_shared<AACRtmpDecoder>(track);
        case CodecH265 : return std::make_shared<H265RtmpDecoder>(track);
        case CodecOpus :
        case CodecG711A :
        case CodecG711U : return std::make_shared<CommonRtmpDecoder>(track);
        default : WarnL << "暂不支持该CodecId:" << track->getCodecName(); return nullptr;
    }
}

RtmpCodec::Ptr Factory::getRtmpEncoderByTrack(const Track::Ptr &track) {
    switch (track->getCodecId()){
        case CodecH264 : return std::make_shared<H264RtmpEncoder>(track);
        case CodecAAC : return std::make_shared<AACRtmpEncoder>(track);
        case CodecH265 : return std::make_shared<H265RtmpEncoder>(track);
        case CodecOpus : return std::make_shared<CommonRtmpEncoder>(track);
        case CodecG711A :
        case CodecG711U : {
            auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
            if (audio_track->getAudioSampleRate() != 8000 || audio_track->getAudioChannel() != 1 || audio_track->getAudioSampleBit() != 16) {
                //rtmp对g711只支持8000/1/16规格，但是ZLMediaKit可以解析其他规格的G711
                WarnL << "RTMP只支持8000/1/16规格的G711,目前规格是:"
                      << audio_track->getAudioSampleRate() << "/"
                      << audio_track->getAudioChannel() << "/"
                      << audio_track->getAudioSampleBit()
                      << ",该音频已被忽略";
                return nullptr;
            }
            return std::make_shared<CommonRtmpEncoder>(track);
        }
        default : WarnL << "暂不支持该CodecId:" << track->getCodecName(); return nullptr;
    }
}

AMFValue Factory::getAmfByCodecId(CodecId codecId) {
    GET_CONFIG(bool, enhanced, Rtmp::kEnhanced);
    switch (codecId) {
        case CodecAAC: return AMFValue((int)RtmpAudioCodec::aac);
        case CodecH264: return AMFValue((int)RtmpVideoCodec::h264);
        case CodecH265: return enhanced ? AMFValue((int)RtmpVideoCodec::fourcc_hevc) : AMFValue((int)RtmpVideoCodec::h265);
        case CodecG711A: return AMFValue((int)RtmpAudioCodec::g711a);
        case CodecG711U: return AMFValue((int)RtmpAudioCodec::g711u);
        case CodecOpus: return AMFValue((int)RtmpAudioCodec::opus);
        case CodecAV1: return AMFValue((int)RtmpVideoCodec::fourcc_av1);
        case CodecVP9: return AMFValue((int)RtmpVideoCodec::fourcc_vp9);
        default: return AMFValue(AMF_NULL);
    }
}

static size_t aacPrefixSize(const char *data, size_t bytes) {
    uint8_t *ptr = (uint8_t *)data;
    size_t prefix = 0;
    if (!(bytes > ADTS_HEADER_LEN && ptr[0] == 0xFF && (ptr[1] & 0xF0) == 0xF0)) {
        return 0;
    }
    return ADTS_HEADER_LEN;
}

Frame::Ptr Factory::getFrameFromPtr(CodecId codec, const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    switch (codec) {
        case CodecH264: return std::make_shared<H264FrameNoCacheAble>((char *)data, bytes, dts, pts, prefixSize(data, bytes));
        case CodecH265: return std::make_shared<H265FrameNoCacheAble>((char *)data, bytes, dts, pts, prefixSize(data, bytes));
        case CodecJPEG: return std::make_shared<JPEGFrame<FrameFromPtr>>(0, codec, (char *)data, bytes, dts, pts);
        case CodecAAC: return std::make_shared<FrameFromPtr>(codec, (char *)data, bytes, dts, pts, aacPrefixSize(data, bytes));
        case CodecOpus:
        case CodecG711A:
        case CodecG711U: return std::make_shared<FrameFromPtr>(codec, (char *)data, bytes, dts, pts);
        default: return nullptr;
    }
}

Frame::Ptr Factory::getFrameFromBuffer(CodecId codec, Buffer::Ptr data, uint64_t dts, uint64_t pts) {
    auto frame =  Factory::getFrameFromPtr(codec, data->data(), data->size(), dts, pts);
    return std::make_shared<FrameCacheAble>(frame, false, std::move(data));
}

}//namespace mediakit

