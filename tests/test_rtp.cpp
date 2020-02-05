/*
 * MIT License
 *
 * Copyright (c) 2019 Gemfield <gemfield@civilnet.cn>
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

#include <map>
#include <iostream>
#include "Util/MD5.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/util.h"
#include "Network/TcpServer.h"
#include "Common/config.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
#include "Rtp/RtpSelector.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

#if defined(ENABLE_RTPPROXY)
static bool loadFile(const char *path){
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        WarnL << "open file failed:" << path;
        return false;
    }

    uint32_t timeStamp_last = 0;
    uint16_t len;
    char rtp[2 * 1024];
    struct sockaddr addr = {0};
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
        RtpSelector::Instance().inputRtp(rtp,len, &addr,&timeStamp);
        if(timeStamp_last){
            auto diff = timeStamp - timeStamp_last;
            if(diff > 0){
                usleep(diff * 1000);
            }
        }
        timeStamp_last = timeStamp;
    }
    fclose(fp);
    return true;
}
#endif//#if defined(ENABLE_RTPPROXY)

int main(int argc,char *argv[]) {
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel"));
#if defined(ENABLE_RTPPROXY)
    //启动异步日志线程
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    loadIniConfig((exeDir() + "config.ini").data());
    TcpServer::Ptr rtspSrv(new TcpServer());
    TcpServer::Ptr rtmpSrv(new TcpServer());
    TcpServer::Ptr httpSrv(new TcpServer());
    rtspSrv->start<RtspSession>(554);//默认554
    rtmpSrv->start<RtmpSession>(1935);//默认1935
    httpSrv->start<HttpSession>(80);//默认80
    //此处可以选择MP4V-ES或MP2P
    mINI::Instance()[RtpProxy::kRtpType] = "MP4V-ES";
    //此处选择是否导出调试文件
//    mINI::Instance()[RtpProxy::kDumpDir] = "/Users/xzl/Desktop/";

    loadFile(argv[1]);
#else
    ErrorL << "please ENABLE_RTPPROXY and then test";
#endif//#if defined(ENABLE_RTPPROXY)
    return 0;
}


