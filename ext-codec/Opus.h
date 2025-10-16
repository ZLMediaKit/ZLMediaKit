/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_OPUS_H
#define ZLMEDIAKIT_OPUS_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

/**
 * Opus帧音频通道
 * Opus frame audio channel
 */
class OpusTrack : public AudioTrackImp {
public:
    using Ptr = std::shared_ptr<OpusTrack>;
    OpusTrack() : AudioTrackImp(CodecOpus,48000,2,16){}

private:
    // Clone this Track
    Track::Ptr clone() const override {
        return std::make_shared<OpusTrack>(*this);
    }

    toolkit::Buffer::Ptr getExtraData() const override;
    void setExtraData(const uint8_t *data, size_t size) override;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_OPUS_H
