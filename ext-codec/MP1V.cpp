/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "MP1V.h"
#include "MP2VRtp.h"
#include "Extension/Factory.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

bool MP1VTrack::inputFrame(const Frame::Ptr &frame) {
    if (!frame) {
        return false;
    }
    auto size = frame->size();
    auto prefix_size = frame->prefixSize();
    auto data = frame->data();
    if (!data || prefix_size > size) {
        return false;
    }
    if (!_sequence_parser.parsed()
        && _sequence_parser.input((const uint8_t *)data + prefix_size, size - prefix_size)) {
        _width = _sequence_parser.width();
        _height = _sequence_parser.height();
        _fps = _sequence_parser.fps();
    }
    return VideoTrackImp::inputFrame(frame);
}

Sdp::Ptr MP1VTrack::getSdp(uint8_t pt) const {
    // 当前只接通 TS/PS；静态 RTP PT 32 仍由 CodecMP2V 表示，不能在此伪装成完整 RTSP 支持。
    // Only TS/PS is wired for now; static RTP PT 32 still maps to CodecMP2V.
    return nullptr;
}

namespace {

CodecId getCodec() {
    return CodecMP1V;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<MP1VTrack>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    WarnL << "Unsupported MP1V sdp track";
    return nullptr;
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    // RFC 2250 的 MPEG Video packetizer 同时适用于 MPEG-1/2。
    // The RFC 2250 MPEG Video packetizer is shared by MPEG-1 and MPEG-2.
    return std::make_shared<MP2VRtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    WarnL << "Unsupported MP1V rtp decoder";
    return nullptr;
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    WarnL << "Unsupported MP1V rtmp encoder";
    return nullptr;
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    WarnL << "Unsupported MP1V rtmp decoder";
    return nullptr;
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<MP1VFrameNoCacheAble>((char *)data, bytes, dts, pts, 0);
}

} // namespace

CodecPlugin mp1v_plugin = { getCodec,
                             getTrackByCodecId,
                             getTrackBySdp,
                             getRtpEncoderByCodecId,
                             getRtpDecoderByCodecId,
                             getRtmpEncoderByTrack,
                             getRtmpDecoderByTrack,
                             getFrameFromPtr };

} // namespace mediakit
