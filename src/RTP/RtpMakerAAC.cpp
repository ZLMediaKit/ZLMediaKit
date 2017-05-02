/*
 * RtpMakerAAC.cpp
 *
 *  Created on: 2016年8月12日
 *      Author: xzl
 */

#include <netinet/in.h>
#include "Common/config.h"
#include "RtpMakerAAC.h"
#include "Util/mini.h"

using namespace ZL::Util;


namespace ZL {
namespace Rtsp {

void RtpMaker_AAC::makeRtp(const char *pcData, int iLen, uint32_t uiStamp) {
	static uint32_t cycleMS = mINI::Instance()[Config::Rtp::kCycleMS].as<uint32_t>();
	uiStamp %= cycleMS;
	char *ptr = (char *) pcData;
	int iSize = iLen;
	while (iSize > 0 ) {
		if (iSize <= m_iMtuSize - 20) {
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
		memcpy(m_aucSectionBuf + 4, ptr, m_iMtuSize - 20);
		makeAACRtp(m_aucSectionBuf, m_iMtuSize - 16, false, uiStamp);
		ptr += (m_iMtuSize - 20);
		iSize -= (m_iMtuSize - 20);

	}
}

inline void RtpMaker_AAC::makeAACRtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp) {
	uint16_t u16RtpLen = uiLen + 12;
	m_ui32TimeStamp = (m_ui32SampleRate / 1000) * uiStamp;
	uint32_t ts = htonl(m_ui32TimeStamp);
	uint16_t sq = htons(m_ui16Sequence);
	uint32_t sc = htonl(m_ui32Ssrc);
	auto pRtppkt = obtainPkt();
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
	memcpy(&pucRtp[16], pData, uiLen);
	rtppkt.PT = m_ui8PlayloadType;
	rtppkt.interleaved = m_ui8Interleaved;
	rtppkt.mark = bMark;
	rtppkt.length = uiLen + 16;
	rtppkt.sequence = m_ui16Sequence;
	rtppkt.timeStamp = m_ui32TimeStamp;
	rtppkt.ssrc = m_ui32Ssrc;
	rtppkt.type = TrackAudio;

	memcpy(rtppkt.payload + 16, pData, uiLen);
	onMakeRtp(pRtppkt, false);
	m_ui16Sequence++;
}

} /* namespace RTP */
} /* namespace ZL */
