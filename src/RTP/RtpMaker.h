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

#ifndef RTP_RTPMAKER_H_
#define RTP_RTPMAKER_H_

#include "Rtsp/RtspMediaSource.h"
#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Thread/ThreadPool.h"

using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace Rtsp {

class RtpMaker {
public:
	typedef function<void(const RtpPacket::Ptr &pPkt, bool bKeyPos)> onGetRTP;
	RtpMaker(const onGetRTP &cb, uint32_t ui32Ssrc, int iMtuSize,int iSampleRate,
			uint8_t ui8PlayloadType, uint8_t ui8Interleaved) {
		callBack = cb;
		m_ui32Ssrc = ui32Ssrc;
		m_ui32SampleRate = iSampleRate;
		m_iMtuSize = iMtuSize;
		m_ui8PlayloadType = ui8PlayloadType;
		m_ui8Interleaved = ui8Interleaved;
	}
	virtual ~RtpMaker() {
	}

	virtual void makeRtp(const char *pcData, int iDataLen, uint32_t uiStamp)=0;

	int getInterleaved() const {
		return m_ui8Interleaved;
	}

	int getPlayloadType() const {
		return m_ui8PlayloadType;
	}

	int getSampleRate() const {
		return m_ui32SampleRate;
	}

	uint32_t getSsrc() const {
		return m_ui32Ssrc;
	}

	uint16_t getSeqence() const {
		return m_ui16Sequence;
	}
	uint32_t getTimestamp() const {
		return m_ui32TimeStamp;
	}
protected:
	uint32_t m_ui32Ssrc;
	uint32_t m_ui32SampleRate;
	int m_iMtuSize;
	uint8_t m_ui8PlayloadType;
	uint8_t m_ui8Interleaved;
	uint16_t m_ui16Sequence = 0;
	uint32_t m_ui32TimeStamp = 0;
	virtual void onMakeRtp(const RtpPacket::Ptr &pkt, bool bKeyPos = true) {
		callBack(pkt, bKeyPos);
	}
	inline RtpPacket::Ptr obtainPkt() {
		return m_pktPool.obtain();
	}
private:
	RtspMediaSource::PoolType m_pktPool;
	onGetRTP callBack;
};

} /* namespace RTP */
} /* namespace ZL */

#endif /* RTP_RTPMAKER_H_ */
