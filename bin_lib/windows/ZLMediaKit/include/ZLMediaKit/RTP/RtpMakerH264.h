/*
 * RtpMakerH264.h
 *
 *  Created on: 2016年8月12日
 *      Author: xzl
 */

#ifndef RTP_RTPMAKERH264_H_
#define RTP_RTPMAKERH264_H_

#include <memory>
#include "RtpMaker.h"
#include "Rtsp/RtspMediaSource.h"
#include "Util/logger.h"
#include "Util/RingBuffer.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Rtsp;

namespace ZL {
namespace Rtsp {

class RtpMaker_H264: public RtpMaker {
public:
	typedef std::shared_ptr<RtpMaker_H264> Ptr;
	RtpMaker_H264(const onGetRTP &cb, uint32_t ui32Ssrc,int iMtuSize = 1400,int iSampleRate = 90000,
			uint8_t ui8PlayloadType = 96, uint8_t ui8Interleaved = 0) :
			RtpMaker(cb, ui32Ssrc, iMtuSize,iSampleRate, ui8PlayloadType, ui8Interleaved) {
	}
	virtual ~RtpMaker_H264() {
	}

	void makeRtp(const char *pcData, int iDataLen, uint32_t uiStamp) override;
private:
	inline void makeH264Rtp(const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp);
	unsigned char aucSectionBuf[1600];
};

} /* namespace RTP */
} /* namespace ZL */

#endif /* RTP_RTPMAKERH264_H_ */
