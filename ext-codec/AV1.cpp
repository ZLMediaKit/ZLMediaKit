/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AV1.h"
#include "AV1Rtp.h"
#include "Extension/Factory.h"
#include "Extension/CommonRtp.h"
#include "Extension/CommonRtmp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

Sdp::Ptr AV1Track::getSdp(uint8_t payload_type) const {
    return std::make_shared<DefaultSdp>(payload_type, *this);
}

Track::Ptr AV1Track::clone() const {
    return std::make_shared<AV1Track>(*this);
}

namespace {

CodecId getCodec() {
    return CodecAV1;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    // AV1是视频编解码器，这里的参数实际上是width, height, fps
    return std::make_shared<AV1Track>(sample_rate, channels, sample_bit);
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    return std::make_shared<AV1Track>();
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<AV1RtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<AV1RtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<CommonRtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<CommonRtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<FrameFromPtr>(CodecAV1, (char *)data, bytes, dts, pts);
}

} // namespace

CodecPlugin av1_plugin = { getCodec,
                           getTrackByCodecId,
                           getTrackBySdp,
                           getRtpEncoderByCodecId,
                           getRtpDecoderByCodecId,
                           getRtmpEncoderByTrack,
                           getRtmpDecoderByTrack,
                           getFrameFromPtr };

}//namespace mediakit