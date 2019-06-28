/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ZLMEDIAKIT_AACRTMPCODEC_H
#define ZLMEDIAKIT_AACRTMPCODEC_H

#include "Rtmp/RtmpCodec.h"
#include "Extension/Track.h"
#include "Extension/AAC.h"

namespace mediakit{
/**
 * aac Rtmp转adts类
 */
class AACRtmpDecoder : public RtmpCodec , public ResourcePoolHelper<AACFrame> {
public:
    typedef std::shared_ptr<AACRtmpDecoder> Ptr;

    AACRtmpDecoder();
    ~AACRtmpDecoder() {}

    /**
     * 输入Rtmp并解码
     * @param Rtmp Rtmp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    bool inputRtmp(const RtmpPacket::Ptr &Rtmp, bool key_pos = false) override;

    TrackType getTrackType() const override{
        return TrackAudio;
    }

    CodecId getCodecId() const override{
        return CodecAAC;
    }

protected:
    void onGetAAC(const char* pcData, int iLen, uint32_t ui32TimeStamp);
    AACFrame::Ptr obtainFrame();
protected:
    AACFrame::Ptr _adts;
    string _aac_cfg;
};


/**
 * aac adts转Rtmp类
 */
class AACRtmpEncoder : public AACRtmpDecoder ,  public ResourcePoolHelper<RtmpPacket> {
public:
    typedef std::shared_ptr<AACRtmpEncoder> Ptr;

    /**
     * 构造函数，track可以为空，此时则在inputFrame时输入adts头
     * 如果track不为空且包含adts头相关信息，
     * 那么inputFrame时可以不输入adts头
     * @param track
     */
    AACRtmpEncoder(const Track::Ptr &track);
    ~AACRtmpEncoder() {}

    /**
     * 输入aac 数据，可以不带adts头
     * @param frame aac数据
     */
    void inputFrame(const Frame::Ptr &frame) override;

private:
    void makeAudioConfigPkt();
private:
    uint8_t _ui8AudioFlags;
    AACTrack::Ptr _track;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_AACRTMPCODEC_H
