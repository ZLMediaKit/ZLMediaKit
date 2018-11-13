# 一个基于C++11的高性能运营级流媒体服务框架
 [![Build Status](https://travis-ci.org/xiongziliang/ZLMediaKit.svg?branch=master)](https://travis-ci.org/xiongziliang/ZLMediaKit)

## 项目特点
- 基于C++11开发，避免使用裸指针，代码稳定可靠；同时跨平台移植简单方便，代码清晰简洁。
- 打包多种流媒体协议(RTSP/RTMP/HLS），支持协议间的互相转换，提供一站式的服务。
- 使用epoll+线程池+异步网络IO模式开发，并发性能优越。
- 已实现主流的的H264+AAC流媒体方案，代码精简,脉络清晰，适合学习。
- 编码格式与框架代码解耦，方便自由简洁的添加支持其他编码格式
- 代码经过大量的稳定性、性能测试，可满足商用服务器项目。
- 支持linux、macos、ios、android、windows平台

## 项目定位
- 移动嵌入式跨平台流媒体解决方案。
- 商用级流媒体服务器。
- 网络编程二次开发SDK。


## 功能清单
- RTSP
  - RTSP 服务器，支持RTMP/MP4转RTSP。
  - RTSP 播放器，支持RTSP代理，支持生成静音音频
  - 支持 `rtp over udp` `rtp over tcp` `rtp over http` `rtp组播`  四种RTP传输方式 。
  - 服务器/客户端完整支持Basic/Digest方式的登录鉴权，全异步可配置化的鉴权接口。
  - 支持H265编码

- RTMP
  - RTMP 播放服务器，支持RTSP/MP4转RTMP。
  - RTMP 发布服务器，支持录制发布流。
  - RTMP 播放器，支持RTMP代理，支持生成静音音频
  - RTMP 推流客户端。
  - 支持http-flv直播。
  - 支持https-flv直播。

- HLS
  - 支持HLS文件生成，自带HTTP文件服务器。
  
- HTTP[S]
  - 服务器支持`目录索引生成`,`文件下载`,`表单提交请求`。
  - 客户端提供`文件下载器(支持断点续传)`,`接口请求器`,`文件上传器`。
  - 完整HTTP API服务器，可以作为web后台开发框架。
  - 支持跨域访问。
  - 客户端支持cookie
  - 支持WebSocket服务器

- 其他
  - 支持输入YUV+PCM自动生成RTSP/RTMP/HLS/MP4.
  - 支持简单的telnet调试。
  - 支持H264的解析，支持B帧的POC计算排序。
  - 支持配置文件热加载
  - 支持流量统计、推流播放鉴权等事件
  - 支持rtsp/rtmp/http虚拟主机
  - 支持flv、mp4文件录制
  - 支持rtps/rtmp点播，支持seek

 
## 后续任务
- 完善支持H265
- 添加rtsp推流功能

## 编译要求
- 编译器支持C++11，GCC4.8/Clang3.3/VC2015或以上
- cmake3.2或以上

## 编译(Linux)
- 我的编译环境
  - Ubuntu16.04 64 bit + gcc5.4
  - cmake 3.5.1
- 编译
  
  ```
  cd ZLMediaKit
  ./build_for_linux.sh
  ```  
    
## 编译(macOS)
- 我的编译环境
  - macOS Sierra(10.12.1) + xcode8.3.1
  - Homebrew 1.1.3
  - cmake 3.8.0
- 编译
  
  ```
  cd ZLMediaKit
  ./build_for_mac.sh
  ```
	 
## 编译(iOS)
- 编译环境:`请参考macOS的编译指导。`
- 编译
  
  ```
  cd ZLMediaKit
  ./build_for_ios.sh
  ```
- 你也可以生成Xcode工程再编译：

  ```
  cd ZLMediaKit
  mkdir -p build
  cd build
  # 生成Xcode工程，工程文件在build目录下
  cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/iOS.cmake -DIOS_PLATFORM=SIMULATOR64 -G "Xcode"
  ```
  
## 编译(Android)
- 我的编译环境
  - macOS Sierra(10.12.1) + xcode8.3.1
  - Homebrew 1.1.3
  - cmake 3.8.0
  - [android-ndk-r14b](https://dl.google.com/android/repository/android-ndk-r14b-darwin-x86_64.zip)
- 编译

  ```
  cd ZLMediaKit
  export ANDROID_NDK_ROOT=/path/to/ndk
  ./build_for_android.sh
  ```
## 编译(Windows)
- 我的编译环境
  - windows 10
  - visual studio 2017
  - [cmake-gui](https://cmake.org/files/v3.10/cmake-3.10.0-rc1-win32-x86.msi)
  
- 编译
```
   1 使用cmake-gui打开工程并生成vs工程文件.
   2 找到工程文件(ZLMediaKit.sln),双击用vs2017打开.
   3 选择编译Release 版本.
   4 找到目标文件并运行测试用例.
```
## 使用方法
- 作为服务器：
	```
	TcpServer::Ptr rtspSrv(new TcpServer());
	TcpServer::Ptr rtmpSrv(new TcpServer());
	TcpServer::Ptr httpSrv(new TcpServer());
	TcpServer::Ptr httpsSrv(new TcpServer());
	
	rtspSrv->start<RtspSession>(mINI::Instance()[Config::Rtsp::kPort]);
	rtmpSrv->start<RtmpSession>(mINI::Instance()[Config::Rtmp::kPort]);
	httpSrv->start<HttpSession>(mINI::Instance()[Config::Http::kPort]);
	httpsSrv->start<HttpsSession>(mINI::Instance()[Config::Http::kSSLPort]);
	EventPoller::Instance().runLoop();
	```

- 作为播放器：
	```
    MediaPlayer::Ptr player(new MediaPlayer());
    weak_ptr<MediaPlayer> weakPlayer = player;
    player->setOnPlayResult([weakPlayer](const SockException &ex) {
        InfoL << "OnPlayResult:" << ex.what();
        auto strongPlayer = weakPlayer.lock();
        if (ex || !strongPlayer) {
            return;
        }

        auto viedoTrack = strongPlayer->getTrack(TrackVideo);
        if (!viedoTrack) {
            WarnL << "没有视频Track!";
            return;
        }
        viedoTrack->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([](const Frame::Ptr &frame) {
            //此处解码并播放
        }));
    });

    player->setOnShutdown([](const SockException &ex) {
        ErrorL << "OnShutdown:" << ex.what();
    });

    //支持rtmp、rtsp
    (*player)[RtspPlayer::kRtpType] = PlayerBase::RTP_TCP;
    player->play("rtsp://admin:jzan123456@192.168.0.122/");
	```
- 作为代理服务器：
	```
	//support rtmp and rtsp url
	//just support H264+AAC
	auto urlList = {"rtmp://live.hkstv.hk.lxdns.com/live/hks",
			"rtsp://184.72.239.149/vod/mp4://BigBuckBunny_175k.mov"};
	map<string , PlayerProxy::Ptr> proxyMap;
	int i=0;
	for(auto url : urlList){
		//PlayerProxy构造函数前两个参数分别为应用名（app）,流id（streamId）
		//比如说应用为live，流id为0，那么直播地址为：
		//http://127.0.0.1/live/0/hls.m3u8
		//rtsp://127.0.0.1/live/0
		//rtmp://127.0.0.1/live/0
		//录像地址为：
		//http://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
		//rtsp://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
		//rtmp://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
		PlayerProxy::Ptr player(new PlayerProxy("live",to_string(i++).data()));
		player->play(url);
		proxyMap.emplace(string(url),player);
	}
	```
	
- 作为推流客户端器：
	```
	PlayerProxy::Ptr player(new PlayerProxy("app","stream"));
	//拉一个流，生成一个RtmpMediaSource，源的名称是"app/stream"
	//你也可以以其他方式生成RtmpMediaSource，比如说MP4文件（请研读MediaReader代码）
	player->play("rtmp://live.hkstv.hk.lxdns.com/live/hks");

	RtmpPusher::Ptr pusher;
	//监听RtmpMediaSource注册事件,在PlayerProxy播放成功后触发。
	NoticeCenter::Instance().addListener(nullptr,Config::Broadcast::kBroadcastRtmpSrcRegisted,
			[&pusher](BroadcastRtmpSrcRegistedArgs){
		//媒体源"app/stream"已经注册，这时方可新建一个RtmpPusher对象并绑定该媒体源
		const_cast<RtmpPusher::Ptr &>(pusher).reset(new RtmpPusher(app,stream));

		//推流地址，请改成你自己的服务器。
		//这个范例地址（也是基于mediakit）是可用的，但是带宽只有1mb，访问可能很卡顿。
		pusher->publish("rtmp://jizan.iok.la/live/test");
	});

	EventPoller::Instance().runLoop();
	```
## QA
- 怎么测试服务器性能？
    
    ZLMediaKit提供了测试性能的示例，代码在tests/test_benchmark.cpp。

    这里是测试报告：[benchmark.md](https://github.com/xiongziliang/ZLMediaKit/blob/master/benchmark.md)

- github下载太慢了，有其他下载方式吗？
    
    你可以在通过开源中国获取最新的代码，地址为：
 
    [ZLToolKit](http://git.oschina.net/xiahcu/ZLToolKit)
  
    [ZLMediaKit](http://git.oschina.net/xiahcu/ZLMediaKit)

## 参考案例
 - [IOS摄像头实时录制,生成rtsp/rtmp/hls/http-flv](https://gitee.com/xiahcu/IOSMedia)
 - [IOS rtmp/rtsp播放器，视频推流器](https://gitee.com/xiahcu/IOSPlayer)
 - [支持linux、windows、mac的rtmp/rtsp播放器](https://github.com/xiongziliang/ZLMediaPlayer)

   上述工程可能在最新的代码的情况下编译不过，请手动修改
 

## 授权协议

本项目自有代码使用宽松的MIT协议，在保留版权信息的情况下可以自由应用于各自商用、非商业的项目。
但是本项目也零碎的使用了一些其他的开源代码，在商用的情况下请自行替代或剔除；
由于使用本项目而产生的商业纠纷或侵权行为一概与本项项目及开发者无关，请自行承担法律风险。

## 联系方式
 - 邮箱：<771730766@qq.com>
 - QQ群：542509000

## 捐赠
如果本项目能切实帮助您减少重复开发的工作量，您可以在自愿的基础上支持下作者，谢谢！

[支付宝](https://raw.githubusercontent.com/xiongziliang/other/master/IMG_3919.JPG)

[微信](https://raw.githubusercontent.com/xiongziliang/other/master/IMG_3920.JPG)

	

