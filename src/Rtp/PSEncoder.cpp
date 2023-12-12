/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)

#include "PSEncoder.h"
#include "Common/config.h"
#include "Extension/CommonRtp.h"
#include "Rtsp/RtspMuxer.h"

using namespace toolkit;

namespace mediakit{

PSEncoderImp::PSEncoderImp(uint32_t ssrc, uint8_t payload_type) : MpegMuxer(true) {
    GET_CONFIG(uint32_t,video_mtu,Rtp::kVideoMtuSize);
    _rtp_encoder = std::make_shared<CommonRtpEncoder>();
    _rtp_encoder->setRtpInfo(ssrc, video_mtu, 90000, payload_type);
    auto ring = std::make_shared<RtpRing::RingType>();
    ring->setDelegate(std::make_shared<RingDelegateHelper>([this](RtpPacket::Ptr rtp, bool is_key) { onRTP(std::move(rtp), is_key); }));
    _rtp_encoder->setRtpRing(std::move(ring));
    InfoL << this << " " << ssrc;
}

PSEncoderImp::~PSEncoderImp() {
    InfoL << this;
}

void PSEncoderImp::onWrite(std::shared_ptr<Buffer> buffer, uint64_t stamp, bool key_pos) {
    if (!buffer) {
        return;
    }
    _rtp_encoder->inputFrame(std::make_shared<FrameFromPtr>(CodecH264/*只用于识别为视频*/, buffer->data(), buffer->size(), stamp, stamp, 0, key_pos));
}

}//namespace mediakit

#endif//defined(ENABLE_RTPPROXY)
