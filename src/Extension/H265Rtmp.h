/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_H265RTMPCODEC_H
#define ZLMEDIAKIT_H265RTMPCODEC_H

#include "Rtmp/RtmpCodec.h"
#include "Extension/Track.h"
#include "Util/ResourcePool.h"
#include "Extension/H265.h"
using namespace toolkit;

namespace mediakit{
/**
 * h265 Rtmp解码类
 * 将 h265 over rtmp 解复用出 h265-Frame
 */
class H265RtmpDecoder : public RtmpCodec ,public ResourcePoolHelper<H265Frame> {
public:
    typedef std::shared_ptr<H265RtmpDecoder> Ptr;

    H265RtmpDecoder();
    ~H265RtmpDecoder() {}

    /**
     * 输入265 Rtmp包
     * @param rtmp Rtmp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos = true) override;

    CodecId getCodecId() const override{
        return CodecH265;
    }
protected:
    bool decodeRtmp(const RtmpPacket::Ptr &Rtmp);
    void onGetH265(const char *pcData, int iLen, uint32_t dts,uint32_t pts);
    H265Frame::Ptr obtainFrame();
protected:
    H265Frame::Ptr _h265frame;
};

/**
 * 265 Rtmp打包类
 */
class H265RtmpEncoder : public H265RtmpDecoder, public ResourcePoolHelper<RtmpPacket> {
public:
    typedef std::shared_ptr<H265RtmpEncoder> Ptr;

    /**
     * 构造函数，track可以为空，此时则在inputFrame时输入sps pps
     * 如果track不为空且包含sps pps信息，
     * 那么inputFrame时可以不输入sps pps
     * @param track
     */
    H265RtmpEncoder(const Track::Ptr &track);
    ~H265RtmpEncoder() {}

    /**
     * 输入265帧，可以不带sps pps
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
    string _vps;
    string _sps;
    string _pps;
    H265Track::Ptr _track;
    bool _gotSpsPps = false;
    RtmpPacket::Ptr _lastPacket;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_H265RTMPCODEC_H
