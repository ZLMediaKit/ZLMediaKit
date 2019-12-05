/*
* MIT License
*
* Copyright (c) 2016-2019 Gemfield <gemfield@civilnet.cn>
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

#include <cstdint>
#include <cstdio>
#include <sys/socket.h>
#include "RtpFileLoader.h"
#include "Util/logger.h"
#include "RtpSelector.h"
using namespace toolkit;

bool RtpFileLoader::loadFile(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        WarnL << "open file failed:" << path;
        return false;
    }

    uint32_t timeStamp_last = 0;
    uint16_t len;
    char rtp[2 * 1024];
    while (true) {
        if (2 != fread(&len, 1, 2, fp)) {
            WarnL;
            break;
        }
        len = ntohs(len);
        if (len < 12 || len > sizeof(rtp)) {
            WarnL << len;
            break;
        }

        if (len != fread(rtp, 1, len, fp)) {
            WarnL;
            break;
        }

        uint32_t timeStamp;
        memcpy(&timeStamp, rtp + 4, 4);
        timeStamp = ntohl(timeStamp);
        timeStamp /= 90;

        if(timeStamp_last){
            auto diff = timeStamp - timeStamp_last;
            if(diff > 0){
                usleep(diff * 1000);
            }
        }

        timeStamp_last = timeStamp;
        RtpSelector::Instance().inputRtp(rtp,len, nullptr);
    }

    fclose(fp);
    return true;
}
