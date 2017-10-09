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

#ifndef RTP_RTPMAKERAAC_H_
#define RTP_RTPMAKERAAC_H_

#include <memory>
#include "RtpMaker.h"
#include "Rtsp/RtspMediaSource.h"
#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/ResourcePool.h"

using namespace std;
using namespace ZL::Util;

namespace ZL {
namespace Rtsp {

class RtpMaker_AAC: public RtpMaker {
public:
	typedef std::shared_ptr<RtpMaker_AAC> Ptr;
	RtpMaker_AAC(const onGetRTP &cb,
			uint32_t ui32Ssrc, int iMtuSize , int iSampleRate, uint8_t ui8PlayloadType = 97,
			uint8_t ui8Interleaved = 2) :
			RtpMaker(cb, ui32Ssrc, iMtuSize,iSampleRate, ui8PlayloadType, ui8Interleaved) {
	}
	virtual ~RtpMaker_AAC() {
	}
	void makeRtp(const char *pcData, int iDataLen, uint32_t uiStamp) override;
private:
	inline void makeAACRtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp);
	unsigned char m_aucSectionBuf[1600];
};

} /* namespace RTP */
} /* namespace ZL */

#endif /* RTP_RTPMAKERAAC_H_ */
