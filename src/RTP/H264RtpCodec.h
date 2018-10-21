//
// Created by xzl on 2018/10/18.
//

#ifndef ZLMEDIAKIT_H264RTPCODEC_H
#define ZLMEDIAKIT_H264RTPCODEC_H

#include "RtpCodec.h"
#include "Util/ResourcePool.h"

using namespace ZL::Util;

class H264RtpDecoder : public RtpCodec {
public:
    H264RtpDecoder() {
        m_framePool.setSize(32);
        m_h264frame = m_framePool.obtain();
    }

    ~H264RtpDecoder() {}

    void inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) override;

private:
    bool decodeRtp(const RtpPacket::Ptr &rtp, bool key_pos);

    void onGetH264(const H264Frame::Ptr &frame);

private:
    H264Frame::Ptr m_h264frame;
    ResourcePool<H264Frame> m_framePool;
};

class H264RtpEncoder : public RtpInfo, public RtpCodec {
public:
    H264RtpEncoder(uint32_t ui32Ssrc,
                   uint32_t ui32MtuSize = 1400,
                   uint32_t ui32SampleRate = 90000,
                   uint8_t ui8PlayloadType = 96,
                   uint8_t ui8Interleaved = TrackVideo * 2);

    ~H264RtpEncoder() {}

    void inputFame(const Frame::Ptr &frame, bool key_pos) override;

private:
    void makeH264Rtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp);

private:
    unsigned char m_aucSectionBuf[1600];
};


#endif //ZLMEDIAKIT_H264RTPCODEC_H
