/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_COMMONRTMP_H
#define ZLMEDIAKIT_COMMONRTMP_H

#include "Frame.h"
#include "Rtmp/RtmpCodec.h"

namespace mediakit{

/**
 * 通用 rtmp解码类
 * Generic rtmp decoder class
 
 * [AUTO-TRANSLATED:b04614f4]
 */
class CommonRtmpDecoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<CommonRtmpDecoder>;

    /**
     * 构造函数
     * Constructor
     
     * [AUTO-TRANSLATED:41469869]
     */
    CommonRtmpDecoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入Rtmp并解码
     * @param rtmp Rtmp数据包
     * Input Rtmp and decode
     * @param rtmp Rtmp data packet
     
     * [AUTO-TRANSLATED:43b1eae8]
     */
    void inputRtmp(const RtmpPacket::Ptr &rtmp) override;
};

/**
 * 通用 rtmp编码类
 * Generic rtmp encoder class
 
 * [AUTO-TRANSLATED:4616a2a8]
 */
class CommonRtmpEncoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<CommonRtmpEncoder>;

    CommonRtmpEncoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入帧数据
     * Input frame data
     
     
     * [AUTO-TRANSLATED:d13bc7f2]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    uint8_t _audio_flv_flags { 0 };
};

}//namespace mediakit
#endif //ZLMEDIAKIT_COMMONRTMP_H
