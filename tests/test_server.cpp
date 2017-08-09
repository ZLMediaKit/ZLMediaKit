//============================================================================
// Name        : main.cpp
// Author      : 熊子良
// Version     :
//============================================================================


#include <map>
#include <signal.h>
#include <iostream>
#include "Common/config.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
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
#include "Device/PlayerProxy.h"

#if !defined(_WIN32)
#include "Shell/ShellSession.h"
using namespace ZL::Shell;
#endif // !defined(_WIN32)

using namespace std;
using namespace ZL::Util;
using namespace ZL::Http;
using namespace ZL::Rtsp;
using namespace ZL::Rtmp;
using namespace ZL::Thread;
using namespace ZL::Network;
using namespace ZL::DEV;

void programExit(int arg) {
	EventPoller::Instance().shutdown();
}
int main(int argc,char *argv[]){
   // setExePath(argv[0]);
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Config::loaIniConfig();
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

	TcpServer<RtspSession>::Ptr rtspSrv(new TcpServer<RtspSession>());
	TcpServer<RtmpSession>::Ptr rtmpSrv(new TcpServer<RtmpSession>());
	TcpServer<HttpSession>::Ptr httpSrv(new TcpServer<HttpSession>());
	

	rtspSrv->start(mINI::Instance()[Config::Rtsp::kPort]);
	rtmpSrv->start(mINI::Instance()[Config::Rtmp::kPort]);
	httpSrv->start(mINI::Instance()[Config::Http::kPort]);

#if !defined(_WIN32)
	//简单的telnet服务器，可用于服务器调试，但是不能使用23端口
	//测试方法:telnet 127.0.0.1 8023
	//输入用户名和密码登录(user:test,pwd:123456)，输入help命令查看帮助
	TcpServer<ShellSession>::Ptr shellSrv(new TcpServer<ShellSession>());
	ShellSession::addUser("test","123456");
	shellSrv->start(8023);
#endif // !defined(_WIN32)

#ifdef ENABLE_OPENSSL
	TcpServer<HttpsSession>::Ptr httpsSrv(new TcpServer<HttpsSession>());
	httpsSrv->start(mINI::Instance()[Config::Http::kSSLPort]);
#endif //ENABLE_OPENSSL

	EventPoller::Instance().runLoop();
	proxyMap.clear();
	rtspSrv.reset();
	rtmpSrv.reset();
	httpSrv.reset();

#if !defined(_WIN32)
	shellSrv.reset();
#endif // !defined(_WIN32)

#ifdef ENABLE_OPENSSL
	httpsSrv.reset();
#endif //ENABLE_OPENSSL

	UDPServer::Destory();
	WorkThreadPool::Destory();
	EventPoller::Destory();
	Logger::Destory();
	return 0;
}

