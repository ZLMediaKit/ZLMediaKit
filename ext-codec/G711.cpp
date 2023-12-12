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

using namespace std;
using namespace toolkit;

namespace mediakit {

/**
 * G711类型SDP
 */
class G711Sdp : public Sdp {
public:
    /**
     * G711采样率固定为8000
     * @param codecId G711A G711U
     * @param payload_type rtp payload type
     * @param sample_rate 音频采样率
     * @param channels 通道数
     * @param bitrate 比特率
     */
    G711Sdp(CodecId codecId, int payload_type, int sample_rate, int channels, int bitrate)
        : Sdp(sample_rate, payload_type) {
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(codecId) << "/" << sample_rate  << "/" << channels << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

private:
    _StrPrinter _printer;
};

Track::Ptr G711Track::clone() const {
    return std::make_shared<G711Track>(*this);
}

Sdp::Ptr G711Track::getSdp(uint8_t payload_type) const {
    if (!ready()) {
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }

    const auto codec = getCodecId();
    const auto sample_rate = getAudioSampleRate();
    const auto audio_channel = getAudioChannel();
    const auto bitrate = getBitRate() >> 10;
    if (sample_rate == 8000 && audio_channel == 1) {
        // https://datatracker.ietf.org/doc/html/rfc3551#section-6
        payload_type = (codec == CodecG711U) ? Rtsp::PT_PCMU : Rtsp::PT_PCMA;
    }

    return std::make_shared<G711Sdp>(codec, payload_type, sample_rate, audio_channel, bitrate);
}


namespace {

CodecId getCodecA() {
    return CodecG711A;
}

CodecId getCodecU() {
    return CodecG711U;
}

Track::Ptr getTrackByCodecId_l(CodecId codec, int sample_rate, int channels, int sample_bit) {
    return (sample_rate && channels && sample_bit) ? std::make_shared<G711Track>(codec, sample_rate, channels, sample_bit) : nullptr;
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
        return std::make_shared<G711RtpEncoder>(codec, 1);
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
        //rtmp对g711只支持8000/1/16规格，但是ZLMediaKit可以解析其他规格的G711
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


