/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_H264RTPCODEC_H
#define ZLMEDIAKIT_H264RTPCODEC_H

#include "H264.h"
// for DtsGenerator
#include "Common/Stamp.h"
#include "Rtsp/RtpCodec.h"

namespace mediakit {

/**
 * h264 rtp解码类
 * 将 h264 over rtsp-rtp 解复用出 h264-Frame
 * rfc3984
 */
class H264RtpDecoder : public RtpCodec{
public:
    using Ptr = std::shared_ptr<H264RtpDecoder>;

    H264RtpDecoder();

    /**
     * 输入264 rtp包
     * @param rtp rtp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true) override;

private:
    bool singleFrame(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp);
    bool unpackStapA(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp);
    bool mergeFu(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp, uint16_t seq);

    bool decodeRtp(const RtpPacket::Ptr &rtp);
    H264Frame::Ptr obtainFrame();
    void outputFrame(const RtpPacket::Ptr &rtp, const H264Frame::Ptr &frame);

private:
    bool _is_gop = false;
    bool _gop_dropped = false;
    bool _fu_dropped = true;
    uint16_t _last_seq = 0;
    H264Frame::Ptr _frame;
    DtsGenerator _dts_generator;
};

/**
 * 264 rtp打包类
 */
class H264RtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<H264RtpEncoder>;

    /**
     * 输入264帧
     * @param frame 帧数据，必须
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 刷新输出所有frame缓存
     */
    void flush() override;

private:
    void insertConfigFrame(uint64_t pts);
    bool inputFrame_l(const Frame::Ptr &frame, bool is_mark);
    void packRtp(const char *data, size_t len, uint64_t pts, bool is_mark, bool gop_pos);
    void packRtpFu(const char *data, size_t len, uint64_t pts, bool is_mark, bool gop_pos);
    void packRtpStapA(const char *data, size_t len, uint64_t pts, bool is_mark, bool gop_pos);
    void packRtpSingleNalu(const char *data, size_t len, uint64_t pts, bool is_mark, bool gop_pos);
    void packRtpSmallFrame(const char *data, size_t len, uint64_t pts, bool is_mark, bool gop_pos);

private:
    Frame::Ptr _sps;
    Frame::Ptr _pps;
    Frame::Ptr _last_frame;
};

}//namespace mediakit{

#endif //ZLMEDIAKIT_H264RTPCODEC_H
