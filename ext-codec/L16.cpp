/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "L16.h"
#include "Extension/Factory.h"
#include "Extension/CommonRtp.h"
#include "Extension/CommonRtmp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

/**
 * L16类型SDP
 */
class L16Sdp : public Sdp {
public:
    /**
     * L16采样位数固定为16位
     * @param payload_type rtp payload type
     * @param channels 通道数
     * @param sample_rate 音频采样率
     * @param bitrate 比特率
     */
    L16Sdp(int payload_type, int sample_rate, int channels, int bitrate) : Sdp(sample_rate, payload_type) {
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecL16) << "/" << sample_rate  << "/" << channels << "\r\n";
    }

    string getSdp() const override { return _printer; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr L16Track::getSdp(uint8_t payload_type) const {
    WarnL << "Enter  L16Track::getSdp function";
    if (!ready()) {
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<L16Sdp>(payload_type, getAudioSampleRate(), getAudioChannel(), getBitRate() / 1024);
}

Track::Ptr L16Track::clone() const {
    return std::make_shared<L16Track>(*this);
}

namespace {

CodecId getCodec() {
    return CodecL16;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<L16Track>(sample_rate, channels);
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    return std::make_shared<L16Track>(track->_samplerate, track->_channel);
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<CommonRtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<CommonRtpDecoder>(CodecL16);
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    WarnL << "Unsupported L16 rtmp encoder";
    return nullptr;
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    WarnL << "Unsupported L16 rtmp decoder";
    return nullptr;
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<FrameFromPtr>(CodecL16, (char *)data, bytes, dts, pts);
}

} // namespace

CodecPlugin l16_plugin = { getCodec,
                           getTrackByCodecId,
                           getTrackBySdp,
                           getRtpEncoderByCodecId,
                           getRtpDecoderByCodecId,
                           getRtmpEncoderByTrack,
                           getRtmpDecoderByTrack,
                           getFrameFromPtr };

}//namespace mediakit


