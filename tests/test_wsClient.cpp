/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <string>
#include <iostream>
#include "Util/MD5.h"
#include "Util/logger.h"
#include "Http/WebSocketClient.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

class EchoTcpClient : public TcpClient {
public:
    EchoTcpClient(const EventPoller::Ptr &poller = nullptr){
        InfoL;
    }
    ~EchoTcpClient() override {
        InfoL;
    }
protected:
    void onRecv(const Buffer::Ptr &pBuf) override {
        DebugL << pBuf->toString();
    }
    //被动断开连接回调
    void onErr(const SockException &ex) override {
        WarnL << ex.what();
    }
    //tcp连接成功后每2秒触发一次该事件
    void onManager() override {
        SockSender::send("echo test!");
        DebugL << "send echo test";
    }
    //连接服务器结果回调
    void onConnect(const SockException &ex) override{
        DebugL << ex.what();
    }

    //数据全部发送完毕后回调
    void onFlush() override{
        DebugL;
    }
};

int main(int argc, char *argv[]) {
    //设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号

    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    {
        WebSocketClient<EchoTcpClient>::Ptr client = std::make_shared<WebSocketClient<EchoTcpClient> >();
        client->startConnect("127.0.0.1", 80);
        sem.wait();
    }
    return 0;
}

