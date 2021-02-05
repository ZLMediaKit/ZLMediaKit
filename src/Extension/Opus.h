/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_OPUS_H
#define ZLMEDIAKIT_OPUS_H

#include "Frame.h"
#include "Track.h"

namespace mediakit{

/**
 * Opus帧音频通道
 */
class OpusTrack : public AudioTrackImp{
public:
    typedef std::shared_ptr<OpusTrack> Ptr;
    OpusTrack() : AudioTrackImp(CodecOpus,48000,2,16){}

private:
    //克隆该Track
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }
    //生成sdp
    Sdp::Ptr getSdp() override ;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_OPUS_H
