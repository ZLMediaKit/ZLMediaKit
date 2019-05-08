/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#ifdef ENABLE_FAAC

#include <cstdlib>
#include "AACEncoder.h"
#include "Util/logger.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <faac.h>
#ifdef __cplusplus
}
#endif

using namespace toolkit;

namespace mediakit {

AACEncoder::AACEncoder() {

}

AACEncoder::~AACEncoder() {
	if (_hEncoder != nullptr) {
		faacEncClose(_hEncoder);
		_hEncoder = nullptr;
	}
	if (_pucAacBuf != nullptr) {
		delete[] _pucAacBuf;
		_pucAacBuf = nullptr;
	}
	if (_pucPcmBuf != nullptr) {
		delete[] _pucPcmBuf;
		_pucPcmBuf = nullptr;
	}
}

bool AACEncoder::init(int iSampleRate, int iChannels, int iSampleBit) {
	if (iSampleBit != 16) {
		return false;
	}
	// (1) Open FAAC engine
	_hEncoder = faacEncOpen(iSampleRate, iChannels, &_ulInputSamples,
			&_ulMaxOutputBytes);
	if (_hEncoder == NULL) {
		return false;
	}
	_pucAacBuf = new unsigned char[_ulMaxOutputBytes];
	_ulMaxInputBytes = _ulInputSamples * iSampleBit / 8;
	_pucPcmBuf = new unsigned char[_ulMaxInputBytes * 4];

	// (2.1) Get current encoding configuration
	faacEncConfigurationPtr pConfiguration = faacEncGetCurrentConfiguration(_hEncoder);
	if (pConfiguration == NULL) {
		faacEncClose(_hEncoder);
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
	if(!faacEncSetConfiguration(_hEncoder, pConfiguration)){
		ErrorL << "faacEncSetConfiguration failed";
		faacEncClose(_hEncoder);
		return false;
	}
	return true;
}

int AACEncoder::inputData(char *pcPcmBufr, int iLen, unsigned char **ppucOutBuffer) {
	memcpy(_pucPcmBuf + _uiPcmLen, pcPcmBufr, iLen);
	_uiPcmLen += iLen;
	if (_uiPcmLen < _ulMaxInputBytes) {
		return 0;
	}

	int nRet = faacEncEncode(_hEncoder, (int32_t *) (_pucPcmBuf), _ulInputSamples, _pucAacBuf, _ulMaxOutputBytes);
	_uiPcmLen -= _ulMaxInputBytes;
	memmove(_pucPcmBuf, _pucPcmBuf + _ulMaxInputBytes, _uiPcmLen);
	*ppucOutBuffer = _pucAacBuf;
	return nRet;
}

} /* namespace mediakit */

#endif //ENABLE_FAAC






