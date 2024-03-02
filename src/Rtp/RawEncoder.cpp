/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)

#include "RawEncoder.h"
#include "Extension/Factory.h"
#include "Rtsp/RtspMuxer.h"
#include "Common//config.h"

using namespace toolkit;

namespace mediakit {

RawEncoderImp::RawEncoderImp(uint32_t ssrc, uint8_t payload_type, bool send_audio)
    : _send_audio(send_audio)
    , _payload_type(payload_type)
    , _ssrc(ssrc) {}

RawEncoderImp::~RawEncoderImp() {
    InfoL << this << " " << printSSRC(_ssrc);
}

bool RawEncoderImp::addTrack(const Track::Ptr &track) {
    if (_send_audio && track->getTrackType() == TrackType::TrackAudio && !_rtp_encoder) { // audio
        _rtp_encoder = createRtpEncoder(track);
        auto ring = std::make_shared<RtpRing::RingType>();
        ring->setDelegate(std::make_shared<RingDelegateHelper>([this](RtpPacket::Ptr rtp, bool is_key) { onRTP(std::move(rtp), true); }));
        _rtp_encoder->setRtpRing(std::move(ring));
        if (track->getCodecId() == CodecG711A || track->getCodecId() == CodecG711U) {
            GET_CONFIG(uint32_t, dur_ms, RtpProxy::kRtpG711DurMs);
            Any param;
            param.set<uint32_t>(dur_ms);
            _rtp_encoder->setOpt(RtpCodec::RTP_ENCODER_PKT_DUR_MS, param);
        }
        return true;
    }

    if (!_send_audio && track->getTrackType() == TrackType::TrackVideo && !_rtp_encoder) {
        _rtp_encoder = createRtpEncoder(track);
        auto ring = std::make_shared<RtpRing::RingType>();
        ring->setDelegate(std::make_shared<RingDelegateHelper>([this](RtpPacket::Ptr rtp, bool is_key) { onRTP(std::move(rtp), is_key); }));
        _rtp_encoder->setRtpRing(std::move(ring));
        return true;
    }
    return true;
}

void RawEncoderImp::resetTracks() {
    return;
}

bool RawEncoderImp::inputFrame(const Frame::Ptr &frame) {
    if (frame->getTrackType() == TrackType::TrackAudio && _send_audio && _rtp_encoder) {
        _rtp_encoder->inputFrame(frame);
    }

    if (frame->getTrackType() == TrackType::TrackVideo && !_send_audio && _rtp_encoder) {
        _rtp_encoder->inputFrame(frame);
    }
    return true;
}

RtpCodec::Ptr RawEncoderImp::createRtpEncoder(const Track::Ptr &track) {
    GET_CONFIG(uint32_t, audio_mtu, Rtp::kAudioMtuSize);
    GET_CONFIG(uint32_t, video_mtu, Rtp::kVideoMtuSize);
    auto sample_rate = 90000u;
    auto mtu = video_mtu;
    if (track->getTrackType() == TrackType::TrackAudio) {
        mtu = audio_mtu;
        sample_rate = std::static_pointer_cast<AudioTrack>(track)->getAudioSampleRate();
    }
    auto ret = Factory::getRtpEncoderByCodecId(track->getCodecId(), _payload_type);
    ret->setRtpInfo(_ssrc, mtu, sample_rate, _payload_type);
    return ret;
}

} // namespace mediakit

#endif // defined(ENABLE_RTPPROXY)
