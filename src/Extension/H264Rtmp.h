/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_H264RTMPCODEC_H
#define ZLMEDIAKIT_H264RTMPCODEC_H

#include "Rtmp/RtmpCodec.h"
#include "Extension/Track.h"
#include "Util/ResourcePool.h"
#include "Extension/H264.h"
using namespace toolkit;

namespace mediakit{
/**
 * h264 Rtmp解码类
 * 将 h264 over rtmp 解复用出 h264-Frame
 */
class H264RtmpDecoder : public RtmpCodec ,public ResourcePoolHelper<H264Frame> {
public:
    typedef std::shared_ptr<H264RtmpDecoder> Ptr;

    H264RtmpDecoder();
    ~H264RtmpDecoder() {}

    /**
     * 输入264 Rtmp包
     * @param rtmp Rtmp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos = true) override;

    CodecId getCodecId() const override{
        return CodecH264;
    }
protected:
    bool decodeRtmp(const RtmpPacket::Ptr &Rtmp);
    void onGetH264(const char *pcData, int iLen, uint32_t dts,uint32_t pts);
    H264Frame::Ptr obtainFrame();
protected:
    H264Frame::Ptr _h264frame;
    string _sps;
    string _pps;
};

/**
 * 264 Rtmp打包类
 */
class H264RtmpEncoder : public H264RtmpDecoder, public ResourcePoolHelper<RtmpPacket> {
public:
    typedef std::shared_ptr<H264RtmpEncoder> Ptr;

    /**
     * 构造函数，track可以为空，此时则在inputFrame时输入sps pps
     * 如果track不为空且包含sps pps信息，
     * 那么inputFrame时可以不输入sps pps
     * @param track
     */
    H264RtmpEncoder(const Track::Ptr &track);
    ~H264RtmpEncoder() {}

    /**
     * 输入264帧，可以不带sps pps
     * @param frame 帧数据
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 生成config包
     */
    void makeConfigPacket() override;
private:
    void makeVideoConfigPkt();
private:
    H264Track::Ptr _track;
    bool _gotSpsPps = false;
    RtmpPacket::Ptr _lastPacket;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_H264RTMPCODEC_H
