/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_G711RTMPCODEC_H
#define ZLMEDIAKIT_G711RTMPCODEC_H

#include "Rtmp/RtmpCodec.h"
#include "Extension/Track.h"
#include "Extension/G711.h"

namespace mediakit{
/**
 * G711 Rtmp转G711 Frame类
 */
class G711RtmpDecoder : public RtmpCodec , public ResourcePoolHelper<G711Frame> {
public:
    typedef std::shared_ptr<G711RtmpDecoder> Ptr;

    G711RtmpDecoder(CodecId codecId);
    ~G711RtmpDecoder() {}

    /**
     * 输入Rtmp并解码
     * @param Rtmp Rtmp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    bool inputRtmp(const RtmpPacket::Ptr &Rtmp, bool key_pos = false) override;

    CodecId getCodecId() const override{
        return _codecId;
    }
private:
    G711Frame::Ptr obtainFrame();
private:
    G711Frame::Ptr _frame;
    CodecId _codecId;
};

/**
 * G711 RTMP打包类
 */
class G711RtmpEncoder : public G711RtmpDecoder ,  public ResourcePoolHelper<RtmpPacket> {
public:
    typedef std::shared_ptr<G711RtmpEncoder> Ptr;

    G711RtmpEncoder(const Track::Ptr &track);
    ~G711RtmpEncoder() {}

    /**
     * 输入G711 数据
     */
    void inputFrame(const Frame::Ptr &frame) override;
private:
    uint8_t _audio_flv_flags = 0;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_G711RTMPCODEC_H
