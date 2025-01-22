/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_H265RTMPCODEC_H
#define ZLMEDIAKIT_H265RTMPCODEC_H

#include "H265.h"
#include "Rtmp/RtmpCodec.h"
#include "Extension/Track.h"

namespace mediakit {
/**
 * h265 Rtmp解码类
 * 将 h265 over rtmp 解复用出 h265-Frame
 * h265 Rtmp decoder class
 * Demultiplex h265-Frame from h265 over rtmp
 
 * [AUTO-TRANSLATED:2768e4bd]
 */
class H265RtmpDecoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<H265RtmpDecoder>;

    H265RtmpDecoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入265 Rtmp包
     * @param rtmp Rtmp包
     * Input 265 Rtmp packet
     * @param rtmp Rtmp packet
     
     * [AUTO-TRANSLATED:63dbe33f]
     */
    void inputRtmp(const RtmpPacket::Ptr &rtmp) override;

protected:
    void outputFrame(const char *data, size_t size, uint32_t dts, uint32_t pts);
    void splitFrame(const uint8_t *data, size_t size, uint32_t dts, uint32_t pts);

protected:
    RtmpPacketInfo _info;
};

/**
 * 265 Rtmp打包类
 * 265 Rtmp packaging class
 
 * [AUTO-TRANSLATED:5891c800]
 */
class H265RtmpEncoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<H265RtmpEncoder>;

    /**
     * 构造函数，track可以为空，此时则在inputFrame时输入sps pps
     * 如果track不为空且包含sps pps信息，
     * 那么inputFrame时可以不输入sps pps
     * @param track
     * Constructor, track can be empty, in which case sps pps is input when inputFrame
     * If track is not empty and contains sps pps information,
     * Then sps pps can be omitted when inputFrame
     * @param track
     
     * [AUTO-TRANSLATED:e61fdfed]
     */
    H265RtmpEncoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入265帧，可以不带sps pps
     * @param frame 帧数据
     * Input 265 frame, sps pps can be omitted
     * @param frame Frame data
     
     * [AUTO-TRANSLATED:7d1be1e7]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 刷新输出所有frame缓存
     * Flush all frame cache output
     
     * [AUTO-TRANSLATED:adaea568]
     */
    void flush() override;

    /**
     * 生成config包
     * Generate config packet
     
     
     * [AUTO-TRANSLATED:8f851364]
     */
    void makeConfigPacket() override;

private:
    RtmpPacket::Ptr _rtmp_packet;
    FrameMerger _merger { FrameMerger::mp4_nal_size };
};

} // namespace mediakit

#endif // ZLMEDIAKIT_H265RTMPCODEC_H
