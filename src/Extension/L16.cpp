/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "L16.h"

using namespace toolkit;

namespace mediakit{

Sdp::Ptr L16Track::getSdp() {
    WarnL << "Enter  L16Track::getSdp function";
    if(!ready()){
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<AudioSdp>(this);
}

Track::Ptr L16Track::clone() {
    return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
}

}//namespace mediakit


