/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FACTORY_H
#define ZLMEDIAKIT_FACTORY_H

#include <string>
#include "Rtmp/amf.h"
#include "Extension/Track.h"
#include "Extension/Frame.h"
#include "Rtsp/RtpCodec.h"
#include "Rtmp/RtmpCodec.h"
#include "Util/onceToken.h"

#define REGISTER_STATIC_VAR_INNER(var_name, line) var_name##_##line##__
#define REGISTER_STATIC_VAR(var_name, line) REGISTER_STATIC_VAR_INNER(var_name, line)

#define REGISTER_CODEC(plugin) \
static toolkit::onceToken REGISTER_STATIC_VAR(s_token, __LINE__) ([]() { \
    Factory::registerPlugin(plugin); \
});

namespace mediakit {

struct CodecPlugin {
    CodecId (*getCodec)();
    Track::Ptr (*getTrackByCodecId)(int sample_rate, int channels, int sample_bit);
    Track::Ptr (*getTrackBySdp)(const SdpTrack::Ptr &track);
    RtpCodec::Ptr (*getRtpEncoderByCodecId)(uint8_t pt);
    RtpCodec::Ptr (*getRtpDecoderByCodecId)();
    RtmpCodec::Ptr (*getRtmpEncoderByTrack)(const Track::Ptr &track);
    RtmpCodec::Ptr (*getRtmpDecoderByTrack)(const Track::Ptr &track);
    Frame::Ptr (*getFrameFromPtr)(const char *data, size_t bytes, uint64_t dts, uint64_t pts);
};

class Factory {
public:
    /**
     * 注册插件，非线程安全的
     */
    static void registerPlugin(const CodecPlugin &plugin);

    /**
     * 根据codec_id 获取track
     * @param codecId 编码id
     * @param sample_rate 采样率，视频固定为90000
     * @param channels 音频通道数
     * @param sample_bit 音频采样位数
     */
    static Track::Ptr getTrackByCodecId(CodecId codecId, int sample_rate = 0, int channels = 0, int sample_bit = 0);

    ////////////////////////////////rtsp相关//////////////////////////////////
    /**
     * 根据sdp生成Track对象
     */
    static Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track);

    /**
     * 根据c api 抽象的Track生成具体Track对象
     */
    static Track::Ptr getTrackByAbstractTrack(const Track::Ptr& track);

    /**
     * 根据codec id生成rtp编码器
     * @param codec_id 编码id
     * @param pt rtp payload type
     */
    static RtpCodec::Ptr getRtpEncoderByCodecId(CodecId codec_id, uint8_t pt);

    /**
     * 根据Track生成Rtp解包器
     */
    static RtpCodec::Ptr getRtpDecoderByCodecId(CodecId codec);


    ////////////////////////////////rtmp相关//////////////////////////////////

    /**
     * 根据amf对象获取视频相应的Track
     * @param amf rtmp metadata中的videocodecid的值
     */
    static Track::Ptr getVideoTrackByAmf(const AMFValue &amf);

    /**
     * 根据amf对象获取音频相应的Track
     * @param amf rtmp metadata中的audiocodecid的值
     */
    static Track::Ptr getAudioTrackByAmf(const AMFValue& amf, int sample_rate, int channels, int sample_bit);

    /**
     * 根据Track获取Rtmp的编码器
     * @param track 媒体描述对象
     */
    static RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track);

    /**
     * 根据Track获取Rtmp的解码器
     * @param track 媒体描述对象
     */
    static RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track);

    /**
     * 根据codecId获取rtmp的codec描述
     */
    static AMFValue getAmfByCodecId(CodecId codecId);

    static Frame::Ptr getFrameFromPtr(CodecId codec, const char *data, size_t size, uint64_t dts, uint64_t pts);
    static Frame::Ptr getFrameFromBuffer(CodecId codec, toolkit::Buffer::Ptr data, uint64_t dts, uint64_t pts);
};

}//namespace mediakit

#endif //ZLMEDIAKIT_FACTORY_H
