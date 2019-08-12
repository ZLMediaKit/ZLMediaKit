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
            cb(nal,next_nal - nal);
            nal = next_nal;
            continue;
        }
        cb(nal,end - nal);
        break;
    }
}


Sdp::Ptr H264Track::getSdp() {
    if(!ready()){
        WarnL << "H264 Track未准备好";
        return nullptr;
    }
    return std::make_shared<H264Sdp>(getSps(),getPps());
}
}//namespace mediakit


