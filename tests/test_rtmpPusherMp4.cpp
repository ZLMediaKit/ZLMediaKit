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
#include <iostream>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "Poller/EventPoller.h"
#include "Device/PlayerProxy.h"
#include "Rtmp/RtmpPusher.h"
#include "Common/config.h"
#include "MediaFile/MediaReader.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Rtmp;
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

	RtmpPusher::Ptr pusher;
	//监听RtmpMediaSource注册事件,在流媒体化MP4文件后触发。
	NoticeCenter::Instance().addListener(nullptr,Config::Broadcast::kBroadcastRtmpSrcRegisted,
			[&pusher](BroadcastRtmpSrcRegistedArgs){
		//媒体源"app/stream"已经注册，这时方可新建一个RtmpPusher对象并绑定该媒体源
		const_cast<RtmpPusher::Ptr &>(pusher).reset(new RtmpPusher(app,stream));

		pusher->setOnShutdown([](const SockException &ex){
			WarnL << "已断开与服务器连接:" << ex.getErrCode() << " " << ex.what();
		});

		pusher->setOnPublished([](const SockException &ex){
			if(ex){
				WarnL << "发布失败:" << ex.getErrCode() << " "<< ex.what();
			}else{
				InfoL << "发布成功，请用播放器打开:rtmp://jizan.iok.la/live/test";
			}
		});

		//推流地址，请改成你自己的服务器。
		//这个范例地址（也是基于mediakit）是可用的，但是带宽只有1mb，访问可能很卡顿。
		InfoL << "start publish rtmp!";
		pusher->publish("rtmp://jizan.iok.la/live/test");
	});


	//流媒体化MP4文件，该文件需要放置在 httpRoot/record目录下
	//app必须为“record”，stream为相对于httpRoot/record的路径
	MediaReader::onMakeRtmp("record","live/0/2017-08-22/10-08-44.mp4");

	EventPoller::Instance().runLoop();
	NoticeCenter::Instance().delListener(nullptr);
	pusher.reset();

	EventPoller::Destory();
	Logger::Destory();
	return 0;
}






