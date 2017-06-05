# 一个基于C++11简单易用的轻量级流媒体库
平台|编译状态
----|-------
Linux | [![Build Status](https://travis-ci.org/xiongziliang/ZLMediaKit.svg?branch=master)](https://travis-ci.org/xiongziliang/ZLMediaKit)
macOS | [![Build Status](https://travis-ci.org/xiongziliang/ZLMediaKit_build_for_mac.svg?branch=master)](https://travis-ci.org/xiongziliang/ZLMediaKit_build_for_mac)
iOS | [![Build Status](https://travis-ci.org/xiongziliang/ZLMediaKit-build_for_ios.svg?branch=master)](https://travis-ci.org/xiongziliang/ZLMediaKit-build_for_ios)
Android | [![Build Status](https://travis-ci.org/xiongziliang/ZLMediaKit_build_for_android.svg?branch=master)](https://travis-ci.org/xiongziliang/ZLMediaKit_build_for_android)

## 项目特点
- 基于C++11开发，避免使用裸指针，代码稳定可靠；同时跨平台移植简单方便，代码清晰简洁。
- 打包多种流媒体协议(RTSP/RTMP/HLS），支持协议间的互相转换，提供一站式的服务。
- 使用epoll+线程池+异步网络IO模式开发，并发性能优越。
- 只实现主流的的H264+AAC流媒体方案，代码精简,脉络清晰，适合学习。

## 功能清单
- RTSP
  - RTSP 服务器，支持RTMP/MP4转RTSP。
  - RTSP 播放器，支持RTSP代理。
  - 支持 `rtp over udp` `rtp over tcp` `rtp over http` `rtp组播`  四种RTP传输方式 。

- RTMP
  - RTMP 播放服务器，支持RTSP/MP4转RTMP。
  - RTMP 发布服务器，支持录制发布流。
  - RTMP 播放器，支持RTMP代理。
  - RTMP 推流客户端。

- HLS
  - 支持HLS文件生成，自带HTTP文件服务器。
  
- HTTP[S]
  - 服务器支持`目录索引生成`,`文件下载`,`表单提交请求`。
  - 客户端提供`文件下载器(支持断点续传)`,`接口请求器`。

- 其他
  - 支持输入YUV+PCM自动生成RTSP/RTMP/HLS/MP4.
  - 支持简单的telnet调试。
  - 支持H264的解析，支持B帧的POC计算排序。
 
## 后续任务
- 提供更多的示例代码

## 编译(Linux)
- 我的编译环境
  - Ubuntu16.04 64 bit + gcc5.4(最低gcc4.7)
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
## 使用方法
- 作为服务器：
```
	TcpServer<RtspSession>::Ptr rtspSrv(new TcpServer<RtspSession>());
	TcpServer<RtmpSession>::Ptr rtmpSrv(new TcpServer<RtmpSession>());
	TcpServer<HttpSession>::Ptr httpSrv(new TcpServer<HttpSession>());
        TcpServer<HttpsSession>::Ptr httpsSrv(new TcpServer<HttpsSession>());
	
	rtspSrv->start(mINI::Instance()[Config::Rtsp::kPort]);
	rtmpSrv->start(mINI::Instance()[Config::Rtmp::kPort]);
	httpSrv->start(mINI::Instance()[Config::Http::kPort]);
        httpsSrv->start(mINI::Instance()[Config::Http::kSSLPort]);
        EventPoller::Instance().runLoop();
```

- 作为播放器：
```
        MediaPlayer::Ptr player(new MediaPlayer());
	player->setOnPlayResult([](const SockException &ex) {
		InfoL << "OnPlayResult:" << ex.what();
	});
	player->setOnShutdown([](const SockException &ex) {
		ErrorL << "OnShutdown:" << ex.what();
	});
        player->setOnVideoCB([&](const H264Frame &frame){
		//在这里解码H264并显示
	});
        player->setOnAudioCB([&](const AdtsFrame &frame){
		//在这里解码AAC并播放
	});
        //支持rtmp、rtsp
	player->play("rtsp://192.168.0.122/","admin","123456",PlayerBase::RTP_TCP);
	EventPoller::Instance().runLoop();
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
## QA
- 为什么VLC播放一段时间就停止了？
    
    由于ZLMediaKit在实现RTSP协议时，采用OPTIONS命令作为心跳包（在RTP over UDP时有效），如果播放器不持续发送OPTIONS指令，那么ZLMediaKit会断开连接。如果你要用第三方播放器测试，你可以改RTP over TCP方式或者修改ZLMediaKit的源码，修改位置位置为src/Rtsp/RtspSession.cpp RtspSession::onManager函数,修改成如下所示：

```
  	void RtspSession::onManager() {
		if (m_ticker.createdTime() > 10 * 1000) {
			if (m_strSession.size() == 0) {
				WarnL << "非法链接:" << getPeerIp();
				shutdown();
				return;
			}
			if (m_bListenPeerUdpPort) {
				UDPServer::Instance().stopListenPeer(getPeerIp().data(), this);
				m_bListenPeerUdpPort = false;
			}
		}
       		/*if (m_rtpType != PlayerBase::RTP_TCP && m_ticker.elapsedTime() > 15 * 1000) {
			WarnL << "RTSP会话超时:" << getPeerIp();
			shutdown();
			return;
 		/*}
  	}
```
- 怎么测试服务器性能？
    
    ZLMediaKit提供了测试性能的示例，代码在tests/test_benchmark.cpp。由于ZLToolKit默认关闭了tcp客户端多线程的支持，如果需要提高测试并发量，需要在编译ZLToolKit时启用ENABLE_ASNC_TCP_CLIENT宏，具体操作如下：
	
```
    #编译ZLToolKit
    cd ZLToolKit
    mkdir -p build
    cd build -DENABLE_ASNC_TCP_CLIENT
    cmake ..
    make -j4
    sudo make install
```

- github下载太慢了，有其他下载方式吗？
    
    你可以在通过开源中国获取最新的代码，地址为：
 
    [ZLToolKit](http://git.oschina.net/xiahcu/ZLToolKit)
  
    [ZLMediaKit](http://git.oschina.net/xiahcu/ZLMediaKit)


## 联系方式
- 邮箱：<771730766@qq.com>
- QQ群：542509000

