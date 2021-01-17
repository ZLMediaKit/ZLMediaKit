/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <signal.h>
#include <iostream>
#include "Util/File.h"
#include "Util/SSLBox.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"
#include "Common/config.h"
#include "Http/WebSocketSession.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace mediakit {
////////////HTTP配置///////////
namespace Http {
#define HTTP_FIELD "http."
#define HTTP_PORT 80
const char kPort[] = HTTP_FIELD"port";
#define HTTPS_PORT 443
extern const char kSSLPort[] = HTTP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = HTTP_PORT;
    mINI::Instance()[kSSLPort] = HTTPS_PORT;
},nullptr);
}//namespace Http
}  // namespace mediakit

void initEventListener(){
    static onceToken s_token([](){
        NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastHttpRequest,[](BroadcastHttpRequestArgs){
            //const Parser &parser,HttpSession::HttpResponseInvoker &invoker,bool &consumed
            if(strstr(parser.Url().data(),"/api/") != parser.Url().data()){
                return;
            }
            //url以"/api/起始，说明是http api"
            consumed = true;//该http请求已被消费

            _StrPrinter printer;
            ////////////////method////////////////////
            printer << "\r\nmethod:\r\n\t" << parser.Method();
            ////////////////url/////////////////
            printer << "\r\nurl:\r\n\t" << parser.Url();
            ////////////////protocol/////////////////
            printer << "\r\nprotocol:\r\n\t" << parser.Tail();
            ///////////////args//////////////////
            printer << "\r\nargs:\r\n";
            for(auto &pr : parser.getUrlArgs()){
                printer <<  "\t" << pr.first << " : " << pr.second << "\r\n";
            }
            ///////////////header//////////////////
            printer << "\r\nheader:\r\n";
            for(auto &pr : parser.getHeader()){
                printer <<  "\t" << pr.first << " : " << pr.second << "\r\n";
            }
            ////////////////content/////////////////
            printer << "\r\ncontent:\r\n" << parser.Content();
            auto contentOut = printer << endl;

            ////////////////我们测算异步回复，当然你也可以同步回复/////////////////
            EventPollerPool::Instance().getPoller()->async([invoker,contentOut](){
                HttpSession::KeyValue headerOut;
                //你可以自定义header,如果跟默认header重名，则会覆盖之
                //默认header有:Server,Connection,Date,Content-Type,Content-Length
                //请勿覆盖Connection、Content-Length键
                //键名覆盖时不区分大小写
                headerOut["TestHeader"] = "HeaderValue";
                invoker(200,headerOut,contentOut);
            });
        });
    }, nullptr);
}

int main(int argc,char *argv[]){
    //设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号

    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    //加载配置文件，如果配置文件不存在就创建一个
    loadIniConfig();
    initEventListener();

    //加载证书，证书包含公钥和私钥
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    //信任某个自签名证书
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    //不忽略无效证书证书(例如自签名或过期证书)
    SSL_Initor::Instance().ignoreInvalidCertificate(false);


    //开启http服务器
    TcpServer::Ptr httpSrv(new TcpServer());
    httpSrv->start<HttpSession>(mINI::Instance()[Http::kPort]);//默认80

    //如果支持ssl，还可以开启https服务器
    TcpServer::Ptr httpsSrv(new TcpServer());
    httpsSrv->start<HttpsSession>(mINI::Instance()[Http::kSSLPort]);//默认443

    InfoL << "你可以在浏览器输入:http://127.0.0.1/api/my_api?key0=val0&key1=参数1" << endl;

    sem.wait();
    return 0;
}

