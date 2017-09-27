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

using namespace std;
using namespace ZL::Util;
using namespace ZL::Rtmp;
using namespace ZL::Thread;
using namespace ZL::Network;
using namespace ZL::DEV;


int domain(int argc, const char *argv[]) {
	signal(SIGINT, [](int){EventPoller::Instance().shutdown();});
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));

	PlayerProxy::Ptr player(new PlayerProxy("app", "stream"));
	//拉一个流，生成一个RtmpMediaSource，源的名称是"app/stream"
	//你也可以以其他方式生成RtmpMediaSource，比如说MP4文件（请研读MediaReader代码）
	player->play(argv[1]);

	RtmpPusher::Ptr pusher;
	//监听RtmpMediaSource注册事件,在PlayerProxy播放成功后触发。
	NoticeCenter::Instance().addListener(nullptr, Config::Broadcast::kBroadcastRtmpSrcRegisted,
		[&pusher, argv](BroadcastRtmpSrcRegistedArgs) {
		//媒体源"app/stream"已经注册，这时方可新建一个RtmpPusher对象并绑定该媒体源
		const_cast<RtmpPusher::Ptr &>(pusher).reset(new RtmpPusher(app, stream));
		string appTmp(app), streamTmp(stream);

		pusher->setOnShutdown([appTmp,streamTmp, argv](const SockException &ex) {
			WarnL << "已断开与服务器连接(Server connection is closed):" << ex.getErrCode() << " " << ex.what();
			AsyncTaskThread::Instance().CancelTask(0);
			AsyncTaskThread::Instance().DoTaskDelay(0, 2000, [appTmp, streamTmp, argv]() {
				InfoL << "正在重新发布(Re-Publish Steam)...";
				NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastRtmpSrcRegisted, appTmp.data(), streamTmp.data());
				return false;
			});
		});

		pusher->setOnPublished([appTmp, streamTmp,argv](const SockException &ex) {
			if (ex) {
				WarnL << "发布失败(Publish fail):" << ex.getErrCode() << " " << ex.what();
				AsyncTaskThread::Instance().CancelTask(0);
				AsyncTaskThread::Instance().DoTaskDelay(0, 2000, [appTmp, streamTmp, argv]() {
					InfoL << "正在重新发布(Re-Publish Steam)...";
					NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastRtmpSrcRegisted, appTmp.data(), streamTmp.data());
					return false;
				});
			}else {
				InfoL << "发布成功，请用播放器打开(Publish success,Please use play with player):" << argv[2];
			}
		});

		//开始推流
		pusher->publish(argv[2]);
	});

	EventPoller::Instance().runLoop();
	NoticeCenter::Instance().delListener(nullptr);
	player.reset();
	pusher.reset();

	EventPoller::Destory();
	Logger::Destory();
	return 0;
}



int main(int argc,char *argv[]){
	const char *argList[] = {argv[0],"rtmp://live.hkstv.hk.lxdns.com/live/hks","rtmp://jizan.iok.la/live/test"};
	return domain(argc,argList);
}






