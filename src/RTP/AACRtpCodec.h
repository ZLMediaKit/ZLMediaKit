//
// Created by xzl on 2018/10/18.
//

#ifndef ZLMEDIAKIT_AACRTPCODEC_H
#define ZLMEDIAKIT_AACRTPCODEC_H

#include "RtpCodec.h"

/**
 * aac rtp转adts类
 */
class AACRtpDecoder : public RtpCodec {
public:
    /**
     * @param ui32SampleRate 采样率，用于时间戳转换用
     */
    AACRtpDecoder(uint32_t ui32SampleRate);
    ~AACRtpDecoder() {}

    /**
     * 输入rtp并解码
     * @param rtp rtp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    void inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false) override;
private:
    void onGetAdts(const AdtsFrame::Ptr &frame);
    AdtsFrame::Ptr obtainFrame();
private:
    AdtsFrame::Ptr m_adts;
    ResourcePool<AdtsFrame> m_framePool;
    uint32_t m_sampleRate;
};


/**
 * aac adts转rtp类
 */
class AACRtpEncoder : public RtpInfo, public RtpCodec {
public:
    /**
     * @param ui32Ssrc ssrc
     * @param ui32MtuSize mtu 大小
     * @param ui32SampleRate 采样率
     * @param ui8PlayloadType pt类型
     * @param ui8Interleaved rtsp interleaved 值
     */
    AACRtpEncoder(uint32_t ui32Ssrc,
                  uint32_t ui32MtuSize,
                  uint32_t ui32SampleRate,
                  uint8_t ui8PlayloadType = 97,
                  uint8_t ui8Interleaved = TrackAudio * 2);
    ~AACRtpEncoder() {}

    /**
     * 输入aac 数据，必须带dats头
     * @param frame 带dats头的aac数据
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    void inputFame(const Frame::Ptr &frame, bool key_pos = false) override;
private:
    void makeAACRtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp);
private:
    unsigned char m_aucSectionBuf[1600];
};


#endif //ZLMEDIAKIT_AACRTPCODEC_H
