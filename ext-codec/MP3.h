/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP3_H
#define ZLMEDIAKIT_MP3_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

/**
 * MP3音频通道
 */
class MP3Track : public AudioTrackImp{
public:
    using Ptr = std::shared_ptr<MP3Track>;
    MP3Track(int sample_rate, int channels) : AudioTrackImp(CodecMP3,sample_rate,channels,16){}

private:
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
    Track::Ptr clone() const override;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_MP3_H