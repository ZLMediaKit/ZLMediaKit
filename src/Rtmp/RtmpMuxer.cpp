/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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

    switch (track->getCodecId()){
        case CodecG711A:
        case CodecG711U:{
            auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
            if(!audio_track){
                return;
            }
            if (audio_track->getAudioSampleRate() != 8000 ||
                audio_track->getAudioChannel() != 1 ||
                audio_track->getAudioSampleBit() != 16) {
                WarnL << "RTMP只支持8000/1/16规格的G711,目前规格是:"
                      << audio_track->getAudioSampleRate() << "/"
                      << audio_track->getAudioChannel() << "/"
                      << audio_track->getAudioSampleBit()
                      << ",该音频已被忽略";
                return;
            }
            break;
        }
        default : break;
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