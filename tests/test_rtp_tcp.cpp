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
#include "Network/TcpClient.h"
using namespace std;
using namespace toolkit;

int main(int argc,char *argv[]) {
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel"));
    if(argc < 4){
        ErrorL << "用法: ./test_rtp_tcp rtp_file server_url server_port";
        return -1;
    }
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        ErrorL << "打开文件失败:" << argv[1];
        return -1;
    }

    static semaphore sem;
    Socket::Ptr socket = std::make_shared<Socket>();
    socket->connect(argv[2], atoi(argv[3]), [&](const SockException &err) {
        if (err) {
            ErrorL << "连接服务器" << argv[2] << ":" << atoi(argv[3]) << "失败:" << err.what();
            sem.post();
            return;
        }
        char buf[4 * 1024];
        while (true) {
            auto size = fread(buf, 1, sizeof(buf), fp);
            if (size < sizeof(buf)) {
                break;
            }
            socket->send(buf, size);
            //休眠
            usleep(10 * 1000);
        }
        sem.post();
    });

    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    fclose(fp);
}


