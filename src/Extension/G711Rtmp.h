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
 * G711 Rtmp转adts类
 */
class G711RtmpDecoder : public RtmpCodec , public ResourcePoolHelper<G711Frame> {
public:
    typedef std::shared_ptr<G711RtmpDecoder> Ptr;

    G711RtmpDecoder();
    ~G711RtmpDecoder() {}

    /**
     * 输入Rtmp并解码
     * @param Rtmp Rtmp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    bool inputRtmp(const RtmpPacket::Ptr &Rtmp, bool key_pos = false) override;

    TrackType getTrackType() const override{
        return TrackAudio;
    }

    void setCodecId(CodecId codecId)
    {
        _codecid = codecId;
    }

    CodecId getCodecId() const override{
        return _codecid;
    }

protected:
    void onGetG711(const char* pcData, int iLen, uint32_t ui32TimeStamp);
    G711Frame::Ptr obtainFrame();
protected:
    G711Frame::Ptr _adts;
    CodecId _codecid = CodecG711A;

};


/**
 * aac adts转Rtmp类
 */
class G711RtmpEncoder : public G711RtmpDecoder ,  public ResourcePoolHelper<RtmpPacket> {
public:
    typedef std::shared_ptr<G711RtmpEncoder> Ptr;

    /**
     * 构造函数，track可以为空，此时则在inputFrame时输入adts头
     * 如果track不为空且包含adts头相关信息，
     * 那么inputFrame时可以不输入adts头
     * @param track
     */
    G711RtmpEncoder(const Track::Ptr &track);
    ~G711RtmpEncoder() {}

    /**
     * 输入aac 数据，可以不带adts头
     * @param frame aac数据
     */
    void inputFrame(const Frame::Ptr &frame) override;
private:
    G711Track::Ptr _track;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_G711RTMPCODEC_H
