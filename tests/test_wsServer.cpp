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
        SockSender::send("from EchoSession:");
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


class EchoSessionWithUrl : public TcpSession {
public:
    EchoSessionWithUrl(const Socket::Ptr &pSock) : TcpSession(pSock){
        DebugL;
    }
    virtual ~EchoSessionWithUrl(){
        DebugL;
    }

    void attachServer(const TcpServer &server) override{
        DebugL << getIdentifier() << " " << TcpSession::getIdentifier();
    }
    void onRecv(const Buffer::Ptr &buffer) override {
        //回显数据
        SockSender::send("from EchoSessionWithUrl:");
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


/**
 * 此对象可以根据websocket 客户端访问的url选择创建不同的对象
 */
struct EchoSessionCreator {
    //返回的TcpSession必须派生于SendInterceptor，可以返回null(拒绝连接)
    TcpSession::Ptr operator()(const Parser &header, const HttpSession &parent, const Socket::Ptr &pSock) {
//        return nullptr;
        if (header.Url() == "/") {
            return std::make_shared<TcpSessionTypeImp<EchoSession> >(header, parent, pSock);
        }
        return std::make_shared<TcpSessionTypeImp<EchoSessionWithUrl> >(header, parent, pSock);
    }
};

int main(int argc, char *argv[]) {
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());

    {
        TcpServer::Ptr httpSrv(new TcpServer());
        //http服务器,支持websocket
        httpSrv->start<WebSocketSessionBase<EchoSessionCreator, HttpSession> >(80);//默认80

        TcpServer::Ptr httpsSrv(new TcpServer());
        //https服务器,支持websocket
        httpsSrv->start<WebSocketSessionBase<EchoSessionCreator, HttpsSession> >(443);//默认443

        TcpServer::Ptr httpSrvOld(new TcpServer());
        //兼容之前的代码(但是不支持根据url选择生成TcpSession类型)
        httpSrvOld->start<WebSocketSession<EchoSession, HttpSession> >(8080);

        DebugL << "请打开网页:http://www.websocket-test.com/,进行测试";
        DebugL << "连接 ws://127.0.0.1/xxxx，ws://127.0.0.1/ 测试的效果将不同，支持根据url选择不同的处理逻辑";

        //设置退出信号处理函数
        static semaphore sem;
        signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
        sem.wait();
    }

    return 0;
}

