/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Opus.h"
#include "Extension/Factory.h"
#include "Extension/CommonRtp.h"
#include "OpusRtmp.h"
#include "opus-head.h"
using namespace std;
using namespace toolkit;

namespace mediakit {

void OpusTrack::setExtraData(const uint8_t *data, size_t size) {
    opus_head_t header;
    if (opus_head_load(data, size, &header) > 0) {
        // Successfully parsed Opus header
        _sample_rate = header.input_sample_rate;
        _channels = header.channels;
    }
}

Buffer::Ptr OpusTrack::getExtraData() const {
    struct opus_head_t opus = { 0 };
    opus.version = 1;
    opus.channels = getAudioChannel();
    opus.input_sample_rate = getAudioSampleRate();
    // opus.pre_skip = 120;
    opus.channel_mapping_family = 0;
    auto ret = BufferRaw::create(29);
    ret->setSize(opus_head_save(&opus, (uint8_t *)ret->data(), ret->getCapacity()));
    return ret;
}

namespace {

CodecId getCodec() {
    return CodecOpus;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<OpusTrack>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    return std::make_shared<OpusTrack>();
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<CommonRtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<CommonRtpDecoder>(CodecOpus);
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<OpusRtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<OpusRtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<FrameFromPtr>(CodecOpus, (char *)data, bytes, dts, pts);
}

} // namespace

CodecPlugin opus_plugin = { getCodec,
                            getTrackByCodecId,
                            getTrackBySdp,
                            getRtpEncoderByCodecId,
                            getRtpDecoderByCodecId,
                            getRtmpEncoderByTrack,
                            getRtmpDecoderByTrack,
                            getFrameFromPtr };

}//namespace mediakit