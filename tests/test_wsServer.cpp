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
#include "Http/WebSocketSession.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

/**
* 回显会话
*/
class EchoSession : public TcpSession {
public:
    EchoSession(const Socket::Ptr &pSock) : TcpSession(pSock){
        DebugL;
    }
    virtual ~EchoSession(){
        DebugL;
    }

    void attachServer(const TcpServer &server) override{
        DebugL << getIdentifier() << " " << TcpSession::getIdentifier();
    }
    void onRecv(const Buffer::Ptr &buffer) override {
        //回显数据
        send(buffer);
    }
    void onError(const SockException &err) override{
        WarnL << err.what();
    }
    //每隔一段时间触发，用来做超时管理
    void onManager() override{
        DebugL;
    }
};

int main(int argc, char *argv[]) {
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());

    TcpServer::Ptr httpSrv(new TcpServer());
    //http服务器,支持websocket
    httpSrv->start<WebSocketSession<EchoSession,HttpSession>>(80);//默认80

    TcpServer::Ptr httpsSrv(new TcpServer());
    //https服务器,支持websocket
    httpsSrv->start<WebSocketSession<EchoSession,HttpsSession>>(443);//默认443

    DebugL << "请打开网页:http://www.websocket-test.com/,连接 ws://127.0.0.1/测试";

    //设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    return 0;
}

