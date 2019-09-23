/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
