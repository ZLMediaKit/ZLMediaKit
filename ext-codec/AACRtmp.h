﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AACRTMPCODEC_H
#define ZLMEDIAKIT_AACRTMPCODEC_H

#include "AAC.h"
#include "Rtmp/RtmpCodec.h"
#include "Extension/Track.h"

namespace mediakit {
/**
 * aac Rtmp转adts类
 */
class AACRtmpDecoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<AACRtmpDecoder>;

    AACRtmpDecoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入Rtmp并解码
     * @param rtmp Rtmp数据包
     */
    void inputRtmp(const RtmpPacket::Ptr &rtmp) override;
};

/**
 * aac adts转Rtmp类
 */
class AACRtmpEncoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<AACRtmpEncoder>;

    /**
     * 构造函数，track可以为空，此时则在inputFrame时输入adts头
     * 如果track不为空且包含adts头相关信息，
     * 那么inputFrame时可以不输入adts头
     * @param track
     */
    AACRtmpEncoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入aac 数据，可以不带adts头
     * @param frame aac数据
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 生成config包
     */
    void makeConfigPacket() override;

private:
    uint8_t _audio_flv_flags {0};
};

}//namespace mediakit

#endif //ZLMEDIAKIT_AACRTMPCODEC_H
