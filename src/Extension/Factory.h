﻿/*
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

    /**
     * 根据CodecId获取Track，该Track的ready()状态一般都为false
     * @param codecId 编解码器id
     * @return
     */
    static Track::Ptr getTrackByCodecId(CodecId codecId);

    ////////////////////////////////rtsp相关//////////////////////////////////
    /**
     * 根据sdp生成Track对象
     */
    static Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track);

    /**
     * 根据sdp生成rtp编码器
     * @param sdp sdp对象
     * @return
     */
    static RtpCodec::Ptr getRtpEncoderBySdp(const Sdp::Ptr &sdp);

    /**
     * 根据Track生成Rtp解包器
     * @param track
     * @return
     */
    static RtpCodec::Ptr getRtpDecoderByTrack(const Track::Ptr &track);


    ////////////////////////////////rtmp相关//////////////////////////////////

    /**
     * 根据amf对象获取响应的Track
     * @param amf rtmp metadata中的videocodecid或audiocodecid的值
     * @return
     */
    static Track::Ptr getTrackByAmf(const AMFValue &amf);

    /**
     * 根据amf对象获取相应的CodecId
     * @param val rtmp metadata中的videocodecid或audiocodecid的值
     * @return
     */
    static CodecId getCodecIdByAmf(const AMFValue &val);

    /**
     * 根据Track获取Rtmp的编解码器
     * @param track 媒体描述对象
     * @return
     */
    static RtmpCodec::Ptr getRtmpCodecByTrack(const Track::Ptr &track);


    /**
     * 根据codecId获取rtmp的codec描述
     * @param codecId
     * @return
     */
    static AMFValue getAmfByCodecId(CodecId codecId);
};

}//namespace mediakit

#endif //ZLMEDIAKIT_FACTORY_H
