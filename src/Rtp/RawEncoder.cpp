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
#include "Extension/Factory.h"
#include "Rtsp/RtspMuxer.h"

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
        _rtp_encoder->setRtpRing(std::make_shared<RtpRing::RingType>());
        _rtp_encoder->getRtpRing()->setDelegate(std::make_shared<RingDelegateHelper>(
            [this](RtpPacket::Ptr rtp, bool is_key) { onRTP(std::move(rtp), true); }));
        return true;
    }

    if (!_send_audio && track->getTrackType() == TrackType::TrackVideo && !_rtp_encoder) {
        _rtp_encoder = createRtpEncoder(track);
        _rtp_encoder->setRtpRing(std::make_shared<RtpRing::RingType>());
        _rtp_encoder->getRtpRing()->setDelegate(std::make_shared<RingDelegateHelper>(
            [this](RtpPacket::Ptr rtp, bool is_key) { onRTP(std::move(rtp), is_key); }));
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
    uint32_t sample_rate = 90000;
    if (track->getTrackType() == TrackType::TrackAudio) {
        sample_rate = std::static_pointer_cast<AudioTrack>(track)->getAudioSampleRate();
    }
    return Factory::getRtpEncoderByCodecId(track->getCodecId(), sample_rate, _payload_type, _ssrc);
}

} // namespace mediakit

#endif // defined(ENABLE_RTPPROXY)
