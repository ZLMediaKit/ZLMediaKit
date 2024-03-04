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

PSEncoderImp::PSEncoderImp(uint32_t ssrc, uint8_t payload_type, bool ps_or_ts) : MpegMuxer(ps_or_ts) {
    GET_CONFIG(uint32_t, s_video_mtu, Rtp::kVideoMtuSize);
    _rtp_encoder = std::make_shared<CommonRtpEncoder>();
    auto video_mtu = s_video_mtu;
    if (!ps_or_ts) {
        // 确保ts rtp负载部分长度是188的倍数
        video_mtu = RtpPacket::kRtpHeaderSize + (s_video_mtu - (s_video_mtu % 188));
        if (video_mtu > s_video_mtu) {
            video_mtu -= 188;
        }
    }
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
