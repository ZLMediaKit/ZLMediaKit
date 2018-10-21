//
// Created by xzl on 2018/10/18.
//

#include "AACRtpCodec.h"

void AACRtpCodec::inputFame(const Frame::Ptr &frame, bool key_pos) {
    RtpCodec::inputFame(frame, key_pos);

    GET_CONFIG_AND_REGISTER(uint32_t,cycleMS,Config::Rtp::kCycleMS);
    auto uiStamp = frame->stamp();
    auto pcData = frame->data();
    auto iLen = frame->size();

    uiStamp %= cycleMS;
    char *ptr = (char *) pcData;
    int iSize = iLen;
    while (iSize > 0 ) {
        if (iSize <= m_ui32MtuSize - 20) {
            m_aucSectionBuf[0] = 0;
            m_aucSectionBuf[1] = 16;
            m_aucSectionBuf[2] = iLen >> 5;
            m_aucSectionBuf[3] = (iLen & 0x1F) << 3;
            memcpy(m_aucSectionBuf + 4, ptr, iSize);
            makeAACRtp(m_aucSectionBuf, iSize + 4, true, uiStamp);
            break;
        }
        m_aucSectionBuf[0] = 0;
        m_aucSectionBuf[1] = 16;
        m_aucSectionBuf[2] = (iLen) >> 5;
        m_aucSectionBuf[3] = (iLen & 0x1F) << 3;
        memcpy(m_aucSectionBuf + 4, ptr, m_ui32MtuSize - 20);
        makeAACRtp(m_aucSectionBuf, m_ui32MtuSize - 16, false, uiStamp);
        ptr += (m_ui32MtuSize - 20);
        iSize -= (m_ui32MtuSize - 20);

    }
}

void AACRtpCodec::makeAACRtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp) {
    uint16_t u16RtpLen = uiLen + 12;
    m_ui32TimeStamp = (m_ui32SampleRate / 1000) * uiStamp;
    uint32_t ts = htonl(m_ui32TimeStamp);
    uint16_t sq = htons(m_ui16Sequence);
    uint32_t sc = htonl(m_ui32Ssrc);
    auto pRtppkt = obtainRtp();
    auto &rtppkt = *(pRtppkt.get());
    unsigned char *pucRtp = rtppkt.payload;
    pucRtp[0] = '$';
    pucRtp[1] = m_ui8Interleaved;
    pucRtp[2] = u16RtpLen >> 8;
    pucRtp[3] = u16RtpLen & 0x00FF;
    pucRtp[4] = 0x80;
    pucRtp[5] = (bMark << 7) | m_ui8PlayloadType;
    memcpy(&pucRtp[6], &sq, 2);
    memcpy(&pucRtp[8], &ts, 4);
    //ssrc
    memcpy(&pucRtp[12], &sc, 4);
    //playload
    memcpy(&pucRtp[16], pData, uiLen);

    rtppkt.PT = m_ui8PlayloadType;
    rtppkt.interleaved = m_ui8Interleaved;
    rtppkt.mark = bMark;
    rtppkt.length = uiLen + 16;
    rtppkt.sequence = m_ui16Sequence;
    rtppkt.timeStamp = m_ui32TimeStamp;
    rtppkt.ssrc = m_ui32Ssrc;
    rtppkt.type = TrackAudio;
    rtppkt.offset = 16;

    RtpCodec::inputRtp(pRtppkt, false);
    m_ui16Sequence++;
}

/////////////////////////////////////////////////////////////////////////////////////

void AACRtpCodec::inputRtp(const RtpPacket::Ptr &rtppack, bool key_pos) {
    RtpCodec::inputRtp(rtppack, key_pos);

    char *frame = (char *) rtppack->payload + rtppack->offset;
    int length = rtppack->length - rtppack->offset;

    if (m_adts->aac_frame_length + length - 4 > sizeof(AdtsFrame::buffer)) {
        m_adts->aac_frame_length = 7;
        WarnL << "aac负载数据太长";
        return ;
    }
    memcpy(m_adts->buffer + m_adts->aac_frame_length, frame + 4, length - 4);
    m_adts->aac_frame_length += (length - 4);
    if (rtppack->mark == true) {
        m_adts->sequence = rtppack->sequence;
        //todo(xzl) 此处完成时间戳转换
//        m_adts->timeStamp = rtppack->timeStamp * (1000.0 / m_iSampleRate);
        writeAdtsHeader(*m_adts, m_adts->buffer);
        onGetAdts(m_adts);
    }
    return ;
}

void AACRtpCodec::onGetAdts(const AdtsFrame::Ptr &frame) {
    RtpCodec::inputFame(frame, false);
    m_adts = m_framePool.obtain();
    m_adts->aac_frame_length = 7;
}

