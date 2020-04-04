/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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






