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
        send("echo test!");
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

    WebSocketClient<EchoTcpClient>::Ptr client = std::make_shared<WebSocketClient<EchoTcpClient> >();
    client->startConnect("121.40.165.18",8800);

    sem.wait();
    return 0;
}

