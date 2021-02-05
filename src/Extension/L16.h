/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_L16_H
#define ZLMEDIAKIT_L16_H

#include "Frame.h"
#include "Track.h"

namespace mediakit{

/**
 * L16音频通道
 */
class L16Track : public AudioTrackImp{
public:
    using Ptr = std::shared_ptr<L16Track>;
    L16Track(int sample_rate, int channels) : AudioTrackImp(CodecL16,sample_rate,channels,16){}

private:
    Sdp::Ptr getSdp() override;
    Track::Ptr clone() override;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_L16_H