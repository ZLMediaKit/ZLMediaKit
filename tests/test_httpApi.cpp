/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#include <map>
#include <signal.h>
#include <iostream>
#include "Common/config.h"
#include "Http/HttpSession.h"
#ifdef ENABLE_OPENSSL
#include "Util/SSLBox.h"
#include "Http/HttpsSession.h"
#endif//ENABLE_OPENSSL

#include "Util/File.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"
#include "Thread/WorkThreadPool.h"
#include "Util/NoticeCenter.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Http;
using namespace ZL::Thread;
using namespace ZL::Network;

static onceToken s_token([](){
    NoticeCenter::Instance().addListener(nullptr,Config::Broadcast::kBroadcastHttpRequest,[](BroadcastHttpRequestArgs){
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
        for(auto &pr : parser.getValues()){
            printer <<  "\t" << pr.first << " : " << pr.second << "\r\n";
        }
        ////////////////content/////////////////
        printer << "\r\ncontent:\r\n" << parser.Content();
        auto contentOut = printer << endl;

        ////////////////我们测算异步回复，当然你也可以同步回复/////////////////
        EventPoller::Instance().sync([invoker,contentOut](){
            HttpSession::KeyValue headerOut;
			//你可以自定义header,如果跟默认header重名，则会覆盖之
			//默认header有:Server,Connection,Date,Content-Type,Content-Length
			//请勿覆盖Connection、Content-Length键
			//键名覆盖时不区分大小写
            headerOut["TestHeader"] = "HeaderValue";
            invoker("200 OK",headerOut,contentOut);
        });
    });
}, nullptr);

int main(int argc,char *argv[]){
	//设置退出信号处理函数
	signal(SIGINT, [](int){EventPoller::Instance().shutdown();});
	//设置日志
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
	//加载配置文件，如果配置文件不存在就创建一个
	Config::loadIniConfig();

#ifdef ENABLE_OPENSSL
	//请把证书"test_httpApi.pem"放置在本程序可执行程序同目录下
	try{
		//加载证书，证书包含公钥和私钥
		SSL_Initor::Instance().loadServerPem((exePath() + ".pem").data());
	}catch(...){
		FatalL << "请把证书:" << (exeName() + ".pem") << "放置在本程序可执行程序同目录下:" << exeDir() << endl;
		return 0;
	}
#endif //ENABLE_OPENSSL

	//开启http服务器
	TcpServer::Ptr httpSrv(new TcpServer());
	httpSrv->start<HttpSession>(mINI::Instance()[Config::Http::kPort]);//默认80

#ifdef ENABLE_OPENSSL
    //如果支持ssl，还可以开启https服务器
	TcpServer::Ptr httpsSrv(new TcpServer());
	httpsSrv->start<HttpsSession>(mINI::Instance()[Config::Http::kSSLPort]);//默认443
#endif //ENABLE_OPENSSL

	InfoL << "你可以在浏览器输入:http://127.0.0.1/api/my_api?key0=val0&key1=参数1" << endl;

	EventPoller::Instance().runLoop();

	static onceToken s_token(nullptr,[]() {
		//TcpServer用到了WorkThreadPool
		WorkThreadPool::Destory();
		EventPoller::Destory();
		Logger::Destory();
	});
	return 0;
}

