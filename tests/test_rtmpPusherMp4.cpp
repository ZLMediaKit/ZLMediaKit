//============================================================================
// Name        : main.cpp
// Author      : 熊子良
// Version     :
//============================================================================


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






