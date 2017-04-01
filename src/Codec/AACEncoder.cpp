/*
 * AACEncoder.cpp
 *
 *  Created on: 2016年8月11日
 *      Author: xzl
 */
#ifdef ENABLE_FAAC
#include "AACEncoder.h"
#include "Util/logger.h"
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

#include <faac.h>

#ifdef __cplusplus
}
#endif

using namespace ZL::Util;

namespace ZL {
namespace Codec {

AACEncoder::AACEncoder() {

}

AACEncoder::~AACEncoder() {
	if (m_hEncoder != nullptr) {
		faacEncClose(m_hEncoder);
		m_hEncoder = nullptr;
	}
	if (m_pucAacBuf != nullptr) {
		delete[] m_pucAacBuf;
		m_pucAacBuf = nullptr;
	}
	if (m_pucPcmBuf != nullptr) {
		delete[] m_pucPcmBuf;
		m_pucPcmBuf = nullptr;
	}
}

bool AACEncoder::init(int iSampleRate, int iChannels, int iSampleBit) {
	if (iSampleBit != 16) {
		return false;
	}
	// (1) Open FAAC engine
	m_hEncoder = faacEncOpen(iSampleRate, iChannels, &m_ulInputSamples,
			&m_ulMaxOutputBytes);
	if (m_hEncoder == NULL) {
		return false;
	}
	m_pucAacBuf = new unsigned char[m_ulMaxOutputBytes];
	m_ulMaxInputBytes = m_ulInputSamples * iSampleBit / 8;
	m_pucPcmBuf = new unsigned char[m_ulMaxInputBytes * 4];

	// (2.1) Get current encoding configuration
	faacEncConfigurationPtr pConfiguration = faacEncGetCurrentConfiguration(m_hEncoder);
	if (pConfiguration == NULL) {
		faacEncClose(m_hEncoder);
		return false;
	}
	pConfiguration->aacObjectType =LOW;
	pConfiguration->mpegVersion = 4;
	pConfiguration->useTns = 1;
	pConfiguration->shortctl = SHORTCTL_NORMAL;
	pConfiguration->useLfe = 1;
	pConfiguration->allowMidside = 1;
	pConfiguration->bitRate = 0;
	pConfiguration->bandWidth = 0;
	pConfiguration->quantqual = 50;
	pConfiguration->outputFormat = 1;
	pConfiguration->inputFormat = FAAC_INPUT_16BIT;

	// (2.2) Set encoding configuration
	if(!faacEncSetConfiguration(m_hEncoder, pConfiguration)){
		ErrorL << "faacEncSetConfiguration failed";
		faacEncClose(m_hEncoder);
		return false;
	}
	return true;
}

int AACEncoder::inputData(char *pcPcmBufr, int iLen, unsigned char **ppucOutBuffer) {
	memcpy(m_pucPcmBuf + m_uiPcmLen, pcPcmBufr, iLen);
	m_uiPcmLen += iLen;
	if (m_uiPcmLen < m_ulMaxInputBytes) {
		return 0;
	}

	int nRet = faacEncEncode(m_hEncoder, (int32_t *) (m_pucPcmBuf), m_ulInputSamples, m_pucAacBuf, m_ulMaxOutputBytes);
	m_uiPcmLen -= m_ulMaxInputBytes;
	memmove(m_pucPcmBuf, m_pucPcmBuf + m_ulMaxInputBytes, m_uiPcmLen);
	*ppucOutBuffer = m_pucAacBuf;
	return nRet;
}

} /* namespace Codec */
} /* namespace ZL */

#endif //ENABLE_FAAC






