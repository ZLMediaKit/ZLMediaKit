/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "G711.h"
#include "G711Rtp.h"
#include "Extension/Factory.h"
#include "Extension/CommonRtp.h"
#include "Extension/CommonRtmp.h"
#include "riff-acm.h"
using namespace std;
using namespace toolkit;

namespace mediakit {

Buffer::Ptr G711Track::getExtraData() const {
    struct wave_format_t wav = {0};
    wav.wFormatTag = getCodecId() == CodecG711A ? WAVE_FORMAT_ALAW : WAVE_FORMAT_MULAW;
    wav.nChannels = getAudioChannel();
    wav.nSamplesPerSec = getAudioSampleRate();
    wav.nAvgBytesPerSec = 8000;
    wav.nBlockAlign = 1;
    wav.wBitsPerSample = 8;
    auto buff = BufferRaw::create(18 + wav.cbSize);
    wave_format_save(&wav, (uint8_t*)buff->data(), buff->size());
    return buff;
}

void G711Track::setExtraData(const uint8_t *data, size_t size) {
    struct wave_format_t wav;
    if (wave_format_load(data, size, &wav) > 0) {
        // Successfully parsed Opus header
        _sample_rate = wav.nSamplesPerSec;
        _channels = wav.nChannels;
        _codecid = (wav.wFormatTag == WAVE_FORMAT_ALAW) ? CodecG711A : CodecG711U;
    } else {
        WarnL << "Failed to parse G711 extra data";
    }
}

namespace {

CodecId getCodecA() {
    return CodecG711A;
}

CodecId getCodecU() {
    return CodecG711U;
}

Track::Ptr getTrackByCodecId_l(CodecId codec, int sample_rate, int channels, int sample_bit) {
    return std::make_shared<G711Track>(codec, sample_rate, 1, 16);
}

Track::Ptr getTrackByCodecIdA(int sample_rate, int channels, int sample_bit) {
    return getTrackByCodecId_l(CodecG711A, sample_rate, channels, sample_bit);
}

Track::Ptr getTrackByCodecIdU(int sample_rate, int channels, int sample_bit) {
    return getTrackByCodecId_l(CodecG711U, sample_rate, channels, sample_bit);
}

Track::Ptr getTrackBySdp_l(CodecId codec, const SdpTrack::Ptr &track) {
    return std::make_shared<G711Track>(codec, track->_samplerate, track->_channel, 16);
}

Track::Ptr getTrackBySdpA(const SdpTrack::Ptr &track) {
    return getTrackBySdp_l(CodecG711A, track);
}

Track::Ptr getTrackBySdpU(const SdpTrack::Ptr &track) {
    return getTrackBySdp_l(CodecG711U, track);
}

RtpCodec::Ptr getRtpEncoderByCodecId_l(CodecId codec, uint8_t pt) {
    if (pt == Rtsp::PT_PCMA || pt == Rtsp::PT_PCMU) {
        return std::make_shared<G711RtpEncoder>(8000, 1);
    }
    return std::make_shared<CommonRtpEncoder>();
}

RtpCodec::Ptr getRtpEncoderByCodecIdA(uint8_t pt) {
    return getRtpEncoderByCodecId_l(CodecG711A, pt);
}

RtpCodec::Ptr getRtpEncoderByCodecIdU(uint8_t pt) {
    return getRtpEncoderByCodecId_l(CodecG711U, pt);
}

RtpCodec::Ptr getRtpDecoderByCodecId_l(CodecId codec) {
    return std::make_shared<CommonRtpDecoder>(codec);
}

RtpCodec::Ptr getRtpDecoderByCodecIdA() {
    return getRtpDecoderByCodecId_l(CodecG711A);
}

RtpCodec::Ptr getRtpDecoderByCodecIdU() {
    return getRtpDecoderByCodecId_l(CodecG711U);
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
    if (audio_track->getAudioSampleRate() != 8000 || audio_track->getAudioChannel() != 1 || audio_track->getAudioSampleBit() != 16) {
        // rtmp对g711只支持8000/1/16规格，但是ZLMediaKit可以解析其他规格的G711  [AUTO-TRANSLATED:0ddeaafe]
        // rtmp only supports 8000/1/16 specifications for g711, but ZLMediaKit can parse other specifications of G711
        WarnL << "RTMP only support G711 with 8000/1/16, now is"
              << audio_track->getAudioSampleRate() << "/"
              << audio_track->getAudioChannel() << "/"
              << audio_track->getAudioSampleBit()
              << ", ignored it";
        return nullptr;
    }
    return std::make_shared<CommonRtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<CommonRtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr_l(CodecId codec, const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<FrameFromPtr>(codec, (char *)data, bytes, dts, pts);
}

Frame::Ptr getFrameFromPtrA(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return getFrameFromPtr_l(CodecG711A, (char *)data, bytes, dts, pts);
}

Frame::Ptr getFrameFromPtrU(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return getFrameFromPtr_l(CodecG711U, (char *)data, bytes, dts, pts);
}

} // namespace

CodecPlugin g711a_plugin = { getCodecA,
                             getTrackByCodecIdA,
                             getTrackBySdpA,
                             getRtpEncoderByCodecIdA,
                             getRtpDecoderByCodecIdA,
                             getRtmpEncoderByTrack,
                             getRtmpDecoderByTrack,
                             getFrameFromPtrA };

CodecPlugin g711u_plugin = { getCodecU,
                             getTrackByCodecIdU,
                             getTrackBySdpU,
                             getRtpEncoderByCodecIdU,
                             getRtpDecoderByCodecIdU,
                             getRtmpEncoderByTrack,
                             getRtmpDecoderByTrack,
                             getFrameFromPtrU };

}//namespace mediakit


