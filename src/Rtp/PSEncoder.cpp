/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)

#include "PSEncoder.h"
#include "Extension/H264.h"
#include "Rtsp/RtspMuxer.h"

using namespace toolkit;

namespace mediakit{

PSEncoderImp::PSEncoderImp(uint32_t ssrc, uint8_t payload_type) : MpegMuxer(true) {
    GET_CONFIG(uint32_t,video_mtu,Rtp::kVideoMtuSize);
    _rtp_encoder = std::make_shared<CommonRtpEncoder>(CodecInvalid, ssrc, video_mtu, 90000, payload_type, 0);
    _rtp_encoder->setRtpRing(std::make_shared<RtpRing::RingType>());
    _rtp_encoder->getRtpRing()->setDelegate(std::make_shared<RingDelegateHelper>([this](RtpPacket::Ptr rtp, bool is_key){
        onRTP(std::move(rtp),is_key);
    }));
    InfoL << this << " " << printSSRC(_rtp_encoder->getSsrc());
}

PSEncoderImp::~PSEncoderImp() {
    InfoL << this << " " << printSSRC(_rtp_encoder->getSsrc());
}

void PSEncoderImp::onWrite(std::shared_ptr<Buffer> buffer, uint64_t stamp, bool key_pos) {
    if (!buffer) {
        return;
    }
    _rtp_encoder->inputFrame(std::make_shared<FrameFromPtr>(buffer->data(), buffer->size(), stamp, stamp,0,key_pos));
}

}//namespace mediakit

#endif//defined(ENABLE_RTPPROXY)
