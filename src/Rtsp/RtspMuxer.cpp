/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtspMuxer.h"
#include "Extension/Factory.h"

namespace mediakit {

RtspMuxer::RtspMuxer(const TitleSdp::Ptr &title){
    if(!title){
        _sdp = std::make_shared<TitleSdp>()->getSdp();
    } else{
        _sdp = title->getSdp();
    }
    _rtpRing = std::make_shared<RtpRing::RingType>();
}

void RtspMuxer::addTrack(const Track::Ptr &track) {
    //根据track生成sdp
    Sdp::Ptr sdp = track->getSdp();
    if (!sdp) {
        return;
    }

    auto &encoder = _encoder[track->getTrackType()];
    encoder = Factory::getRtpEncoderBySdp(sdp);
    if (!encoder) {
        return;
    }

    //设置rtp输出环形缓存
    encoder->setRtpRing(_rtpRing);

    //添加其sdp
    _sdp.append(sdp->getSdp());
}

void RtspMuxer::inputFrame(const Frame::Ptr &frame) {
    auto &encoder = _encoder[frame->getTrackType()];
    if(encoder){
        encoder->inputFrame(frame);
    }
}

string RtspMuxer::getSdp() {
    return _sdp;
}

RtpRing::RingType::Ptr RtspMuxer::getRtpRing() const {
    return _rtpRing;
}

void RtspMuxer::resetTracks() {
    _sdp.clear();
    for(auto &encoder : _encoder){
        encoder = nullptr;
    }
}


} /* namespace mediakit */