/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "G711.h"

using namespace toolkit;

namespace mediakit{

Track::Ptr G711Track::clone() {
    return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this);
}

Sdp::Ptr G711Track::getSdp() {
    if(!ready()){
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }

    const auto codec = getCodecId();
    const auto sample_rate = getAudioSampleRate();
    const auto audio_channel = getAudioChannel();
    auto payload_type = 98;
    if (sample_rate == 8000 && audio_channel == 1) {
        // https://datatracker.ietf.org/doc/html/rfc3551#section-6
        payload_type = (codec == CodecG711U) ? Rtsp::PT_PCMU : Rtsp::PT_PCMA;
    }

    return std::make_shared<AudioSdp>(this, payload_type);
}

}//namespace mediakit


