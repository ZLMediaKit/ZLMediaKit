/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_H265RTPCODEC_H
#define ZLMEDIAKIT_H265RTPCODEC_H

#include "H265.h"
#include "Rtsp/RtpCodec.h"
// for DtsGenerator
#include "Common/Stamp.h"

namespace mediakit {

/**
 * h265 rtp解码类
 * 将 h265 over rtsp-rtp 解复用出 h265-Frame
 * 《草案（H265-over-RTP）draft-ietf-payload-rtp-h265-07.pdf》
 */
class H265RtpDecoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<H265RtpDecoder>;

    H265RtpDecoder();

    /**
     * 输入265 rtp包
     * @param rtp rtp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true) override;

private:
    bool unpackAp(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp);
    bool mergeFu(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp, uint16_t seq);
    bool singleFrame(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp);

    bool decodeRtp(const RtpPacket::Ptr &rtp);
    H265Frame::Ptr obtainFrame();
    void outputFrame(const RtpPacket::Ptr &rtp, const H265Frame::Ptr &frame);

private:
    bool _is_gop = false;
    bool _using_donl_field = false;
    bool _gop_dropped = false;
    bool _fu_dropped = true;
    uint16_t _last_seq = 0;
    H265Frame::Ptr _frame;
    DtsGenerator _dts_generator;
};

/**
 * 265 rtp打包类
 */
class H265RtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<H265RtpEncoder>;

    /**
     * 输入265帧
     * @param frame 帧数据，必须
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 刷新输出所有frame缓存
     */
    void flush() override;

private:
    void packRtp(const char *ptr, size_t len, uint64_t pts, bool is_mark, bool gop_pos);
    void packRtpFu(const char *ptr, size_t len, uint64_t pts, bool is_mark, bool gop_pos);
    void insertConfigFrame(uint64_t pts);
    bool inputFrame_l(const Frame::Ptr &frame, bool is_mark);
private:
    Frame::Ptr _sps;
    Frame::Ptr _pps;
    Frame::Ptr _vps;
    Frame::Ptr _last_frame;
};

}//namespace mediakit{

#endif //ZLMEDIAKIT_H265RTPCODEC_H
