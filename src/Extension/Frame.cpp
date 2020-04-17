/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Frame.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

Frame::Ptr Frame::getCacheAbleFrame(const Frame::Ptr &frame){
    if(frame->cacheAble()){
        return frame;
    }
    return std::make_shared<FrameCacheAble>(frame);
}

#define SWITCH_CASE(codec_id) case codec_id : return #codec_id
const char *CodecInfo::getCodecName() {
    switch (getCodecId()) {
        SWITCH_CASE(CodecH264);
        SWITCH_CASE(CodecH265);
        SWITCH_CASE(CodecAAC);
        SWITCH_CASE(CodecG711A);
        SWITCH_CASE(CodecG711U);
        default:
            return "unknown codec";
    }
}

}//namespace mediakit

