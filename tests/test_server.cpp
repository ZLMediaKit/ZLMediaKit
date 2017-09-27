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
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
#include "Shell/ShellSession.h"

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
#include "Device/PlayerProxy.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Http;
using namespace ZL::Rtsp;
using namespace ZL::Rtmp;
using namespace ZL::Shell;
using namespace ZL::Thread;
using namespace ZL::Network;
using namespace ZL::DEV;

void programExit(int arg) {
	EventPoller::Instance().shutdown();
}
int main(int argc,char *argv[]){
    setExePath(argv[0]);
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Config::loadIniConfig();
	DebugL << exePath();
	//support rtmp and rtsp url
	//just support H264+AAC
	auto urlList = {"rtmp://live.hkstv.hk.lxdns.com/live/hks",
					"rtsp://admin:jzan123456@192.168.0.122/"};
	 map<string , PlayerProxy::Ptr> proxyMap;
	 int i=0;
	 for(auto url : urlList){
		 //PlayerProxy构造函数前两个参数分别为应用名（app）,流id（streamId）
		 //比如说应用为live，流id为0，那么直播地址为:
		 //http://127.0.0.1/live/0/hls.m3u8
		 //rtsp://127.0.0.1/live/0
		 //rtmp://127.0.0.1/live/0
		 //录像地址为:
		 //http://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
		 //rtsp://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
		 //rtmp://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
		 PlayerProxy::Ptr player(new PlayerProxy("live",to_string(i++).data()));
		 (*player)[PlayerProxy::kAliveSecond] = 10;//录制10秒
		 player->play(url);
		 proxyMap.emplace(string(url),player);
	 }

#ifdef ENABLE_OPENSSL
	//请把证书"test_server.pem"放置在本程序可执行程序同目录下
	try{
		SSL_Initor::Instance().loadServerPem((exePath() + ".pem").data());
	}catch(...){
		FatalL << "请把证书:" << (exeName() + ".pem") << "放置在本程序可执行程序同目录下:" << exeDir() << endl;
		return 0;
	}
#endif //ENABLE_OPENSSL

	//简单的telnet服务器，可用于服务器调试，但是不能使用23端口
	//测试方法:telnet 127.0.0.1 8023
	//输入用户名和密码登录(user:test,pwd:123456)，输入help命令查看帮助
	TcpServer<ShellSession>::Ptr shellSrv(new TcpServer<ShellSession>());
	TcpServer<RtspSession>::Ptr rtspSrv(new TcpServer<RtspSession>());
	TcpServer<RtmpSession>::Ptr rtmpSrv(new TcpServer<RtmpSession>());
	TcpServer<HttpSession>::Ptr httpSrv(new TcpServer<HttpSession>());
	
	ShellSession::addUser("test", "123456");
	shellSrv->start(8023);
	rtspSrv->start(mINI::Instance()[Config::Rtsp::kPort]);
	rtmpSrv->start(mINI::Instance()[Config::Rtmp::kPort]);
	httpSrv->start(mINI::Instance()[Config::Http::kPort]);

#ifdef ENABLE_OPENSSL
	TcpServer<HttpsSession>::Ptr httpsSrv(new TcpServer<HttpsSession>());
	httpsSrv->start(mINI::Instance()[Config::Http::kSSLPort]);
#endif //ENABLE_OPENSSL

	EventPoller::Instance().runLoop();
	proxyMap.clear();
	shellSrv.reset();
	rtspSrv.reset();
	rtmpSrv.reset();
	httpSrv.reset();

#ifdef ENABLE_OPENSSL
	httpsSrv.reset();
#endif //ENABLE_OPENSSL

	UDPServer::Destory();
	WorkThreadPool::Destory();
	EventPoller::Destory();
	Logger::Destory();
	return 0;
}

