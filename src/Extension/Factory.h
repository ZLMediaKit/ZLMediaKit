/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FACTORY_H
#define ZLMEDIAKIT_FACTORY_H

#include <string>
#include "Rtmp/amf.h"
#include "Extension/Track.h"
#include "Rtsp/RtpCodec.h"
#include "Rtmp/RtmpCodec.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

class Factory {
public:
    ////////////////////////////////rtsp相关//////////////////////////////////
    /**
     * 根据sdp生成Track对象
     */
    static Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track);

    /**
     * 根据sdp生成rtp编码器
     * @param sdp sdp对象
     */
    static RtpCodec::Ptr getRtpEncoderBySdp(const Sdp::Ptr &sdp);

    /**
     * 根据Track生成Rtp解包器
     */
    static RtpCodec::Ptr getRtpDecoderByTrack(const Track::Ptr &track);


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
     * 根据Track获取Rtmp的编解码器
     * @param track 媒体描述对象
     * @param is_encode 是否为编码器还是解码器
     */
    static RtmpCodec::Ptr getRtmpCodecByTrack(const Track::Ptr &track, bool is_encode);

    /**
     * 根据codecId获取rtmp的codec描述
     */
    static AMFValue getAmfByCodecId(CodecId codecId);
};

}//namespace mediakit

#endif //ZLMEDIAKIT_FACTORY_H
