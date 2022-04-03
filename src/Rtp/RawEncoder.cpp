/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)

#include "RawEncoder.h"
#include "Extension/H264Rtp.h"
#include "Extension/AACRtp.h"
#include "Extension/H265Rtp.h"
#include "Extension/CommonRtp.h"
#include "Extension/G711Rtp.h"
#include "Rtsp/RtspMuxer.h"

using namespace toolkit;

namespace mediakit{

RawEncoderImp::RawEncoderImp(uint32_t ssrc, uint8_t payload_type,bool sendAudio):_ssrc(ssrc),_payload_type(payload_type),_sendAudio(sendAudio) {

}

RawEncoderImp::~RawEncoderImp() {
    InfoL << this << " " << printSSRC(_ssrc);
}

bool RawEncoderImp::addTrack(const Track::Ptr &track){
    if(_sendAudio && track->getTrackType() == TrackType::TrackAudio && !_rtp_encoder){// audio
        _rtp_encoder = createRtpEncoder(track);
        _rtp_encoder->setRtpRing(std::make_shared<RtpRing::RingType>());
        _rtp_encoder->getRtpRing()->setDelegate(std::make_shared<RingDelegateHelper>([this](RtpPacket::Ptr rtp, bool is_key){
            onRTP(std::move(rtp));
         }));
        return true;
    }

    if(!_sendAudio && track->getTrackType()==TrackType::TrackVideo && !_rtp_encoder){
         _rtp_encoder = createRtpEncoder(track);
         _rtp_encoder->setRtpRing(std::make_shared<RtpRing::RingType>());
        _rtp_encoder->getRtpRing()->setDelegate(std::make_shared<RingDelegateHelper>([this](RtpPacket::Ptr rtp, bool is_key){
            onRTP(std::move(rtp));
         }));
        return true;
    }
    return true;
}


void RawEncoderImp::resetTracks(){
    return;
}

  
bool RawEncoderImp::inputFrame(const Frame::Ptr &frame){
    if(frame->getTrackType() == TrackType::TrackAudio && _sendAudio && _rtp_encoder){
        _rtp_encoder->inputFrame(frame);
    }

    if(frame->getTrackType() == TrackType::TrackVideo && !_sendAudio && _rtp_encoder){
         _rtp_encoder->inputFrame(frame);
    }
    return true;
}

RtpCodec::Ptr RawEncoderImp::createRtpEncoder(const Track::Ptr &track){
    GET_CONFIG(uint32_t,audio_mtu,Rtp::kAudioMtuSize);
    GET_CONFIG(uint32_t,video_mtu,Rtp::kVideoMtuSize);
    auto codec_id = track->getCodecId();
    uint32_t sample_rate = 90000;
    int channels = 1;
    auto mtu = (track->getTrackType() == TrackVideo ? video_mtu : audio_mtu);
    if(track->getTrackType() == TrackType::TrackAudio){
        AudioTrack::Ptr audioTrack = std::dynamic_pointer_cast<AudioTrack>(track);
        sample_rate = audioTrack->getAudioSampleRate();
        channels = audioTrack->getAudioChannel();
    }
    switch (codec_id){
        case CodecH264 : return std::make_shared<H264RtpEncoder>(_ssrc, mtu, sample_rate, _payload_type, 0);
        case CodecH265 : return std::make_shared<H265RtpEncoder>(_ssrc, mtu, sample_rate, _payload_type, 0);
        case CodecAAC : return std::make_shared<AACRtpEncoder>(_ssrc, mtu, sample_rate, _payload_type, 0);
        case CodecL16 :
        case CodecOpus : return std::make_shared<CommonRtpEncoder>(codec_id, _ssrc, mtu, sample_rate, _payload_type, 0);
        case CodecG711A :
        case CodecG711U : {
            if (_payload_type == Rtsp::PT_PCMA || _payload_type == Rtsp::PT_PCMU) {
                return std::make_shared<G711RtpEncoder>(codec_id, _ssrc, mtu, sample_rate, _payload_type, 0, channels);
            }
            return std::make_shared<CommonRtpEncoder>(codec_id, _ssrc, mtu, sample_rate, _payload_type, 0);
        }
        default : WarnL << "暂不支持该CodecId:" << codec_id; return nullptr;
    }
}

}//namespace mediakit

#endif//defined(ENABLE_RTPPROXY)
