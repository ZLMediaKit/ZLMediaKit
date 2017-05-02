//============================================================================
// Name        : main.cpp
// Author      : 熊子良
// Version     :
//============================================================================


#include <signal.h>
#include <unistd.h>
#include <iostream>
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"

#ifdef ENABLE_OPENSSL
#include "Util/SSLBox.h"
#include "Http/HttpsSession.h"
#endif//ENABLE_OPENSSL

#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/File.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"
#include "Thread/WorkThreadPool.h"
#include "Device/PlayerProxy.h"
#include <map>
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
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));

	//support rtmp and rtsp url
	//just support H264+AAC
	auto urlList = {"rtmp://live.hkstv.hk.lxdns.com/live/hks",
					"rtsp://184.72.239.149/vod/mp4://BigBuckBunny_175k.mov"};
	 map<string , PlayerProxy::Ptr> proxyMap;
	 int i=0;
	 for(auto url : urlList){
		 PlayerProxy::Ptr player(new PlayerProxy("live",std::to_string(i++).data()));
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


#ifdef ENABLE_OPENSSL
	TcpServer<HttpsSession>::Ptr httpsSrv(new TcpServer<HttpsSession>());
	httpsSrv->start(mINI::Instance()[Config::Http::kSSLPort]);
#endif //ENABLE_OPENSSL


	EventPoller::Instance().runLoop();

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

