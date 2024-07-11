/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_G711_H
#define ZLMEDIAKIT_G711_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit{

/**
 * G711音频通道
 */
class G711Track : public AudioTrackImp{
public:
    using Ptr = std::shared_ptr<G711Track>;
    G711Track(CodecId codecId, int sample_rate, int channels, int sample_bit) : AudioTrackImp(codecId, 8000, 1, 16) {}

private:
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
    Track::Ptr clone() const override;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_G711_H