![logo](https://raw.githubusercontent.com/zlmediakit/ZLMediaKit/master/www/logo.png)

[简体中文](./README.md) | English

# An high-performance, enterprise-level streaming media service framework based on C++11.


[![](https://img.shields.io/badge/license-MIT-green.svg)](https://github.com/ZLMediaKit/ZLMediaKit/blob/master/LICENSE)
[![](https://img.shields.io/badge/language-c++-red.svg)](https://en.cppreference.com/)
[![](https://img.shields.io/badge/platform-linux%20|%20macos%20|%20windows-blue.svg)](https://github.com/ZLMediaKit/ZLMediaKit)
[![](https://img.shields.io/badge/PRs-welcome-yellow.svg)](https://github.com/ZLMediaKit/ZLMediaKit/pulls)

[![](https://github.com/ZLMediaKit/ZLMediaKit/actions/workflows/android.yml/badge.svg)](https://github.com/ZLMediaKit/ZLMediaKit)
[![](https://github.com/ZLMediaKit/ZLMediaKit/actions/workflows/linux.yml/badge.svg)](https://github.com/ZLMediaKit/ZLMediaKit)
[![](https://github.com/ZLMediaKit/ZLMediaKit/actions/workflows/macos.yml/badge.svg)](https://github.com/ZLMediaKit/ZLMediaKit)
[![](https://github.com/ZLMediaKit/ZLMediaKit/actions/workflows/windows.yml/badge.svg)](https://github.com/ZLMediaKit/ZLMediaKit)

[![](https://github.com/ZLMediaKit/ZLMediaKit/actions/workflows/docker.yml/badge.svg)](https://hub.docker.com/r/zlmediakit/zlmediakit/tags)
[![](https://img.shields.io/docker/pulls/zlmediakit/zlmediakit)](https://hub.docker.com/r/zlmediakit/zlmediakit/tags)

## Project Features
- Developed with C++11, avoiding the use of raw pointers, providing stable and reliable code with superior performance.
- Supports multiple protocols (RTSP/RTMP/HLS/HTTP-FLV/WebSocket-FLV/GB28181/HTTP-TS/WebSocket-TS/HTTP-fMP4/WebSocket-fMP4/MP4/WebRTC), and protocol conversion.
- Developed with multiplexing/multithreading/asynchronous network IO models, providing excellent concurrency performance and supporting massive client connections.
- The code has undergone extensive stability and performance testing, and has been extensively used in production environments.
- Supports all major platforms, including linux, macos, ios, android, and windows.
- Supports multiple instruction set platforms, such as x86, arm, risc-v, mips, Loongson, and Shenwei.
- Provides ultra-fast startup, extremely low latency (within 500 milliseconds, and can be as low as 100 milliseconds), and excellent user experience.
- Provides a comprehensive standard [C API](https://github.com/ZLMediaKit/ZLMediaKit/tree/master/api/include) that can be used as an SDK or called by other languages.
- Provides a complete [MediaServer](https://github.com/ZLMediaKit/ZLMediaKit/tree/master/server) server, which can be deployed directly as a commercial server without additional development.
- Provides a complete [restful api](https://github.com/ZLMediaKit/ZLMediaKit/wiki/MediaServer%E6%94%AF%E6%8C%81%E7%9A%84HTTP-API) and [web hook](https://github.com/ZLMediaKit/ZLMediaKit/wiki/MediaServer%E6%94%AF%E6%8C%81%E7%9A%84HTTP-HOOK-API), supporting rich business logic.
- Bridges the video surveillance protocol stack and the live streaming protocol stack, and provides comprehensive support for RTSP/RTMP.
- Fully supports H265/H264/AAC/G711/OPUS.
- Provides complete functions, including clustering, on-demand protocol conversion, on-demand push/pull streams, playback before publishing, and continuous publishing after disconnection.
- Provides ultimate performance, supporting 10W-level players on a single machine and 100Gb/s-level IO bandwidth capability.
- Provides ultimate user experience with [exclusive features](https://github.com/ZLMediaKit/ZLMediaKit/wiki/ZLMediakit%E7%8B%AC%E5%AE%B6%E7%89%B9%E6%80%A7%E4%BB%8B%E7%BB%8D).
- [Who is using zlmediakit?](https://github.com/ZLMediaKit/ZLMediaKit/issues/511)
- Fully supports IPv6 networks.

## Project Positioning

- Cross-platform streaming media solution for mobile and embedded systems.
- Commercial-grade streaming media server.
- Network programming secondary development SDK.

## Feature List
### Overview of Features
<img width="800" alt="Overview of Features" src="https://github.com/ZLMediaKit/ZLMediaKit/assets/11495632/481ea769-5b27-495e-bf7d-31191e6af9d2">

- RTSP[S]
  - RTSP[S] server, supports RTMP/MP4/HLS to RTSP[S] conversion, supports devices such as Amazon Echo Show
  - RTSP[S] player, supports RTSP proxy, supports generating silent audio
  - RTSP[S] push client and server
  - Supports four RTP transmission modes: `rtp over udp` `rtp over tcp` `rtp over http` `rtp multicast`
  - Server/client fully supports Basic/Digest authentication, asynchronous configurable authentication interface
  - Supports H265 encoding
  - The server supports RTSP pushing (including `rtp over udp` and `rtp over tcp`)
  - Supports H264/H265/AAC/G711/OPUS/MJPEG encoding. Other encodings can be forwarded but cannot be converted to protocol

- RTMP[S]
  - RTMP[S] playback server, supports RTSP/MP4/HLS to RTMP conversion
  - RTMP[S] publishing server, supports recording and publishing streams
  - RTMP[S] player, supports RTMP proxy, supports generating silent audio
  - RTMP[S] push client
  - Supports http[s]-flv live streaming server
  - Supports http[s]-flv live streaming player
  - Supports websocket-flv live streaming
  - Supports H264/H265/AAC/G711/OPUS encoding. Other encodings can be forwarded but cannot be converted to protocol
  - Supports [RTMP-H265](https://github.com/ksvc/FFmpeg/wiki)
  - Supports [RTMP-OPUS](https://github.com/ZLMediaKit/ZLMediaKit/wiki/RTMP%E5%AF%B9H265%E5%92%8COPUS%E7%9A%84%E6%94%AF%E6%8C%81)
  - Supports [enhanced-rtmp(H265)](https://github.com/veovera/enhanced-rtmp)

- HLS
  - Supports HLS file(mpegts/fmp4) generation and comes with an HTTP file server
  - Through cookie tracking technology, it can simulate HLS playback as a long connection, which can achieve HLS on-demand pulling, playback statistics, and other businesses
  - Supports HLS player and can pull HLS to rtsp/rtmp/mp4
  - Supports H264/H265/AAC/G711/OPUS encoding

- TS
  - Supports http[s]-ts live streaming
  - Supports ws[s]-ts live streaming
  - Supports H264/H265/AAC/G711/OPUS encoding

- fMP4
  - Supports http[s]-fmp4 live streaming
  - Supports ws[s]-fmp4 live streaming
  - Supports H264/H265/AAC/G711/OPUS/MJPEG encoding

- HTTP[S] and WebSocket
  - The server supports `directory index generation`, `file download`, `form submission requests`
  - The client provides `file downloader (supports resume breakpoint)`, `interface requestor`, `file uploader`
  - Complete HTTP API server, which can be used as a web backend development framework
  - Supports cross-domain access
  - Supports http client/server cookie
  - Supports WebSocket server and client
  - Supports http file access authentication
 
- GB28181 and RTP Streaming
  - Supports UDP/TCP RTP (PS/TS/ES) streaming server, which can be converted to RTSP/RTMP/HLS and other protocols
  - Supports RTSP/RTMP/HLS and other protocol conversion to RTP streaming client, supports TCP/UDP mode, provides corresponding RESTful API, supports active and passive modes
  - Supports H264/H265/AAC/G711/OPUS encoding
  - Supports ES/PS/TS/EHOME RTP streaming
  - Supports ES/PS RTP forwarding
  - Supports GB28181 active pull mode
  - Supports two-way voice intercom

- MP4 VOD and Recording
  - Supports recording as FLV/HLS/MP4
  - Supports MP4 file playback for RTSP/RTMP/HTTP-FLV/WS-FLV, supports seek
  - Supports H264/H265/AAC/G711/OPUS encoding

- WebRTC
  - Supports WebRTC streaming and conversion to other protocols
  - Supports WebRTC playback and conversion from other protocols to WebRTC
  - Supports two-way echo testing
  - Supports simulcast streaming
  - Supports uplink and downlink RTX/NACK packet loss retransmission
  - **Supports single-port, multi-threaded, and client network connection migration (unique in the open source community)**.
  - Supports TWCC RTCP dynamic rate control
  - Supports REMB/PLI/SR/RR RTCP
  - Supports RTP extension parsing
  - Supports GOP buffer and instant WebRTC playback
  - Supports data channels
  - Supports WebRTC over TCP mode
  - Excellent NACK and jitter buffer algorithms with outstanding packet loss resistance
  - Supports WHIP/WHEP protocols
- [SRT support](./srt/srt.md)
- Others
  - Supports rich RESTful APIs and webhook events
  - Supports simple Telnet debugging
  - Supports hot reloading of configuration files
  - Supports traffic statistics, stream authentication, and other events
  - Supports virtual hosts for isolating different domain names
  - Supports on-demand streaming and automatic shutdown of streams with no viewers
  - Supports pre-play before streaming to increase the rate of timely stream openings
  - Provides a complete and powerful C API SDK
  - Supports FFmpeg stream proxy for any format
  - Supports HTTP API for real-time screenshot generation and return
  - Supports on-demand demultiplexing and protocol conversion, reducing CPU usage by only enabling it when someone is watching
  - Supports cluster deployment in traceable mode, with RTSP/RTMP/HLS/HTTP-TS support for traceable mode and HLS support for edge stations and multiple sources for source stations (using round-robin tracing)
  - Can reconnect to streaming after abnormal disconnection in RTSP/RTMP/WebRTC pushing within a timeout period, with no impact on the player.
 
## System Requirements

- Compiler with c++11 support, such as GCC 4.8+, Clang 3.3+, or VC2015+.
- CMake 3.1+.
- Linux (32-bit and 64-bit).
- Apple macOS (32-bit and 64-bit).
- Any hardware with x86, x86_64, ARM, or MIPS CPU.
- Windows.

## How to build

It is recommended to compile on Ubuntu or macOS. Compiling on Windows is cumbersome, and some features are not compiled by default.

### Before Building

- **You must use Git to clone the complete code. Do not download the source code by downloading the ZIP package. Otherwise, the submodule code will not be downloaded by default. You can do it like this:**
```
git clone https://github.com/ZLMediaKit/ZLMediaKit.git
cd ZLMediaKit
git submodule update --init
```

### Building on Linux

- My Environment
  - Ubuntu 16.04 (64-bit) with GCC 5.4.
  - CMake 3.5.1.
- Guidance
  
  ```
	# If it is on CentOS 6.x, you need to install a newer version of GCC and CMake first,
	# and then compile manually according to the "build_for_linux.sh" script.
	# If it is on a newer version of a system such as Ubuntu or Debian,
	# step 4 can be manipulated directly.
	
	# 1. Install GCC 5.2 (this step can be skipped if the GCC version is higher than 4.7).
	sudo yum install centos-release-scl -y
	sudo yum install devtoolset-4-toolchain -y
	scl enable devtoolset-4 bash
	
	# 2. Install CMake (this step can be skipped if the CMake version is higher than 3.1).
	tar -xvf cmake-3.10.0-rc4.tar.gz #you need to download the CMake source file manually
	cd cmake-3.10.0-rc4
	./configure
	make -j4
	sudo make install
	
	# 3. Switch to a higher version of GCC.
	scl enable devtoolset-4 bash
	
	# 4. Build.
	cd ZLMediaKit
	./build_for_linux.sh
  ```

### Building on macOS

- My Environment
  - macOS Sierra (10.12.1) with Xcode 8.3.1.
  - Homebrew 1.1.3.
  - CMake 3.8.0.
- Guidance
  
  ```
  cd ZLMediaKit
  ./build_for_mac.sh
  ```

### Building on iOS
- You can generate Xcode projects and recompile them , [learn more](https://github.com/leetal/ios-cmake):

  ```
  cd ZLMediaKit
  mkdir -p build
  cd build
  # Generate Xcode project, project file is in build directory
  cmake .. -G Xcode -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake  -DPLATFORM=OS64COMBINED
  ```
  

### Building on Android

  Now you can open the Android Studio project in the `Android` folder. This is an `AAR` library and demo project.

- My environment
  - macOS Sierra (10.12.1) + Xcode 8.3.1
  - Homebrew 1.1.3
  - CMake 3.8.0
  - [Android NDK r14b](https://dl.google.com/android/repository/android-ndk-r14b-darwin-x86_64.zip)
  
- Guidance 

  ```
  cd ZLMediaKit
  export ANDROID_NDK_ROOT=/path/to/ndk
  ./build_for_android.sh
  ```
  
### Building on Windows

- My environment
  - Windows 10
  - Visual Studio 2017
  - [CMake GUI](https://cmake.org/files/v3.10/cmake-3.10.0-rc1-win32-x86.msi)
  
- Guidance
```
1. Enter the ZLMediaKit directory and execute `git submodule update --init` to download the code for ZLToolKit.
2. Open the project with CMake GUI and generate the Visual Studio project file.
3. Find the project file (ZLMediaKit.sln), double-click to open it with VS2017.
4. Choose to compile the Release version. Find the target file and run the test cases.
```

## Usage

- As a server：
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

- As a player：
	```cpp
    MediaPlayer::Ptr player(new MediaPlayer());
    weak_ptr<MediaPlayer> weakPlayer = player;
    player->setOnPlayResult([weakPlayer](const SockException &ex) {
        InfoL << "OnPlayResult:" << ex.what();
        auto strongPlayer = weakPlayer.lock();
        if (ex || !strongPlayer) {
            return;
        }

        auto videoTrack = strongPlayer->getTrack(TrackVideo);
        if (!videoTrack) {
            WarnL << "No video Track!";
            return;
        }
        videoTrack->addDelegate([](const Frame::Ptr &frame) {
            //please decode video here
        });
    });

    player->setOnShutdown([](const SockException &ex) {
        ErrorL << "OnShutdown:" << ex.what();
    });

    //RTP transport over TCP
    (*player)[Client::kRtpType] = Rtsp::RTP_TCP;
    player->play("rtsp://admin:jzan123456@192.168.0.122/");
	```
- As a proxy server：
	```cpp
	//Support RTMP and RTSP URLs, but only H264 + AAC codec is supported
	auto urlList = {"rtmp://live.hkstv.hk.lxdns.com/live/hks",
			"rtsp://184.72.239.149/vod/mp4://BigBuckBunny_175k.mov"};
	map<string , PlayerProxy::Ptr> proxyMap;
	int i=0;
	for(auto url : urlList){
		PlayerProxy::Ptr player(new PlayerProxy("live",to_string(i++).data()));
		player->play(url);
		proxyMap.emplace(string(url),player);
	}
	```
	
- As a pusher：
	```cpp
	PlayerProxy::Ptr player(new PlayerProxy("app","stream"));
	player->play("rtmp://live.hkstv.hk.lxdns.com/live/hks");
	
	RtmpPusher::Ptr pusher;
	NoticeCenter::Instance().addListener(nullptr,Config::Broadcast::kBroadcastRtmpSrcRegisted,
			[&pusher](BroadcastRtmpSrcRegistedArgs){
		const_cast<RtmpPusher::Ptr &>(pusher).reset(new RtmpPusher(app,stream));
		pusher->publish("rtmp://jizan.iok.la/live/test");
	});
	
	```

## Binary file download
zlmediakit uses github action to continuously integrate automatic compilation package and upload the compilation output package. Please download the latest sdk library file and executable file at [issue list] (https://github.com/ZLMediaKit/ZLMediaKit/issues/483).

## Docker Image

You can download the pre-compiled image from Docker Hub and start it:

```bash
#This image is pushed by the GitHub continuous integration automatic compilation to keep up with the latest code (master branch)
docker run -id -p 1935:1935 -p 8080:80 -p 8443:443 -p 8554:554 -p 10000:10000 -p 10000:10000/udp -p 8000:8000/udp -p 9000:9000/udp zlmediakit/zlmediakit:master
```

You can also compile the image based on the Dockerfile:

```bash
bash build_docker_images.sh
```

## Collaborative Projects

- Visual management website
   - [A backend management website for this project](https://github.com/1002victor/zlm_webassist)
   - [The latest web project with front-end and back-end separation, supporting webrtc playback](https://github.com/langmansh/AKStreamNVR)
   - [Management web site based on ZLMediaKit master branch](https://gitee.com/kkkkk5G/MediaServerUI) 
   - [Management web site based on ZLMediaKit branch](https://github.com/chenxiaolei/ZLMediaKit_NVR_UI)
   - [A very beautiful visual background management system](https://github.com/MingZhuLiu/ZLMediaServerManagent)
    
- Media management platform
  - [GB28181 complete solution with web management website, supporting webrtc and h265 playback](https://github.com/648540858/wvp-GB28181-pro)
  - [Powerful media control and management interface platform, supporting GB28181](https://github.com/chatop2020/AKStream)
  - [GB28181 server implemented in C++](https://github.com/any12345com/BXC_SipServer)
  - [GB28181 server implemented in Go](https://github.com/panjjo/gosip)
  - [Node-js version of GB28181 platform](https://gitee.com/hfwudao/GB28181_Node_Http)
  - [Hikvision ehome server implemented in Go](https://github.com/tsingeye/FreeEhome)

- Client
  - [Http Api and hook in zlm-spring-boot-starter](https://github.com/lunasaw/zlm-spring-boot-starter)
  - [Complete java wrapper library for c sdk](https://github.com/lidaofu-hub/j_zlm_sdk)
  - [Complete C# wrapper library for c sdk](https://github.com/malegend/ZLMediaKit.Autogen) 
  - [Push client implemented based on C SDK](https://github.com/hctym1995/ZLM_ApiDemo)
  - [Http API and Hook in C#](https://github.com/chengxiaosheng/ZLMediaKit.HttpApi)
  - [RESTful client in DotNetCore](https://github.com/MingZhuLiu/ZLMediaKit.DotNetCore.Sdk)
   
- Player
  - [Player supporting H265 based on wasm](https://github.com/numberwolf/h265web.js)
  - [WebSocket-fmp4 player based on MSE](https://github.com/v354412101/wsPlayer) 
  - [Domestic webrtc sdk(metaRTC)](https://github.com/metartc/metaRTC)
  - [GB28181 player implemented in C++](https://github.com/any12345com/BXC_gb28181Player)

## License

The self-owned code of this project is licensed under the permissive MIT License and can be freely applied to commercial and non-commercial projects while retaining copyright information.
However, this project also uses some scattered [open source code](https://github.com/ZLMediaKit/ZLMediaKit/wiki/%E4%BB%A3%E7%A0%81%E4%BE%9D%E8%B5%96%E4%B8%8E%E7%89%88%E6%9D%83%E5%A3%B0%E6%98%8E) , please replace or remove it for commercial use.
Any commercial disputes or infringement caused by using this project have nothing to do with the project and developers and shall be at your own legal risk.
When using the code of this project, the license agreement should also indicate the license of the third-party libraries that this project depends on.

## Contact Information

- Email: <1213642868@qq.com> (For project-related or streaming media-related questions, please follow the issue process. Otherwise, we will not reply to emails.)
- QQ groups: Both QQ groups with a total of 4000 members are full. We will not create new QQ groups in the future. Users can join the [Knowledge Planet](https://github.com/ZLMediaKit/ZLMediaKit/issues/2364) to ask questions and support this project.
- Follow WeChat Official Account:
<img src=https://user-images.githubusercontent.com/11495632/232451702-4c50bc72-84d8-4c94-af2b-57290088ba7a.png width=15% />

## How to Ask Questions?

If you have any questions about the project, we recommend that you:

- 1. Carefully read the readme and wiki. If necessary, you can also check the issues.
- 2. If your question has not been resolved, you can raise an issue.
- 3. Some questions may not be suitable for issues, but can be raised in QQ groups.
- 4. We generally do not accept free technical consulting and support via QQ private chat. ([Why we don't encourage QQ private chat](https://github.com/ZLMediaKit/ZLMediaKit/wiki/%E4%B8%BA%E4%BB%80%E4%B9%88%E4%B8%8D%E5%BB%BA%E8%AE%AEQQ%E7%A7%81%E8%81%8A%E5%92%A8%E8%AF%A2%E9%97%AE%E9%A2%98%EF%BC%9F)).
- 5. If you need more timely and thoughtful technical support, you can join the [Knowledge Planet](https://github.com/ZLMediaKit/ZLMediaKit/issues/2364) for a fee.

## Special Thanks

This project uses the [media-server](https://github.com/ireader/media-server) library developed by [Lao Chen](https://github.com/ireader). The reuse and de-multiplexing of ts/fmp4/mp4/ps container formats in this project depend on the media-server library. Lao Chen has provided invaluable help and support multiple times in implementing many functions of this project, and we would like to express our sincere gratitude to him!

## Acknowledgments

Thanks to all those who have supported this project in various ways, including but not limited to code contributions, problem feedback, and donations. The following list is not in any particular order:

[老陈](https://github.com/ireader)
[Gemfield](https://github.com/gemfield)
[南冠彤](https://github.com/nanguantong2)
[凹凸慢](https://github.com/tsingeye)
[chenxiaolei](https://github.com/chenxiaolei)
[史前小虫](https://github.com/zqsong)
[清涩绿茶](https://github.com/baiyfcu)
[3503207480](https://github.com/3503207480)
[DroidChow](https://github.com/DroidChow)
[阿塞](https://github.com/HuoQiShuai)
[火宣](https://github.com/ChinaCCF)
[γ瑞γミ](https://github.com/JerryLinGd)
[linkingvision](https://www.linkingvision.com/)
[茄子](https://github.com/taotaobujue2008)
[好心情](mailto:409257224@qq.com)
[浮沉](https://github.com/MingZhuLiu)
[Xiaofeng Wang](https://github.com/wasphin)
[doodoocoder](https://github.com/doodoocoder)
[qingci](https://github.com/Colibrow)
[swwheihei](https://github.com/swwheihei)
[KKKKK5G](https://gitee.com/kkkkk5G)
[Zhou Weimin](mailto:zhouweimin@supremind.com)
[Jim Jin](https://github.com/jim-king-2000)
[西瓜丶](mailto:392293307@qq.com)
[MingZhuLiu](https://github.com/MingZhuLiu)
[chengxiaosheng](https://github.com/chengxiaosheng)
[big panda](mailto:2381267071@qq.com)
[tanningzhong](https://github.com/tanningzhong)
[hctym1995](https://github.com/hctym1995)
[hewenyuan](https://gitee.com/kingyuanyuan)
[sunhui](mailto:sunhui200475@163.com)
[mirs](mailto:fangpengcheng@bilibili.com)
[Kevin Cheng](mailto:kevin__cheng@outlook.com)
[Liu Jiang](mailto:root@oopy.org)
[along](https://github.com/alongl)
[qingci](mailto:xpy66swsry@gmail.com)
[lyg1949](mailto:zh.ghlong@qq.com)
[zhlong](mailto:zh.ghlong@qq.com)
[大裤衩](mailto:3503207480@qq.com)
[droid.chow](mailto:droid.chow@gmail.com)
[陈晓林](https://github.com/musicwood)
[CharleyWangHZ](https://github.com/CharleyWangHZ)
[Johnny](https://github.com/johzzy)
[DoubleX69](https://github.com/DoubleX69)
[lawrencehj](https://github.com/lawrencehj)
[yangkun](mailto:xyyangkun@163.com)
[Xinghua Zhao](mailto:holychaossword@hotmail.com)
[hejilin](https://github.com/brokensword2018)
[rqb500](https://github.com/rqb500)
[Alex](https://github.com/alexliyu7352)
[Dw9](https://github.com/Dw9)
[明月惊鹊](mailto:mingyuejingque@gmail.com)
[cgm](mailto:2958580318@qq.com)
[hejilin](mailto:1724010622@qq.com)
[alexliyu7352](mailto:liyu7352@gmail.com)
[cgm](mailto:2958580318@qq.com)
[haorui wang](https://github.com/HaoruiWang)
[joshuafc](mailto:joshuafc@foxmail.com)
[JayChen0519](https://github.com/JayChen0519)
[zx](mailto:zuoxue@qq.com)
[wangcker](mailto:wangcker@163.com)
[WuPeng](mailto:wp@zafu.edu.cn)
[starry](https://github.com/starry)
[mtdxc](https://github.com/mtdxc)
[胡刚风](https://github.com/hugangfeng333)
[zhao85](https://github.com/zhao85)
[dreamisdream](https://github.com/dreamisdream)
[dingcan](https://github.com/dcan123)
[Haibo Chen](https://github.com/duiniuluantanqin)
[Leon](https://gitee.com/leon14631)
[custompal](https://github.com/custompal)
[PioLing](https://github.com/PioLing)
[KevinZang](https://github.com/ZSC714725)
[gongluck](https://github.com/gongluck)
[a-ucontrol](https://github.com/a-ucontrol)
[TalusL](https://github.com/TalusL)
[ahaooahaz](https://github.com/AHAOAHA)
[TempoTian](https://github.com/TempoTian)
[Derek Liu](https://github.com/yjkhtddx)
[ljx0305](https://github.com/ljx0305)
[朱如洪 ](https://github.com/zhu410289616)
[lijin](https://github.com/1461521844lijin)
[PioLing](https://github.com/PioLing)
[BackT0TheFuture](https://github.com/BackT0TheFuture)
[perara](https://github.com/perara)
[codeRATny](https://github.com/codeRATny)
[dengjfzh](https://github.com/dengjfzh)
[百鸣](https://github.com/ixingqiao)
[fruit Juice](https://github.com/xuandu)
[tbago](https://github.com/tbago)
[Luosh](https://github.com/Luosh)
[linxiaoyan87](https://github.com/linxiaoyan)
[waken](https://github.com/mc373906408)
[Deepslient](https://github.com/Deepslient)
[imp_rayjay](https://github.com/rayjay214)
[ArmstrongCN](https://github.com/ArmstrongCN)
[leibnewton](https://github.com/leibnewton)
[1002victor](https://github.com/1002victor)
[Grin](https://github.com/xyyangkun)
[xbpeng121](https://github.com/xbpeng121)
[lvchenyun](https://github.com/lvchenyun)
[Fummowo](https://github.com/Fummowo)
[Jovial Young ](https://github.com/JHYoung1034)
[yujitai](https://github.com/yujitai)
[KisChang](https://github.com/kisChang)
[zjx94](https://github.com/zjx94)
[LeiZhi.Mai ](https://github.com/blueskiner)
[JiaHao](https://github.com/nashiracn)
[chdahuzi](https://github.com/chdahuzi)
[snysmtx](https://github.com/snysmtx)
[SetoKaiba](https://github.com/SetoKaiba)
[sandro-qiang](https://github.com/sandro-qiang)
[Paul Philippov](https://github.com/themactep)
[张传峰](https://github.com/zhang-chuanfeng)
[lidaofu-hub](https://github.com/lidaofu-hub)
[huangcaichun](https://github.com/huangcaichun)
[jamesZHANG500](https://github.com/jamesZHANG500)
[weidelong](https://github.com/wdl1697454803)
[小强先生](https://github.com/linshangqiang)

Also thank to JetBrains for their support for open source project, we developed and debugged zlmediakit with CLion:

[![JetBrains](https://resources.jetbrains.com/storage/products/company/brand/logos/CLion.svg)](https://jb.gg/OpenSourceSupport)

## Use Cases

This project has gained recognition from many companies and individual developers. According to the author's incomplete statistics, companies using this project include well-known Internet giants, leading cloud service companies in China, several well-known AI unicorn companies, as well as a series of small and medium-sized companies. Users can endorse this project by pasting their company name and relevant project information on the [issue page](https://github.com/ZLMediaKit/ZLMediaKit/issues/511). Thank you for your support!
