/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AACRTMPCODEC_H
#define ZLMEDIAKIT_AACRTMPCODEC_H

#include "Rtmp/RtmpCodec.h"
#include "Extension/Track.h"
#include "Extension/AAC.h"

namespace mediakit{
/**
 * aac Rtmp转adts类
 */
class AACRtmpDecoder : public RtmpCodec{
public:
    typedef std::shared_ptr<AACRtmpDecoder> Ptr;

    AACRtmpDecoder() {}
    ~AACRtmpDecoder() {}

    /**
     * 输入Rtmp并解码
     * @param rtmp Rtmp数据包
     */
    void inputRtmp(const RtmpPacket::Ptr &rtmp) override;

    CodecId getCodecId() const override{
        return CodecAAC;
    }

private:
    void onGetAAC(const char *data, size_t len, uint32_t stamp);

private:
    std::string _aac_cfg;
};


/**
 * aac adts转Rtmp类
 */
class AACRtmpEncoder : public AACRtmpDecoder{
public:
    typedef std::shared_ptr<AACRtmpEncoder> Ptr;

    /**
     * 构造函数，track可以为空，此时则在inputFrame时输入adts头
     * 如果track不为空且包含adts头相关信息，
     * 那么inputFrame时可以不输入adts头
     * @param track
     */
    AACRtmpEncoder(const Track::Ptr &track);
    ~AACRtmpEncoder() {}

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
    void makeAudioConfigPkt();

private:
    uint8_t _audio_flv_flags;
    AACTrack::Ptr _track;
    std::string _aac_cfg;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_AACRTMPCODEC_H
