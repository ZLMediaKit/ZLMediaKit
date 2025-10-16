/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_VPX_RTMPCODEC_H
#define ZLMEDIAKIT_VPX_RTMPCODEC_H

#include "Rtmp/RtmpCodec.h"
#include "Extension/Track.h"

namespace mediakit {
/**
 * Rtmp解码类
 * 将 Vpx over rtmp 解复用出 VpxFrame
 */
class VpxRtmpDecoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<VpxRtmpDecoder>;

    VpxRtmpDecoder(const Track::Ptr &track) : RtmpCodec(track) {}

    void inputRtmp(const RtmpPacket::Ptr &rtmp) override;

protected:
    void outputFrame(const char *data, size_t size, uint32_t dts, uint32_t pts);

protected:
    RtmpPacketInfo _info;
};

/**
 * Rtmp打包类
 */
class VpxRtmpEncoder : public RtmpCodec {
    bool _enhanced = false;
public:
    using Ptr = std::shared_ptr<VpxRtmpEncoder>;

    VpxRtmpEncoder(const Track::Ptr &track);

    bool inputFrame(const Frame::Ptr &frame) override;

    void makeConfigPacket() override;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_VPX_RTMPCODEC_H
