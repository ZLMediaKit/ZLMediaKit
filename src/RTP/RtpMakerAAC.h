/*
 * RtpMakerAAC.h
 *
 *  Created on: 2016年8月12日
 *      Author: xzl
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
