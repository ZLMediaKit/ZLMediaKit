/*
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
 * aac Rtmp to adts class
 
 * [AUTO-TRANSLATED:8b262ddb]
 */
class AACRtmpDecoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<AACRtmpDecoder>;

    AACRtmpDecoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入Rtmp并解码
     * @param rtmp Rtmp数据包
     * Input Rtmp and decode
     * @param rtmp Rtmp data packet
     
     * [AUTO-TRANSLATED:43b1eae8]
     */
    void inputRtmp(const RtmpPacket::Ptr &rtmp) override;
};

/**
 * aac adts转Rtmp类
 * aac adts to Rtmp class
 
 * [AUTO-TRANSLATED:2d9c53dd]
 */
class AACRtmpEncoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<AACRtmpEncoder>;

    /**
     * 构造函数，track可以为空，此时则在inputFrame时输入adts头
     * 如果track不为空且包含adts头相关信息，
     * 那么inputFrame时可以不输入adts头
     * @param track
     * Constructor, track can be empty, in which case the adts header is input when inputFrame is called
     * If track is not empty and contains adts header related information,
     * then the adts header can be omitted when inputFrame is called
     * @param track
     
     * [AUTO-TRANSLATED:fcf8f765]
     */
    AACRtmpEncoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入aac 数据，可以不带adts头
     * @param frame aac数据
     * Input aac data, can be without adts header
     * @param frame aac data
     
     * [AUTO-TRANSLATED:d9f4131a]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 生成config包
     * Generate config package
     
     
     * [AUTO-TRANSLATED:8f851364]
     */
    void makeConfigPacket() override;

private:
    uint8_t _audio_flv_flags {0};
};

}//namespace mediakit

#endif //ZLMEDIAKIT_AACRTMPCODEC_H
