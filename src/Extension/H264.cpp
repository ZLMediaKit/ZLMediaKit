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

void splitH264(const char *ptr, int len, int prefix, const std::function<void(const char *, int, int)> &cb) {
    auto start = ptr + prefix;
    auto end = ptr + len;
    int next_prefix;
    while (true) {
        auto next_start = memfind(start, end - start, "\x00\x00\x01", 3);
        if (next_start) {
            //找到下一帧
            if (*(next_start - 1) == 0x00) {
                //这个是00 00 00 01开头
                next_start -= 1;
                next_prefix = 4;
            } else {
                //这个是00 00 01开头
                next_prefix = 3;
            }
            //记得加上本帧prefix长度
            cb(start - prefix, next_start - start + prefix, prefix);
            //搜索下一帧末尾的起始位置
            start = next_start + next_prefix;
            //记录下一帧的prefix长度
            prefix = next_prefix;
            continue;
        }
        //未找到下一帧,这是最后一帧
        cb(start - prefix, end - start + prefix, prefix);
        break;
    }
}

int prefixSize(const char *ptr, int len){
    if (len < 4) {
        return 0;
    }

    if (ptr[0] != 0x00 || ptr[1] != 0x00) {
        //不是0x00 00开头
        return 0;
    }

    if (ptr[2] == 0x00 && ptr[3] == 0x01) {
        //是0x00 00 00 01
        return 4;
    }

    if (ptr[2] == 0x01) {
        //是0x00 00 01
        return 3;
    }
    return 0;
}

#if 0
//splitH264函数测试程序
static onceToken s_token([](){
    {
        char buf[] = "\x00\x00\x00\x01\x12\x23\x34\x45\x56"
                     "\x00\x00\x00\x01\x23\x34\x45\x56"
                     "\x00\x00\x00\x01x34\x45\x56"
                     "\x00\x00\x01\x12\x23\x34\x45\x56";
        splitH264(buf, sizeof(buf) - 1, 4, [](const char *ptr, int len, int prefix) {
            cout << prefix << " " << hexdump(ptr, len) << endl;
        });
    }

    {
        char buf[] = "\x00\x00\x00\x01\x12\x23\x34\x45\x56";
        splitH264(buf, sizeof(buf) - 1, 4, [](const char *ptr, int len, int prefix) {
            cout << prefix << " " << hexdump(ptr, len) << endl;
        });
    }
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


