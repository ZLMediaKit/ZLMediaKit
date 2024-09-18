/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
 * Generic rtp decoder class
 
 * [AUTO-TRANSLATED:41b57089]
 */
class CommonRtpDecoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr <CommonRtpDecoder>;

    /**
     * 构造函数
     * @param codec 编码id
     * @param max_frame_size 允许的最大帧大小
     * Constructor
     * @param codec codec id
     * @param max_frame_size maximum allowed frame size
     
     * [AUTO-TRANSLATED:c6b0414f]
     */
    CommonRtpDecoder(CodecId codec, size_t max_frame_size = 2 * 1024);

    /**
     * 输入rtp并解码
     * @param rtp rtp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     * Input rtp and decode
     * @param rtp rtp data packet
     * @param key_pos This parameter is internally forced to false, please ignore it
     
     * [AUTO-TRANSLATED:2993fcbe]
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false) override;

private:
    void obtainFrame();

private:
    bool _drop_flag = false;
    uint16_t _last_seq = 0;
    uint64_t _last_stamp = 0;
    size_t _max_frame_size;
    CodecId _codec;
    FrameImp::Ptr _frame;
};

/**
 * 通用 rtp编码类
 * Generic rtp encoder class
 
 * [AUTO-TRANSLATED:bb3991a5]
 */
class CommonRtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr <CommonRtpEncoder>;

    /**
     * 输入帧数据并编码成rtp
     * Input frame data and encode into rtp
     
     
     * [AUTO-TRANSLATED:02bc9009]
     */
    bool inputFrame(const Frame::Ptr &frame) override;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_COMMONRTP_H
