![logo](https://raw.githubusercontent.com/zlmediakit/ZLMediaKit/master/logo.png)

[english readme](https://github.com/xiongziliang/ZLMediaKit/blob/master/README_en.md)

# 一个基于C++11的高性能运营级流媒体服务框架
 [![Build Status](https://travis-ci.org/xiongziliang/ZLMediaKit.svg?branch=master)](https://travis-ci.org/xiongziliang/ZLMediaKit)


## 国内用户请使用gitee镜像下载
```
git clone --depth 1 https://gitee.com/xiahcu/ZLMediaKit
cd ZLMediaKit
git submodule update --init
```
## 项目特点
- 基于C++11开发，避免使用裸指针，代码稳定可靠；同时跨平台移植简单方便，代码清晰简洁。
- 打包多种流媒体协议(RTSP/RTMP/HLS/HTTP-FLV/Websocket-FLV），支持协议间的互相转换，提供一站式的服务。
- 使用epoll+线程池+异步网络IO模式开发，并发性能优越。
- 已实现主流的的H264/H265+AAC流媒体方案，代码精简,脉络清晰，适合学习。
- 编码格式与框架代码解耦，方便自由简洁的添加支持其他编码格式。
- 代码经过大量的稳定性、性能测试，可满足商用服务器项目。
- 支持linux、macos、ios、android、windows平台。
- 支持画面秒开(GOP缓存)、极低延时([500毫秒内，最低可达100毫秒](https://github.com/zlmediakit/ZLMediaKit/wiki/%E5%BB%B6%E6%97%B6%E6%B5%8B%E8%AF%95))。
- [ZLMediaKit高并发实现原理](https://github.com/xiongziliang/ZLMediaKit/wiki/ZLMediaKit%E9%AB%98%E5%B9%B6%E5%8F%91%E5%AE%9E%E7%8E%B0%E5%8E%9F%E7%90%86)。
- 提供完善的标准[C API](https://github.com/xiongziliang/ZLMediaKit/tree/master/api/include),可以作SDK用，或供其他语言调用。
- 提供完整的[MediaServer](https://github.com/xiongziliang/ZLMediaKit/tree/master/server)服务器，可以免开发直接部署为商用服务器。

## 项目定位
- 移动嵌入式跨平台流媒体解决方案。
- 商用级流媒体服务器。
- 网络编程二次开发SDK。


## 功能清单
- RTSP
  - RTSP 服务器，支持RTMP/MP4转RTSP。
  - RTSPS 服务器，支持亚马逊echo show这样的设备
  - RTSP 播放器，支持RTSP代理，支持生成静音音频
  - RTSP 推流客户端与服务器
  - 支持 `rtp over udp` `rtp over tcp` `rtp over http` `rtp组播`  四种RTP传输方式 。
  - 服务器/客户端完整支持Basic/Digest方式的登录鉴权，全异步可配置化的鉴权接口。
  - 支持H265编码
  - 服务器支持RTSP推流(包括`rtp over udp` `rtp over tcp`方式)
  - 支持任意编码格式的rtsp推流，只是除H264/H265+AAC外无法转协议

- RTMP
  - RTMP 播放服务器，支持RTSP/MP4转RTMP。
  - RTMP 发布服务器，支持录制发布流。
  - RTMP 播放器，支持RTMP代理，支持生成静音音频
  - RTMP 推流客户端。
  - 支持http-flv直播。
  - 支持https-flv直播。
  - 支持任意编码格式的rtmp推流，只是除H264/H265+AAC外无法转协议

- HLS
  - 支持HLS文件生成，自带HTTP文件服务器。
  - 支持播放鉴权，鉴权结果可以缓存为cookie

- HTTP[S]
  - 服务器支持`目录索引生成`,`文件下载`,`表单提交请求`。
  - 客户端提供`文件下载器(支持断点续传)`,`接口请求器`,`文件上传器`。
  - 完整HTTP API服务器，可以作为web后台开发框架。
  - 支持跨域访问。
  - 支持http客户端、服务器cookie
  - 支持WebSocket服务器和客户端
  - 支持http文件访问鉴权

- 其他
  - 支持输入YUV+PCM自动生成RTSP/RTMP/HLS/MP4.
  - 支持简单的telnet调试。
  - 支持H264的解析，支持B帧的POC计算排序。
  - 支持配置文件热加载
  - 支持流量统计、推流播放鉴权等事件
  - 支持rtsp/rtmp/http虚拟主机
  - 支持flv、mp4文件录制
  - 支持rtps/rtmp协议的mp4点播，支持seek
  - 支持按需拉流，无人观看自动关断拉流
  - 支持先拉流后推流，提高及时推流画面打开率
  - 支持rtsp/rtmp/http-flv/hls播放鉴权(url参数方式)
 


## 其他功能细节表

- 转协议:

    |          功能/编码格式           | H264 | H265 | AAC  | other |
    | :------------------------------: | :--: | :--: | :--: | :---: |
    | RTSP[S] --> RTMP/HTTP[S]-FLV/FLV |  Y   |  N   |  Y   |   N   |
    |         RTMP --> RTSP[S]         |  Y   |  N   |  Y   |   N   |
    |         RTSP[S] --> HLS          |  Y   |  Y   |  Y   |   N   |
    |           RTMP --> HLS           |  Y   |  N   |  Y   |   N   |
    |         RTSP[S] --> MP4          |  Y   |  Y   |  Y   |   N   |
    |           RTMP --> MP4           |  Y   |  N   |  Y   |   N   |
    |         MP4 --> RTSP[S]          |  Y   |  N   |  Y   |   N   |
    |           MP4 --> RTMP           |  Y   |  N   |  Y   |   N   |

- 流生成：

  |          功能/编码格式             | H264 | H265 | AAC  | other |
  | :------------------------------: | :--: | :--: | :--: | :---: |
  | RTSP[S]推流 |  Y   |  Y  |  Y   |   Y   |
  |         RTSP拉流代理         |  Y   |  Y  |  Y   |   Y   |
  |   RTMP推流    |  Y   |  Y   |  Y   |   Y   |
  | RTMP拉流代理  |  Y   |  Y   |  Y   |   Y   |

- RTP传输方式:

  |  功能/RTP传输方式   | tcp  | udp  | http | udp_multicast |
  | :-----------------: | :--: | :--: | :--: | :-----------: |
  | RTSP[S] Play Server |  Y   |  Y   |  Y   |       Y       |
  | RTSP[S] Push Server |  Y   |  Y   |  N   |       N       |
  |     RTSP Player     |  Y   |  Y   |  N   |       Y       |
  |     RTSP Pusher     |  Y   |  Y   |  N   |       N       |


- 支持的服务器类型列表

  |      服务类型       | Y/N  |
  | :-----------------: | :--: |
  | RTSP[S] Play Server |  Y   |
  | RTSP[S] Push Server |  Y   |
  |        RTMP         |  Y   |
  |  HTTP[S]/WebSocket[S]  |  Y   |

- 支持的客户端类型

  | 客户端类型  | Y/N  |
  | :---------: | :--: |
  | RTSP Player |  Y   |
  | RTSP Pusher |  Y   |
  | RTMP Player |  Y   |
  | RTMP Pusher |  Y   |
  |   HTTP[S]   |  Y   |
  | WebSocket[S] |  Y  |

## 后续任务
- 完善支持H265

## 编译要求
- 编译器支持C++11，GCC4.8/Clang3.3/VC2015或以上
- cmake3.2或以上

## 编译前必看！！！

- **必须使用git下载完整的代码，不要使用下载zip包的方式下载源码，否则子模块代码默认不下载！你可以像以下这样操作:**
```
git clone https://github.com/zlmediakit/ZLMediaKit.git
cd ZLMediaKit
git submodule update --init
```

## 编译(Linux)
- 我的编译环境
  - Ubuntu16.04 64 bit + gcc5.4
  - cmake 3.5.1
- 编译
  
  ```
	//如果是centos6.x,需要先安装较新版本的gcc以及cmake，然后打开脚本build_for_linux.sh手动编译
	//如果是ubuntu这样的比较新的系统版本可以直接操作第4步

	1、安装GCC5.2(如果gcc版本高于4.7可以跳过此步骤)
	sudo yum install centos-release-scl -y
	sudo yum install devtoolset-4-toolchain -y
	scl enable devtoolset-4 bash

	2、安装cmake
	#需要安装新版本cmake,当然你也可以通过yum或者apt-get方式安装(前提是版本够新)
	tar -xvf cmake-3.10.0-rc4.tar.gz
	cd cmake-3.10.0-rc4
	./configure
	make -j4
	sudo make install

	3、切换高版本gcc
	scl enable devtoolset-4 bash

	4、编译
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
- 你也可以生成Xcode工程再编译,[了解更多](https://github.com/leetal/ios-cmake):

  ```
  cd ZLMediaKit
  mkdir -p build
  cd build
  # 生成Xcode工程，工程文件在build目录下
  cmake .. -G Xcode -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake  -DPLATFORM=OS64COMBINED
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
   1 进入ZLMediaKit目录执行 git submodule update --init 以下载ZLToolKit的代码
   2 使用cmake-gui打开工程并生成vs工程文件.
   3 找到工程文件(ZLMediaKit.sln),双击用vs2017打开.
   4 选择编译Release 版本.
   5 找到目标文件并运行测试用例.
```
## 使用方法
- 作为服务器：
	```cpp
	TcpServer::Ptr rtspSrv(new TcpServer());
	TcpServer::Ptr rtmpSrv(new TcpServer());
	TcpServer::Ptr httpSrv(new TcpServer());
	TcpServer::Ptr httpsSrv(new TcpServer());
	
	rtspSrv->start<RtspSession>(mINI::Instance()[Config::Rtsp::kPort]);
	rtmpSrv->start<RtmpSession>(mINI::Instance()[Config::Rtmp::kPort]);
	httpSrv->start<HttpSession>(mINI::Instance()[Config::Http::kPort]);
	httpsSrv->start<HttpsSession>(mINI::Instance()[Config::Http::kSSLPort]);
	```

- 作为播放器：
	```cpp
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
    (*player)[Client::kRtpType] = Rtsp::RTP_TCP;
    player->play("rtsp://admin:jzan123456@192.168.0.122/");
	```
- 作为代理服务器：
	```cpp
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
	```cpp
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

	```
## QA
- 怎么测试服务器性能？

    ZLMediaKit提供了测试性能的示例，代码在tests/test_benchmark.cpp。

    这里是测试报告：[benchmark.md](https://github.com/xiongziliang/ZLMediaKit/blob/master/benchmark.md)

- github下载太慢了，有其他下载方式吗？

    你可以在通过开源中国获取最新的代码，地址为：

    [ZLToolKit](http://git.oschina.net/xiahcu/ZLToolKit)

    [ZLMediaKit](http://git.oschina.net/xiahcu/ZLMediaKit)


- 在windows下编译很多错误？

    由于本项目主体代码在macOS/linux下开发，部分源码采用的是无bom头的UTF-8编码；由于windows对于utf-8支持不甚友好，所以如果发现编译错误请先尝试添     加bom头再编译。
    也可以通过参考这篇博客解决:
    [vs2015:/utf-8选项解决UTF-8 without BOM 源码中文输出乱码问题](https://blog.csdn.net/10km/article/details/80203286)

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
 - 邮箱：<771730766@qq.com>(本项目相关或流媒体相关问题请走issue流程，否则恕不邮件答复)
 - QQ群：542509000
 
## 怎么提问？
如果要对项目有相关疑问，建议您这么做：
 - 1、仔细看下readme、wiki，如果有必要可以查看下issue.
 - 2、如果您的问题还没解决，可以提issue.
 - 3、有些问题，如果不具备参考性的，无需在issue提的，可以在qq群提.
 - 4、QQ私聊一般不接受无偿技术咨询和支持(谈谈人生理想还是可以的😂)，毕竟精力有限，谢谢理解.

## 捐赠
欢迎捐赠以便更好的推动项目的发展，谢谢您的支持!

[支付宝](https://raw.githubusercontent.com/xiongziliang/other/master/IMG_3919.JPG)

[微信](https://raw.githubusercontent.com/xiongziliang/other/master/IMG_3920.JPG)



