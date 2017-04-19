//============================================================================
// Name        : main.cpp
// Author      : 熊子良
// Version     :
//============================================================================


#include <signal.h>
#include <unistd.h>
#include <iostream>
#include "Util/SqlPool.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.hpp"
#include "Thread/WorkThreadPool.h"
#include "Http/HttpSession.h"
#include "Http/HttpsSession.h"
#include "Util/SSLBox.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Http;
using namespace ZL::Rtsp;
using namespace ZL::Rtmp;
using namespace ZL::Thread;
using namespace ZL::Network;

void programExit(int arg) {
	EventPoller::Instance().shutdown();
}
int main(int argc,char *argv[]){
	signal(SIGINT, programExit);

	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
	SSL_Initor::Instance().loadServerPem((exeDir() + ".HttpServer.pem").data());

	TcpServer<RtspSession>::Ptr rtspSrv(new TcpServer<RtspSession>());
	TcpServer<RtmpSession>::Ptr rtmpSrv(new TcpServer<RtmpSession>());
	TcpServer<HttpSession>::Ptr httpSrv(new TcpServer<HttpSession>());
	TcpServer<HttpsSession>::Ptr httpsSrv(new TcpServer<HttpsSession>());
	rtspSrv->start(mINI::Instance()[Config::Rtsp::kPort]);
	rtmpSrv->start(mINI::Instance()[Config::Rtmp::kPort]);
	httpSrv->start(mINI::Instance()[Config::Http::kPort]);
	httpsSrv->start(mINI::Instance()[Config::Http::kSSLPort]);

	EventPoller::Instance().runLoop();

	rtspSrv.reset();
	rtmpSrv.reset();
	httpSrv.reset();
	httpsSrv.reset();
	static onceToken token(nullptr, []() {
		UDPServer::Destory();
		WorkThreadPool::Destory();
		EventPoller::Destory();
		Logger::Destory();
	});
	return 0;
}

