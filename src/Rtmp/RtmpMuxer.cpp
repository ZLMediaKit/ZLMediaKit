/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
    _rtmp_ring = std::make_shared<RtmpRing::RingType>();
}

bool RtmpMuxer::addTrack(const Track::Ptr &track) {
    auto &encoder = _encoder[track->getTrackType()];
    //生成rtmp编码器,克隆该Track，防止循环引用
    encoder = Factory::getRtmpCodecByTrack(track->clone(), true);
    if (!encoder) {
        return false;
    }

    //设置rtmp输出环形缓存
    encoder->setRtmpRing(_rtmp_ring);

    //添加metadata
    Metadata::addTrack(_metadata, track);
    return true;
}

bool RtmpMuxer::inputFrame(const Frame::Ptr &frame) {
    auto &encoder = _encoder[frame->getTrackType()];
    return encoder ? encoder->inputFrame(frame) : false;
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
    return _rtmp_ring;
}

void RtmpMuxer::resetTracks() {
    _metadata.clear();
    for(auto &encoder : _encoder){
        encoder = nullptr;
    }
}


}/* namespace mediakit */