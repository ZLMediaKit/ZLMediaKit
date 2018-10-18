//
// Created by xzl on 2018/10/18.
//

#include "H264RtpEncoder.h"

void H264RtpEncoder::inputFame(const Frame::Ptr &frame, bool key_pos) {
    RtpCodec::inputFame(frame, key_pos);

    GET_CONFIG_AND_REGISTER(uint32_t,cycleMS,Config::Rtp::kCycleMS);
    auto uiStamp = frame->stamp();
    auto pcData = frame->data();
    auto iLen = frame->size();

    uiStamp %= cycleMS;
    int iSize = m_ui32MtuSize - 2;
    if (iLen > iSize) { //超过MTU
        const unsigned char s_e_r_Start = 0x80;
        const unsigned char s_e_r_Mid = 0x00;
        const unsigned char s_e_r_End = 0x40;
        //获取帧头数据，1byte
        unsigned char naluType = *((unsigned char *) pcData) & 0x1f; //获取NALU的5bit 帧类型

        unsigned char nal_ref_idc = *((unsigned char *) pcData) & 0x60; //获取NALU的2bit 帧重要程度 00 可以丢 11不能丢
        //nal_ref_idc = 0x60;
        //组装FU-A帧头数据 2byte
        unsigned char f_nri_type = nal_ref_idc + 28;//F为0 1bit,nri上面获取到2bit,28为FU-A分片类型5bit
        unsigned char s_e_r_type = naluType;
        bool bFirst = true;
        bool mark = false;
        int nOffset = 1;
        while (!mark) {
            if (iLen < nOffset + iSize) {			//是否拆分结束
                iSize = iLen - nOffset;
                mark = true;
                s_e_r_type = s_e_r_End + naluType;
            } else {
                if (bFirst == true) {
                    s_e_r_type = s_e_r_Start + naluType;
                    bFirst = false;
                } else {
                    s_e_r_type = s_e_r_Mid + naluType;
                }
            }
            memcpy(m_aucSectionBuf, &f_nri_type, 1);
            memcpy(m_aucSectionBuf + 1, &s_e_r_type, 1);
            memcpy(m_aucSectionBuf + 2, (unsigned char *) pcData + nOffset, iSize);
            nOffset += iSize;
            makeH264Rtp(m_aucSectionBuf, iSize + 2, mark, uiStamp);
        }
    } else {
        makeH264Rtp(pcData, iLen, true, uiStamp);
    }
}

void H264RtpEncoder::makeH264Rtp(const void* data, unsigned int len, bool mark, uint32_t uiStamp) {
    uint16_t ui16RtpLen = len + 12;
    m_ui32TimeStamp = (m_ui32SampleRate / 1000) * uiStamp;
    uint32_t ts = htonl(m_ui32TimeStamp);
    uint16_t sq = htons(m_ui16Sequence);
    uint32_t sc = htonl(m_ui32Ssrc);

    auto pRtppkt = obtainRtp();
    auto &rtppkt = *(pRtppkt.get());
    unsigned char *pucRtp = rtppkt.payload;
    pucRtp[0] = '$';
    pucRtp[1] = m_ui8Interleaved;
    pucRtp[2] = ui16RtpLen >> 8;
    pucRtp[3] = ui16RtpLen & 0x00FF;
    pucRtp[4] = 0x80;
    pucRtp[5] = (mark << 7) | m_ui8PlayloadType;
    memcpy(&pucRtp[6], &sq, 2);
    memcpy(&pucRtp[8], &ts, 4);
    //ssrc
    memcpy(&pucRtp[12], &sc, 4);
    //playload
    memcpy(&pucRtp[16], data, len);

    rtppkt.PT = m_ui8PlayloadType;
    rtppkt.interleaved = m_ui8Interleaved;
    rtppkt.mark = mark;
    rtppkt.length = len + 16;
    rtppkt.sequence = m_ui16Sequence;
    rtppkt.timeStamp = m_ui32TimeStamp;
    rtppkt.ssrc = m_ui32Ssrc;
    rtppkt.type = TrackVideo;
    rtppkt.offset = 16;

    uint8_t type = ((uint8_t *) (data))[0] & 0x1F;
    RtpCodec::inputRtp(pRtppkt,type == 5);
    m_ui16Sequence++;
}
