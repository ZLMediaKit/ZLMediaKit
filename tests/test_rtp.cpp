/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
        RtpSelector::Instance().inputRtp(nullptr,rtp,len, &addr,&timeStamp);
        if(timeStamp_last){
            auto diff = timeStamp - timeStamp_last;
            if(diff > 0 && diff < 500){
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
    //此处选择是否导出调试文件
//    mINI::Instance()[RtpProxy::kDumpDir] = "/Users/xzl/Desktop/";

    if (argc == 2)
      loadFile(argv[1]);
    else
      ErrorL << "parameter error.";
#else
    ErrorL << "please ENABLE_RTPPROXY and then test";
#endif//#if defined(ENABLE_RTPPROXY)
    return 0;
}


