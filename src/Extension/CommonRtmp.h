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
 */
class CommonRtmpDecoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<CommonRtmpDecoder>;

    /**
     * 构造函数
     */
    CommonRtmpDecoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入Rtmp并解码
     * @param rtmp Rtmp数据包
     */
    void inputRtmp(const RtmpPacket::Ptr &rtmp) override;
};

/**
 * 通用 rtmp编码类
 */
class CommonRtmpEncoder : public RtmpCodec {
public:
    using Ptr = std::shared_ptr<CommonRtmpEncoder>;

    CommonRtmpEncoder(const Track::Ptr &track) : RtmpCodec(track) {}

    /**
     * 输入帧数据
     */
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    uint8_t _audio_flv_flags { 0 };
};

}//namespace mediakit
#endif //ZLMEDIAKIT_COMMONRTMP_H
