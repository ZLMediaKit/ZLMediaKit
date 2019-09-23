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

#include "RtmpMuxer.h"
#include "Extension/Factory.h"

namespace mediakit {

RtmpMuxer::RtmpMuxer(const TitleMeta::Ptr &title) {
    if(!title){
        _metadata = std::make_shared<TitleMeta>()->getMetadata();
    }else{
        _metadata = title->getMetadata();
    }
    _rtmpRing = std::make_shared<RtmpRingInterface::RingType>();
}

void RtmpMuxer::onTrackReady(const Track::Ptr &track) {
    //生成rtmp编码器
    //克隆该Track，防止循环引用
    auto encoder = Factory::getRtmpCodecByTrack(track->clone());
    if (!encoder) {
        return;
    }
    //根据track生产metadata
    Metadata::Ptr metadata;
    switch (track->getTrackType()){
        case TrackVideo:{
            metadata = std::make_shared<VideoMeta>(dynamic_pointer_cast<VideoTrack>(track));
        }
            break;
        case TrackAudio:{
            metadata = std::make_shared<AudioMeta>(dynamic_pointer_cast<AudioTrack>(track));
        }
            break;
        default:
            return;;

    }
    //添加其metadata
    metadata->getMetadata().object_for_each([&](const std::string &key, const AMFValue &value){
        _metadata.set(key,value);
    });
    //设置Track的代理，这样输入frame至Track时，最终数据将输出到RtmpEncoder中
    track->addDelegate(encoder);
    //Rtmp编码器共用同一个环形缓存
    encoder->setRtmpRing(_rtmpRing);
}


const AMFValue &RtmpMuxer::getMetadata() const {
    if(!isAllTrackReady()){
        //尚未就绪
        static AMFValue s_amf;
        return s_amf;
    }
    return _metadata;
}

RtmpRingInterface::RingType::Ptr RtmpMuxer::getRtmpRing() const {
    return _rtmpRing;
}

}/* namespace mediakit */