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
#include "VpxRtmp.h"
#include "Extension/Factory.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

bool AV1Track::inputFrame(const Frame::Ptr &frame) {
    char *dataPtr = frame->data() + frame->prefixSize();
    // init() appends configuration OBUs and may partially modify its context on
    // failure. Parse into a fresh context; ordinary inter frames have no dimensions.
    aom_av1_t context {};
    if (0 == aom_av1_codec_configuration_record_init(&context, dataPtr, frame->size() - frame->prefixSize())
        && context.width && context.height) {
        _context = context;
        _width = context.width;
        _height = context.height;
    }
    return VideoTrackImp::inputFrame(frame);
}

Track::Ptr AV1Track::clone() const {
    return std::make_shared<AV1Track>(*this);
}

Buffer::Ptr AV1Track::getExtraData() const {
    if (_context.bytes <= 0)
        return nullptr;
    auto ret = BufferRaw::create(4 + _context.bytes);
    ret->setSize(aom_av1_codec_configuration_record_save(&_context, (uint8_t *)ret->data(), ret->getCapacity()));
    return ret;
}

void AV1Track::setExtraData(const uint8_t *data, size_t size) {
    aom_av1_t context {};
    if (size < 4 || data[0] != 0x81
        || aom_av1_codec_configuration_record_load(data, size, &context) <= 0) {
        return;
    }
    // load() only copies av1C fields/configOBUs; dimensions come from the
    // sequence header inside configOBUs. Use a separate context to avoid
    // appending into the same buffer that init() is reading.
    aom_av1_t parsed {};
    if (context.bytes && aom_av1_codec_configuration_record_init(&parsed, context.data, context.bytes) != 0) {
        return;
    }
    _context = context;
    _context.width = _width = parsed.width;
    _context.height = _height = parsed.height;
}

namespace {

CodecId getCodec() {
    return CodecAV1;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<AV1Track>();
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
    return std::make_shared<VpxRtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<VpxRtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<AV1FrameNoCacheAble>((char *)data, bytes, dts, pts, 0);
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

} // namespace mediakit