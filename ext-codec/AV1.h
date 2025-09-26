/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AV1_H
#define ZLMEDIAKIT_AV1_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

/**
 * AV1视频通道
 */
class AV1Track : public VideoTrackImp {
public:
    using Ptr = std::shared_ptr<AV1Track>;

    AV1Track(int width = 0, int height = 0, int fps = 0) : VideoTrackImp(CodecAV1, width, height, fps) {}

private:
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
    Track::Ptr clone() const override;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_AV1_H