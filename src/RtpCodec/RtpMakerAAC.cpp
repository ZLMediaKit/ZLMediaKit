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
#include "RtpMakerAAC.h"
#include "Util/mini.h"
#include "Network/sockutil.h"

using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

void RtpMaker_AAC::makeRtp(const char *pcData, int iLen, uint32_t uiStamp) {
    GET_CONFIG_AND_REGISTER(uint32_t,cycleMS,Config::Rtp::kCycleMS);

    uiStamp %= cycleMS;
	char *ptr = (char *) pcData;
	int iSize = iLen;
	while (iSize > 0 ) {
		if (iSize <= _iMtuSize - 20) {
			_aucSectionBuf[0] = 0;
			_aucSectionBuf[1] = 16;
			_aucSectionBuf[2] = iLen >> 5;
			_aucSectionBuf[3] = (iLen & 0x1F) << 3;
			memcpy(_aucSectionBuf + 4, ptr, iSize);
			makeAACRtp(_aucSectionBuf, iSize + 4, true, uiStamp);
			break;
		}
		_aucSectionBuf[0] = 0;
		_aucSectionBuf[1] = 16;
		_aucSectionBuf[2] = (iLen) >> 5;
		_aucSectionBuf[3] = (iLen & 0x1F) << 3;
		memcpy(_aucSectionBuf + 4, ptr, _iMtuSize - 20);
		makeAACRtp(_aucSectionBuf, _iMtuSize - 16, false, uiStamp);
		ptr += (_iMtuSize - 20);
		iSize -= (_iMtuSize - 20);

	}
}

inline void RtpMaker_AAC::makeAACRtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp) {
	uint16_t u16RtpLen = uiLen + 12;
	_ui32TimeStamp = (_ui32SampleRate / 1000) * uiStamp;
	uint32_t ts = htonl(_ui32TimeStamp);
	uint16_t sq = htons(_ui16Sequence);
	uint32_t sc = htonl(_ui32Ssrc);
	auto pRtppkt = obtainPkt();
	auto &rtppkt = *(pRtppkt.get());
	unsigned char *pucRtp = rtppkt.payload;
	pucRtp[0] = '$';
	pucRtp[1] = _ui8Interleaved;
	pucRtp[2] = u16RtpLen >> 8;
	pucRtp[3] = u16RtpLen & 0x00FF;
	pucRtp[4] = 0x80;
	pucRtp[5] = (bMark << 7) | _ui8PlayloadType;
	memcpy(&pucRtp[6], &sq, 2);
	memcpy(&pucRtp[8], &ts, 4);
	//ssrc
	memcpy(&pucRtp[12], &sc, 4);
	//playload
	memcpy(&pucRtp[16], pData, uiLen);

	rtppkt.PT = _ui8PlayloadType;
	rtppkt.interleaved = _ui8Interleaved;
	rtppkt.mark = bMark;
	rtppkt.length = uiLen + 16;
	rtppkt.sequence = _ui16Sequence;
	rtppkt.timeStamp = _ui32TimeStamp;
	rtppkt.ssrc = _ui32Ssrc;
	rtppkt.type = TrackAudio;
	rtppkt.offset = 16;

	onMakeRtp(pRtppkt, false);
	_ui16Sequence++;
}

} /* namespace RTP */
} /* namespace ZL */
