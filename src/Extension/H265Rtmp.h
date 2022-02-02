/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

namespace mediakit{
/**
 * h265 Rtmp解码类
 * 将 h265 over rtmp 解复用出 h265-Frame
 */
class H265RtmpDecoder : public RtmpCodec {
public:
    typedef std::shared_ptr<H265RtmpDecoder> Ptr;

    H265RtmpDecoder();
    ~H265RtmpDecoder() {}

    /**
     * 输入265 Rtmp包
     * @param rtmp Rtmp包
     */
    void inputRtmp(const RtmpPacket::Ptr &rtmp) override;

    CodecId getCodecId() const override{
        return CodecH265;
    }

protected:
    void onGetH265(const char *pcData, size_t iLen, uint32_t dts,uint32_t pts);
    H265Frame::Ptr obtainFrame();

protected:
    H265Frame::Ptr _h265frame;
};

/**
 * 265 Rtmp打包类
 */
class H265RtmpEncoder : public H265RtmpDecoder{
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
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 生成config包
     */
    void makeConfigPacket() override;

private:
    void makeVideoConfigPkt();

private:
    bool _got_config_frame = false;
    std::string _vps;
    std::string _sps;
    std::string _pps;
    H265Track::Ptr _track;
    RtmpPacket::Ptr _rtmp_packet;
    FrameMerger _merger{FrameMerger::mp4_nal_size};
};

}//namespace mediakit

#endif //ZLMEDIAKIT_H265RTMPCODEC_H
