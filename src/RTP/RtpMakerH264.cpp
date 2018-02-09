/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#include "Common/config.h"
#include "RtpMakerH264.h"
#include "Util/mini.h"
#include "Network/sockutil.h"

using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

void RtpMaker_H264::makeRtp(const char* pcData, int iLen, uint32_t uiStamp) {
    GET_CONFIG_AND_REGISTER(uint32_t,cycleMS,Config::Rtp::kCycleMS);

    uiStamp %= cycleMS;
	int iSize = m_iMtuSize - 2;
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
			memcpy(aucSectionBuf, &f_nri_type, 1);
			memcpy(aucSectionBuf + 1, &s_e_r_type, 1);
			memcpy(aucSectionBuf + 2, (unsigned char *) pcData + nOffset, iSize);
			nOffset += iSize;
			makeH264Rtp(aucSectionBuf, iSize + 2, mark, uiStamp);
		}
	} else {
		makeH264Rtp(pcData, iLen, true, uiStamp);
	}
}

inline void RtpMaker_H264::makeH264Rtp(const void* data, unsigned int len, bool mark, uint32_t uiStamp) {
	uint16_t ui16RtpLen = len + 12;
	m_ui32TimeStamp = (m_ui32SampleRate / 1000) * uiStamp;
	uint32_t ts = htonl(m_ui32TimeStamp);
	uint16_t sq = htons(m_ui16Sequence);
	uint32_t sc = htonl(m_ui32Ssrc);

	auto pRtppkt = obtainPkt();
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
	memcpy(&pucRtp[16], data, len);
	rtppkt.PT = m_ui8PlayloadType;
	rtppkt.interleaved = m_ui8Interleaved;
	rtppkt.mark = mark;
	rtppkt.length = len + 16;
	rtppkt.sequence = m_ui16Sequence;
	rtppkt.timeStamp = m_ui32TimeStamp;
	rtppkt.ssrc = m_ui32Ssrc;
	rtppkt.type = TrackVideo;
	uint8_t type = ((uint8_t *) (data))[0] & 0x1F;
	memcpy(rtppkt.payload + 16, data, len);
	onMakeRtp(pRtppkt, type == 5);
	m_ui16Sequence++;
	//InfoL<<timeStamp<<" "<<time<<" "<<sampleRate;
}

} /* namespace RTP */
} /* namespace ZL */
