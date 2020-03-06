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
    _rtmpRing = std::make_shared<RtmpRing::RingType>();
}

void RtmpMuxer::addTrack(const Track::Ptr &track) {
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
            return;

    }

    auto &encoder = _encoder[track->getTrackType()];
    //生成rtmp编码器,克隆该Track，防止循环引用
    encoder = Factory::getRtmpCodecByTrack(track->clone());
    if (!encoder) {
        return;
    }

    //设置rtmp输出环形缓存
    encoder->setRtmpRing(_rtmpRing);

    //添加其metadata
    metadata->getMetadata().object_for_each([&](const std::string &key, const AMFValue &value){
        _metadata.set(key,value);
    });
}

void RtmpMuxer::inputFrame(const Frame::Ptr &frame) {
    auto &encoder = _encoder[frame->getTrackType()];
    if(encoder){
        encoder->inputFrame(frame);
    }
}

void RtmpMuxer::makeConfigPacket(){
    for(auto &encoder : _encoder){
        if(encoder){
            encoder->makeConfigPacket();
        }
    }
}

const AMFValue &RtmpMuxer::getMetadata() const {
    return _metadata;
}

RtmpRing::RingType::Ptr RtmpMuxer::getRtmpRing() const {
    return _rtmpRing;
}

void RtmpMuxer::resetTracks() {
    _metadata.clear();
    for(auto &encoder : _encoder){
        encoder = nullptr;
    }
}


}/* namespace mediakit */