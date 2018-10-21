//
// Created by xzl on 2018/10/18.
//

#ifndef ZLMEDIAKIT_AACRTPCODEC_H
#define ZLMEDIAKIT_AACRTPCODEC_H


#include "RtpCodec.h"

class AACRtpCodec : public RtpEncoder {
public:
    AACRtpCodec(uint32_t ui32Ssrc,
                  uint32_t ui32MtuSize ,
                  uint32_t ui32SampleRate,
                  uint8_t ui8PlayloadType = 97,
                  uint8_t ui8Interleaved = TrackAudio * 2) :
            RtpEncoder(ui32Ssrc,ui32MtuSize,ui32SampleRate,ui8PlayloadType,ui8Interleaved) {
        m_framePool.setSize(32);
        m_adts = m_framePool.obtain();
    }
    ~AACRtpCodec(){}

    void inputFame(const Frame::Ptr &frame,bool key_pos) override;
    void inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) override ;
private:
    void makeAACRtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp);
    void onGetAdts(const AdtsFrame::Ptr &frame);
private:
    unsigned char m_aucSectionBuf[1600];
    AdtsFrame::Ptr m_adts;
    ResourcePool<AdtsFrame> m_framePool;
};



#endif //ZLMEDIAKIT_AACRTPCODEC_H
