/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_G722_H
#define ZLMEDIAKIT_G722_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

/**
 * G722编解码器
 * G.722 是一种宽带音频编解码器，采样率16kHz，带宽50-7000Hz
 * 注意: CodecG722 已在 Frame.h 中定义（值为18）
 */

/**
 * G722 Frame
 */
class G722Frame : public FrameImp {
public:
    using Ptr = std::shared_ptr<G722Frame>;
    
    G722Frame() {
        _codec_id = CodecG722;
    }
    
    bool keyFrame() const override { return false; }
    bool configFrame() const override { return false; }
};

/**
 * G722 Track
 */
class G722Track : public AudioTrackImp {
public:
    using Ptr = std::shared_ptr<G722Track>;
    
    G722Track(int sample_rate = 16000, int channels = 1) 
        : AudioTrackImp(CodecG722, sample_rate, channels, 16) {}
    
    bool ready() const override { return true; }
    
    Track::Ptr clone() const override {
        return std::make_shared<G722Track>(*this);
    }
    
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_G722_H
