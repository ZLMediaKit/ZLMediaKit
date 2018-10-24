//
// Created by xzl on 2018/10/24.
//

#ifndef ZLMEDIAKIT_AACRTMPCODEC_H
#define ZLMEDIAKIT_AACRTMPCODEC_H

#include "RtmpCodec.h"

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

    AACRtmpEncoder();
    ~AACRtmpEncoder() {}

    /**
     * 输入aac 数据，必须带dats头
     * @param frame 带dats头的aac数据
     */
    void inputFrame(const Frame::Ptr &frame) override;

private:
    void makeAudioConfigPkt();
    uint8_t _ui8AudioFlags;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_AACRTMPCODEC_H
