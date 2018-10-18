//
// Created by xzl on 2018/10/18.
//

#ifndef ZLMEDIAKIT_H264RTPCODEC_H
#define ZLMEDIAKIT_H264RTPCODEC_H

#include "RtpCodec.h"

class H264RtpEncoder : public RtpEncoder {
public:
    H264RtpEncoder(uint32_t ui32Ssrc,
                 uint32_t ui32MtuSize = 1400,
                 uint32_t ui32SampleRate = 90000,
                 uint8_t ui8PlayloadType = 96,
                 uint8_t ui8Interleaved = TrackVideo * 2) :
            RtpEncoder(ui32Ssrc,ui32MtuSize,ui32SampleRate,ui8PlayloadType,ui8Interleaved) {
    }
    ~H264RtpEncoder(){}

    void inputFame(const Frame::Ptr &frame,bool key_pos) override;
private:
    void makeH264Rtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp);
private:
    unsigned char m_aucSectionBuf[1600];
};


#endif //ZLMEDIAKIT_H264RTPCODEC_H
