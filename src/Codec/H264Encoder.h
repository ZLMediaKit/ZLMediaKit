/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */


#ifndef CODEC_H264ENCODER_H_
#define CODEC_H264ENCODER_H_

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
#include <x264.h>
#ifdef __cplusplus
}
#endif //__cplusplus

namespace mediakit {

class H264Encoder {
public:
    typedef struct {
        int iType;
        int iLength;
        uint8_t *pucData;
    } H264Frame;

    H264Encoder(void);
    virtual ~H264Encoder(void);
    bool init(int iWidth, int iHeight, int iFps);
    int inputData(char *apcYuv[3], int aiYuvLen[3], int64_t i64Pts, H264Frame **ppFrame);
private:
    x264_t* _pX264Handle = nullptr;
    x264_picture_t* _pPicIn = nullptr;
    x264_picture_t* _pPicOut = nullptr;
    H264Frame _aFrames[10];
};

} /* namespace mediakit */

#endif /* CODEC_H264ENCODER_H_ */
