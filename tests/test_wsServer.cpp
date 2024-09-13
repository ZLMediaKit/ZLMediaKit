/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
 * Echo Session
 
 * [AUTO-TRANSLATED:bc2a4e9e]
*/
class EchoSession : public Session {
public:
    EchoSession(const Socket::Ptr &pSock) : Session(pSock){
        DebugL;
    }
    virtual ~EchoSession(){
        DebugL;
    }

    void attachServer(const Server &server) override{
        DebugL << getIdentifier() << " " << Session::getIdentifier();
    }
    void onRecv(const Buffer::Ptr &buffer) override {
        // 回显数据  [AUTO-TRANSLATED:36769402]
        // Echo Data
        SockSender::send("from EchoSession:");
        send(buffer);
    }
    void onError(const SockException &err) override{
        WarnL << err.what();
    }
    // 每隔一段时间触发，用来做超时管理  [AUTO-TRANSLATED:823ffe1f]
    // Triggered at regular intervals, used for timeout management
    void onManager() override{
        DebugL;
    }
};


class EchoSessionWithUrl : public Session {
public:
    EchoSessionWithUrl(const Socket::Ptr &pSock) : Session(pSock){
        DebugL;
    }
    virtual ~EchoSessionWithUrl(){
        DebugL;
    }

    void attachServer(const Server &server) override{
        DebugL << getIdentifier() << " " << Session::getIdentifier();
    }
    void onRecv(const Buffer::Ptr &buffer) override {
        // 回显数据  [AUTO-TRANSLATED:36769402]
        // Echo Data
        SockSender::send("from EchoSessionWithUrl:");
        send(buffer);
    }
    void onError(const SockException &err) override{
        WarnL << err.what();
    }
    // 每隔一段时间触发，用来做超时管理  [AUTO-TRANSLATED:823ffe1f]
    // Triggered at regular intervals, used for timeout management
    void onManager() override{
        DebugL;
    }
};


/**
 * 此对象可以根据websocket 客户端访问的url选择创建不同的对象
 * This object can create different objects based on the URL accessed by the WebSocket client
 
 * [AUTO-TRANSLATED:57f51c96]
 */
struct EchoSessionCreator {
    // 返回的Session必须派生于SendInterceptor，可以返回null(拒绝连接)  [AUTO-TRANSLATED:014d6a8a]
    // The returned Session must inherit from SendInterceptor, can return null (refuse connection)
    Session::Ptr operator()(const Parser &header, const HttpSession &parent, const Socket::Ptr &pSock, mediakit::WebSocketHeader::Type &type) {
//        return nullptr;
        if (header.url() == "/") {
            // 可以指定传输方式  [AUTO-TRANSLATED:81ddc417]
            // Transport method can be specified
            // type = mediakit::WebSocketHeader::BINARY;
            return std::make_shared<SessionTypeImp<EchoSession> >(header, parent, pSock);
        }
        return std::make_shared<SessionTypeImp<EchoSessionWithUrl> >(header, parent, pSock);
    }
};

int main(int argc, char *argv[]) {
    // 设置日志  [AUTO-TRANSLATED:50372045]
    // Set log
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());

    {
        TcpServer::Ptr httpSrv(new TcpServer());
        // http服务器,支持websocket  [AUTO-TRANSLATED:74a2bdb0]
        // HTTP server, supports WebSocket
        httpSrv->start<WebSocketSessionBase<EchoSessionCreator, HttpSession> >(80);//默认80

        TcpServer::Ptr httpsSrv(new TcpServer());
        // https服务器,支持websocket  [AUTO-TRANSLATED:bc268bb9]
        // HTTPS server, supports WebSocket
        httpsSrv->start<WebSocketSessionBase<EchoSessionCreator, HttpsSession> >(443);//默认443

        TcpServer::Ptr httpSrvOld(new TcpServer());
        // 兼容之前的代码(但是不支持根据url选择生成Session类型)  [AUTO-TRANSLATED:d14395bd]
        // Compatible with previous code (but does not support generating Session type based on URL)
        httpSrvOld->start<WebSocketSession<EchoSession, HttpSession> >(8080);

        DebugL << "请打开网页:http://www.websocket-test.com/,进行测试";
        DebugL << "连接 ws://127.0.0.1/xxxx，ws://127.0.0.1/ 测试的效果将不同，支持根据url选择不同的处理逻辑";

        // 设置退出信号处理函数  [AUTO-TRANSLATED:4f047770]
        // Set exit signal processing function
        static semaphore sem;
        signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
        sem.wait();
    }

    return 0;
}

