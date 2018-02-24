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
#include <signal.h>
#include <unistd.h>
#include "Util/logger.h"
#include <iostream>
#include "Poller/EventPoller.h"
#include "Rtsp/UDPServer.h"
#include "Player/MediaPlayer.h"
#include "Util/onceToken.h"
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
using namespace ZL::Player;

int main(int argc, char *argv[]){
	//设置退出信号处理函数
	signal(SIGINT, [](int){EventPoller::Instance().shutdown();});
	//设置日志
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	if(argc != 3){
		FatalL << "\r\n测试方法：./test_player rtxp_url rtp_type\r\n"
		       << "例如：./test_player rtsp://admin:123456@127.0.0.1/live/0 0\r\n"
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
	(*player)[RtspPlayer::kRtpType] = atoi(argv[2]);
	player->play(argv[1]);

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
		EventPoller::Destory();
		AsyncTaskThread::Destory();
		Logger::Destory();
	});
	return 0;
}

