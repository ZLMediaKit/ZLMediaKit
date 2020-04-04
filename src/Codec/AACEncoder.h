/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CODEC_AACENCODER_H_
#define CODEC_AACENCODER_H_

namespace mediakit {

class AACEncoder {
public:
    AACEncoder(void);
    virtual ~AACEncoder(void);
    bool init(int iSampleRate, int iAudioChannel, int iAudioSampleBit);
    int inputData(char *pcData, int iLen, unsigned char **ppucOutBuffer);

private:
    unsigned char *_pucPcmBuf = nullptr;
    unsigned int _uiPcmLen = 0;

    unsigned char *_pucAacBuf = nullptr;
    void *_hEncoder = nullptr;

    unsigned long _ulInputSamples = 0;
    unsigned long _ulMaxInputBytes = 0;
    unsigned long _ulMaxOutputBytes = 0;

};

} /* namespace mediakit */

#endif /* CODEC_AACENCODER_H_ */
