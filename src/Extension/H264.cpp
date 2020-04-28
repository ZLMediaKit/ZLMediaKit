/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H264.h"
#include "SPSParser.h"
#include "Util/logger.h"
using namespace toolkit;

namespace mediakit{

bool getAVCInfo(const char * sps,int sps_len,int &iVideoWidth, int &iVideoHeight, float  &iVideoFps){
    T_GetBitContext tGetBitBuf;
    T_SPS tH264SpsInfo;
    memset(&tGetBitBuf,0,sizeof(tGetBitBuf));
    memset(&tH264SpsInfo,0,sizeof(tH264SpsInfo));
    tGetBitBuf.pu8Buf = (uint8_t*)sps + 1;
    tGetBitBuf.iBufSize = sps_len - 1;
    if(0 != h264DecSeqParameterSet((void *) &tGetBitBuf, &tH264SpsInfo)){
        return false;
    }
    h264GetWidthHeight(&tH264SpsInfo, &iVideoWidth, &iVideoHeight);
    h264GeFramerate(&tH264SpsInfo, &iVideoFps);
    //ErrorL << iVideoWidth << " " << iVideoHeight << " " << iVideoFps;
    return true;
}

bool getAVCInfo(const string& strSps,int &iVideoWidth, int &iVideoHeight, float  &iVideoFps) {
    return getAVCInfo(strSps.data(),strSps.size(),iVideoWidth,iVideoHeight,iVideoFps);
}

const char *memfind(const char *buf, int len, const char *subbuf, int sublen) {
    for (auto i = 0; i < len - sublen; ++i) {
        if (memcmp(buf + i, subbuf, sublen) == 0) {
            return buf + i;
        }
    }
    return NULL;
}

void splitH264(const char *ptr, int len, const std::function<void(const char *, int)> &cb) {
    auto nal = ptr;
    auto end = ptr + len;
    while(true) {
        auto next_nal = memfind(nal + 3,end - nal - 3,"\x0\x0\x1",3);
        if(next_nal){
            if(*(next_nal - 1) == 0x00){
                next_nal -= 1;
            }
            cb(nal,next_nal - nal);
            nal = next_nal;
            continue;
        }
        cb(nal,end - nal);
        break;
    }
}

#if 0
//splitH264函数测试程序
static onceToken s_token([](){
    char buf[] = "\x00\x00\x00\x01\x12\x23\x34\x45\x56"
                 "\x00\x00\x00\x01\x12\x23\x34\x45\x56"
                 "\x00\x00\x00\x01\x12\x23\x34\x45\x56"
                 "\x00\x00\x01\x12\x23\x34\x45\x56";
    splitH264(buf, sizeof(buf) - 1, [](const char *ptr, int len){
        cout << hexdump(ptr, len) << endl;
    });
});
#endif //0

Sdp::Ptr H264Track::getSdp() {
    if(!ready()){
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<H264Sdp>(getSps(),getPps());
}
}//namespace mediakit


