/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_COMMONRTP_H
#define ZLMEDIAKIT_COMMONRTP_H

#include "Frame.h"
#include "Rtsp/RtpCodec.h"

namespace mediakit{

/**
 * 通用 rtp解码类
 */
class CommonRtpDecoder : public RtpCodec {
public:
    typedef std::shared_ptr <CommonRtpDecoder> Ptr;

    ~CommonRtpDecoder() override {}

    /**
     * 构造函数
     * @param codec 编码id
     * @param max_frame_size 允许的最大帧大小
     */
    CommonRtpDecoder(CodecId codec, size_t max_frame_size = 2 * 1024);

    /**
     * 返回编码类型ID
     */
    CodecId getCodecId() const override;

    /**
     * 输入rtp并解码
     * @param rtp rtp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false) override;

private:
    void obtainFrame();

private:
    bool _drop_flag = false;
    uint16_t _last_seq = 0;
    size_t _max_frame_size;
    CodecId _codec;
    FrameImp::Ptr _frame;
};

/**
 * 通用 rtp编码类
 */
class CommonRtpEncoder : public CommonRtpDecoder, public RtpInfo {
public:
    typedef std::shared_ptr <CommonRtpEncoder> Ptr;

    ~CommonRtpEncoder() override {}

    /**
     * 构造函数
     * @param codec 编码类型
     * @param ssrc ssrc
     * @param mtu_size mtu 大小
     * @param sample_rate 采样率
     * @param payload_type pt类型
     * @param interleaved rtsp interleaved 值
     */
    CommonRtpEncoder(CodecId codec, uint32_t ssrc, uint32_t mtu_size, uint32_t sample_rate, uint8_t payload_type, uint8_t interleaved);

    /**
     * 输入帧数据并编码成rtp
     */
    bool inputFrame(const Frame::Ptr &frame) override;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_COMMONRTP_H
