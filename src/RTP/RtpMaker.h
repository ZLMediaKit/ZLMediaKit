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
#include "Rtsp/Rtsp.h"
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
	typedef std::shared_ptr<RtpMaker> Ptr;

	RtpMaker(const onGetRTP &cb, uint32_t ui32Ssrc, int iMtuSize,int iSampleRate,
			uint8_t ui8PlayloadType, uint8_t ui8Interleaved) {
		callBack = cb;
		_ui32Ssrc = ui32Ssrc;
		_ui32SampleRate = iSampleRate;
		_iMtuSize = iMtuSize;
		_ui8PlayloadType = ui8PlayloadType;
		_ui8Interleaved = ui8Interleaved;
		_pktPool.setSize(64);
	}
	virtual ~RtpMaker() {
	}

	virtual void makeRtp(const char *pcData, int iDataLen, uint32_t uiStamp)=0;

	int getInterleaved() const {
		return _ui8Interleaved;
	}

	int getPlayloadType() const {
		return _ui8PlayloadType;
	}

	int getSampleRate() const {
		return _ui32SampleRate;
	}

	uint32_t getSsrc() const {
		return _ui32Ssrc;
	}

	uint16_t getSeqence() const {
		return _ui16Sequence;
	}
	uint32_t getTimestamp() const {
		return _ui32TimeStamp;
	}
protected:
	uint32_t _ui32Ssrc;
	uint32_t _ui32SampleRate;
	int _iMtuSize;
	uint8_t _ui8PlayloadType;
	uint8_t _ui8Interleaved;
	uint16_t _ui16Sequence = 0;
	uint32_t _ui32TimeStamp = 0;
	virtual void onMakeRtp(const RtpPacket::Ptr &pkt, bool bKeyPos = true) {
		callBack(pkt, bKeyPos);
	}
	inline RtpPacket::Ptr obtainPkt() {
		return _pktPool.obtain();
	}
private:
	RtspMediaSource::PoolType _pktPool;
	onGetRTP callBack;
};

} /* namespace RTP */
} /* namespace ZL */

#endif /* RTP_RTPMAKER_H_ */
