/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Track.h"
#include "Util/util.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

Sdp::Ptr AudioTrackImp::getSdp(uint8_t payload_type) const {
    return std::make_shared<DefaultSdp>(payload_type, *this);
}
Sdp::Ptr VideoTrackImp::getSdp(uint8_t payload_type) const {
    return std::make_shared<DefaultSdp>(payload_type, *this);
}

} // namespace mediakit