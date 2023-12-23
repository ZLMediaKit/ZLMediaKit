/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "test_video_stack.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

static std::unordered_map<uint16_t, toolkit::TcpServer::Ptr> _srvMap;

// 播放地址 http://127.0.0.1:7089/stack/89.live.flv
int main(int argc, char *argv[]) {

    EventPollerPool::enableCpuAffinity(false);   //是否开启cpu亲和性
    Logger::Instance().add(std::make_shared<ConsoleChannel>());

    TcpServer::Ptr httpSrv(new TcpServer());
    httpSrv->start<HttpSession>(7089);
    _srvMap.emplace(7089, httpSrv);

    VideoStackManager vs;

    for (int i = 0; i < 100;i++) {
        vs.start(testJson);
		std::this_thread::sleep_for(std::chrono::seconds(60));
        vs.stop("89");
		std::this_thread::sleep_for(std::chrono::seconds(10));
    }


    getchar();
    return 0;
}
