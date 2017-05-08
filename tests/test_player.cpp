
#include <signal.h>
#include <unistd.h>
#include "Util/logger.h"
#include <iostream>
#include "Poller/EventPoller.h"
#include "Rtsp/UDPServer.h"
#include "Util/onceToken.h"
#include "Device/PlayerProxy.h"
#include "H264Decoder.h"
#include "YuvDisplayer.h"
#include "Network/sockutil.h"

using namespace std;
using namespace ZL::Screen;
using namespace ZL::Codec;
using namespace ZL::Util;
using namespace ZL::Thread;
using namespace ZL::Network;
using namespace ZL::Rtsp;
using namespace ZL::DEV;

void programExit(int arg) {
	EventPoller::Instance().shutdown();
}

int main(int argc, char *argv[]){
	
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	//Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
	signal(SIGINT, programExit);

	if(argc != 5){
		FatalL << "\r\n测试方法：./test_player rtxp_url rtsp_user rtsp_pwd rtp_type\r\n"
		       << "例如：./test_player rtsp://127.0.0.1/live/0 admin 123456 0\r\n"
		       <<endl;
		Logger::Destory();
		return 0;

	}

	MediaPlayer::Ptr player(new MediaPlayer());
	player->setOnPlayResult([](const SockException &ex) {
		InfoL << "OnPlayResult:" << ex.what();
	});
	player->setOnShutdown([](const SockException &ex) {
		ErrorL << "OnShutdown:" << ex.what();
	});

	//DebugL << argv[1] << " " << argv[2] << " " << argv[3] << " " << argv[4] << endl;
	player->play(argv[1],argv[2],argv[3],(PlayerBase::eRtpType)atoi(argv[4]));

	H264Decoder decoder;
	YuvDisplayer displayer;
	ThreadPool pool(1);
	player->setOnVideoCB([&](const H264Frame &frame){
		pool.async([&,frame]() {
					AVFrame *pFrame = nullptr;
					bool flag = decoder.inputVideo((unsigned char *)frame.data.data(), frame.data.size() ,frame.timeStamp, &pFrame);
					if(flag) {
						//DebugL << pFrame->pkt_pts;
						displayer.displayYUV(pFrame);
					}
				});
	});

	EventPoller::Instance().runLoop();


	static onceToken token(nullptr, []() {
		UDPServer::Destory();
        	AsyncTaskThread::Destory();
		EventPoller::Destory();
		Logger::Destory();
	});
	return 0;
}

