/*
* MIT License
*
* Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
*
* This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
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