//
// Created by xzl on 2018/10/24.
//

#ifndef ZLMEDIAKIT_H264RTMPCODEC_H
#define ZLMEDIAKIT_H264RTMPCODEC_H

#include "RtmpCodec.h"
#include "Util/ResourcePool.h"

using namespace ZL::Rtmp;

/**
 * h264 Rtmp解码类
 */
class H264RtmpDecoder : public RtmpCodec ,public ResourcePoolHelper<H264Frame> {
public:
    typedef std::shared_ptr<H264RtmpDecoder> Ptr;

    H264RtmpDecoder();
    ~H264RtmpDecoder() {}

    /**
     * 输入264 Rtmp包
     * @param rtmp Rtmp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos = true) override;

    TrackType getTrackType() const override{
        return TrackVideo;
    }

    CodecId getCodecId() const override{
        return CodecH264;
    }
protected:
    bool decodeRtmp(const RtmpPacket::Ptr &Rtmp);
    void onGetH264_l(const char *pcData, int iLen, uint32_t ui32TimeStamp);
    void onGetH264(const char *pcData, int iLen, uint32_t ui32TimeStamp);
    H264Frame::Ptr obtainFrame();
protected:
    H264Frame::Ptr _h264frame;
    string _sps;
    string _pps;
};

/**
 * 264 Rtmp打包类
 */
class H264RtmpEncoder : public H264RtmpDecoder, public ResourcePoolHelper<RtmpPacket> {
public:
    typedef std::shared_ptr<H264RtmpEncoder> Ptr;

    H264RtmpEncoder();
    ~H264RtmpEncoder() {}

    /**
     * 输入264帧,需要指出的是，必须输入sps pps帧
     * @param frame 帧数据
     */
    void inputFrame(const Frame::Ptr &frame) override;
private:
    void makeVideoConfigPkt();
};

#endif //ZLMEDIAKIT_H264RTMPCODEC_H
