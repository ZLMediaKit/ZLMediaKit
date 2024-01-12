/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <iostream>
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/TcpServer.h"
#include "Common/config.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
#include "Rtp/RtpSelector.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

static semaphore sem;

#if defined(ENABLE_RTPPROXY)
static bool loadFile(const char *path, const EventPoller::Ptr &poller) {
    std::shared_ptr<FILE> fp(fopen(path, "rb"), [](FILE *fp) {
        sem.post();
        if (fp) {
            fclose(fp);
        }
    });
    if (!fp) {
        WarnL << "open file failed:" << path;
        return false;
    }

    struct sockaddr_storage addr;
    memset(&addr, 0, sizeof(addr));
    addr.ss_family = AF_INET;
    auto sock = Socket::createSocket(poller);
    auto process = RtpSelector::Instance().getProcess("test", true);

    uint64_t stamp_last = 0;
    auto total_size = std::make_shared<size_t>(0);
    auto do_read = [fp, total_size, sock, addr, process, stamp_last]() mutable -> int {
        uint16_t len;
        char rtp[0xFFFF];
        while (true) {
            if (2 != fread(&len, 1, 2, fp.get())) {
                WarnL << "Read rtp size failed";
                // 重新播放
                fseek(fp.get(), 0, SEEK_SET);
                return 1;
            }
            len = ntohs(len);
            if (len < 12 || len > sizeof(rtp)) {
                WarnL << "Invalid rtp size: " << len;
                return 0;
            }

            if (len != fread(rtp, 1, len, fp.get())) {
                WarnL << "Read rtp data failed";
                return 0;
            }
            (*total_size) += len;
            uint64_t stamp = 0;
            try {
                process->inputRtp(true, sock, rtp, len, (struct sockaddr *)&addr, &stamp);
            } catch (std::exception &ex) {
                WarnL << "Input rtp failed: " << ex.what();
                return 0;
            }

            auto diff = stamp - stamp_last;
            if (diff < 0 || diff > 500) {
                diff = 1;
            }
            if (diff) {
                stamp_last = stamp;
                return diff;
            }
        }
    };
    poller->doDelayTask(1, [do_read, total_size, process]() mutable {
        auto ret = do_read();
        if (!ret) {
            WarnL << *total_size / 1024 << "KB";
            RtpSelector::Instance().delProcess("test", process.get());
        }
        return ret;
    });

    return true;
}
#endif // #if defined(ENABLE_RTPPROXY)

int main(int argc, char *argv[]) {
    // 设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel"));
#if defined(ENABLE_RTPPROXY)
    // 启动异步日志线程
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    loadIniConfig((exeDir() + "config.ini").data());
    TcpServer::Ptr rtspSrv(new TcpServer());
    TcpServer::Ptr rtmpSrv(new TcpServer());
    TcpServer::Ptr httpSrv(new TcpServer());
    rtspSrv->start<RtspSession>(554); // 默认554
    rtmpSrv->start<RtmpSession>(1935); // 默认1935
    httpSrv->start<HttpSession>(80); // 默认80
    // 此处选择是否导出调试文件
    // mINI::Instance()[RtpProxy::kDumpDir] = "/Users/xzl/Desktop/";

    if (argc == 2) {
        loadFile(argv[1], EventPollerPool::Instance().getPoller());
        sem.wait();
    } else {
        ErrorL << "parameter error.";
    }
#else
    ErrorL << "please ENABLE_RTPPROXY and then test";
#endif // #if defined(ENABLE_RTPPROXY)
    return 0;
}
