/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "MP2V.h"
#include "MP2VRtp.h"
#include "Extension/Factory.h"
#include "Rtsp/Rtsp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

// MPEG-2 sequence header 帧率表 (ISO 13818-2 Table 6-4)
// MPEG-2 sequence header frame rate table
static const float s_mp2v_frame_rate_table[] = {
    0,       // 0000 forbidden
    24000.0 / 1001, // 0001 23.976
    24.0,    // 0010
    25.0,    // 0011
    30000.0 / 1001, // 0100 29.97
    30.0,    // 0101
    50.0,    // 0110
    60000.0 / 1001, // 0111 59.94
    60.0,    // 1000
};

void MP2VTrack::parseSequenceHeader(const uint8_t *data, size_t size) {
    // 查找 sequence header start code: 00 00 01 B3
    // Look for sequence header start code: 00 00 01 B3
    for (size_t i = 0; i + 7 < size; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01 && data[i + 3] == 0xB3) {
            // sequence_header() 结构:
            // horizontal_size_value: 12 bits
            // vertical_size_value: 12 bits
            // aspect_ratio_information: 4 bits
            // frame_rate_code: 4 bits
            _width = (data[i + 4] << 4) | ((data[i + 5] >> 4) & 0x0F);
            _height = ((data[i + 5] & 0x0F) << 8) | data[i + 6];
            uint8_t frame_rate_code = data[i + 7] & 0x0F;
            if (frame_rate_code > 0 && frame_rate_code <= 8) {
                _fps = s_mp2v_frame_rate_table[frame_rate_code];
            }
            _seq_header_parsed = true;
            return;
        }
    }
}

bool MP2VTrack::inputFrame(const Frame::Ptr &frame) {
    if (!_seq_header_parsed) {
        parseSequenceHeader((const uint8_t *)frame->data() + frame->prefixSize(),
                            frame->size() - frame->prefixSize());
    }
    return VideoTrackImp::inputFrame(frame);
}

Sdp::Ptr MP2VTrack::getSdp(uint8_t pt) const {
    return std::make_shared<DefaultSdp>(pt, *this);
}

namespace {

CodecId getCodec() {
    return CodecMP2V;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<MP2VTrack>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    return std::make_shared<MP2VTrack>();
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<MP2VRtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<MP2VRtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    WarnL << "Unsupported MP2V rtmp encoder";
    return nullptr;
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    WarnL << "Unsupported MP2V rtmp decoder";
    return nullptr;
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<MP2VFrameNoCacheAble>((char *)data, bytes, dts, pts, 0);
}

} // namespace

CodecPlugin mp2v_plugin = { getCodec,
                             getTrackByCodecId,
                             getTrackBySdp,
                             getRtpEncoderByCodecId,
                             getRtpDecoderByCodecId,
                             getRtmpEncoderByTrack,
                             getRtmpDecoderByTrack,
                             getFrameFromPtr };

} // namespace mediakit
