/*
* MIT License
*
* Copyright (c) 2016 xiongziliang <771730766@qq.com>
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
    _rtpRing = std::make_shared<RtpRingInterface::RingType>();
}

void RtspMuxer::onTrackReady(const Track::Ptr &track) {
    //根据track生产sdp
    Sdp::Ptr sdp = Factory::getSdpByTrack(track);
    if (!sdp) {
        return;
    }
    uint32_t ssrc = ((uint64_t) sdp.get()) & 0xFFFFFFFF;

    GET_CONFIG_AND_REGISTER(uint32_t,audio_mtu,Rtp::kAudioMtuSize);
    GET_CONFIG_AND_REGISTER(uint32_t,video_mtu,Rtp::kVideoMtuSize);

    auto mtu = (track->getTrackType() == TrackVideo ? video_mtu : audio_mtu);
    // 根据sdp生成rtp编码器ssrc
    auto encoder = sdp->createRtpEncoder(ssrc, mtu);
    if (!encoder) {
        return;
    }
    //添加其sdp
    _sdp.append(sdp->getSdp());
    //设置Track的代理，这样输入frame至Track时，最终数据将输出到RtpEncoder中
    track->addDelegate(encoder);
    //rtp编码器共用同一个环形缓存
    encoder->setRtpRing(_rtpRing);
}

string RtspMuxer::getSdp() {
    if(!isAllTrackReady()){
        //尚未就绪
        return "";
    }
    return _sdp;
}

RtpRingInterface::RingType::Ptr RtspMuxer::getRtpRing() const {
    return _rtpRing;
}


} /* namespace mediakit */