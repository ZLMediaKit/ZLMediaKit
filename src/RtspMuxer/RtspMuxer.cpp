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
#include "Common/Factory.h"

namespace mediakit {

void RtspMuxer::addTrack(const Track::Ptr &track, uint32_t ssrc, int mtu) {
    auto codec_id = track->getCodecId();
    _track_map[codec_id] = track;

    auto lam = [this,ssrc,mtu,track](){
        //异步生成rtp编码器
        //根据track生产sdp
        Sdp::Ptr sdp = Factory::getSdpByTrack(track);
        if (!sdp) {
            return;
        }

        // 根据sdp生成rtp编码器
        auto encoder = sdp->createRtpEncoder(ssrc ? ssrc : ((uint64_t) sdp.get()) & 0xFFFFFFFF, mtu);
        if (!encoder) {
            return;
        }
        //添加其sdp
        _sdp.append(sdp->getSdp());
        //设置Track的代理，这样输入frame至Track时，最终数据将输出到RtpEncoder中
        track->addDelegate(encoder);
        //rtp编码器共用同一个环形缓存
        encoder->setRtpRing(_rtpRing);
    };
    if(track->ready()){
        lam();
    }else{
        _trackReadyCallback[codec_id] = lam;
    }
}

string RtspMuxer::getSdp() {
    if(!_trackReadyCallback.empty()){
        //尚未就绪
        return "";
    }
    return _sdp;
}


void RtspMuxer::inputFrame(const Frame::Ptr &frame) {
    auto codec_id = frame->getCodecId();
    auto it = _track_map.find(codec_id);
    if (it == _track_map.end()) {
        return;
    }
    //Track是否准备好
    auto ready = it->second->ready();
    //inputFrame可能使Track变成就绪状态
    it->second->inputFrame(frame);

    if(!ready && it->second->ready()){
        //Track由未就绪状态装换成就绪状态，我们就生成sdp以及rtp编码器
        auto it_callback = _trackReadyCallback.find(codec_id);
        if(it_callback != _trackReadyCallback.end()){
            it_callback->second();
            _trackReadyCallback.erase(it_callback);
        }
    }

    if(!_inited && _trackReadyCallback.empty()){
        _inited = true;
        onInited();
    }
}

bool RtspMuxer::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
   _rtpRing->write(rtp,key_pos);
    return key_pos;
}

RtpRingInterface::RingType::Ptr RtspMuxer::getRtpRing() const {
    return _rtpRing;
}

} /* namespace mediakit */