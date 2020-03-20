![logo](https://raw.githubusercontent.com/zlmediakit/ZLMediaKit/master/logo.png)

[english readme](https://github.com/xiongziliang/ZLMediaKit/blob/master/README_en.md)

# 一个基于C++11的高性能运营级流媒体服务框架
 [![Build Status](https://travis-ci.org/xiongziliang/ZLMediaKit.svg?branch=master)](https://travis-ci.org/xiongziliang/ZLMediaKit)


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
  - RTSP 服务器，支持RTMP/MP4转RTSP
  - RTSPS 服务器，支持亚马逊echo show这样的设备
  - RTSP 播放器，支持RTSP代理，支持生成静音音频
  - RTSP 推流客户端与服务器
  - 支持 `rtp over udp` `rtp over tcp` `rtp over http` `rtp组播`  四种RTP传输方式 
  - 服务器/客户端完整支持Basic/Digest方式的登录鉴权，全异步可配置化的鉴权接口
  - 支持H265编码
  - 服务器支持RTSP推流(包括`rtp over udp` `rtp over tcp`方式)
  - 支持任意编码格式的rtsp推流，只是除H264/H265+AAC外无法转协议

- RTMP
  - RTMP 播放服务器，支持RTSP/MP4转RTMP
  - RTMP 发布服务器，支持录制发布流
  - RTMP 播放器，支持RTMP代理，支持生成静音音频
  - RTMP 推流客户端
  - 支持http[s]-flv直播
  - 支持websocket-flv直播
  - 支持任意编码格式的rtmp推流，只是除H264/H265+AAC外无法转协议

- HLS
  - 支持HLS文件生成，自带HTTP文件服务器
  - 通过cookie追踪技术，可以模拟HLS播放为长连接，实现丰富的业务逻辑
  - 支持完备的HLS用户追踪、播放统计等业务功能，可以实现HLS按需拉流等业务

- HTTP[S]
  - 服务器支持`目录索引生成`,`文件下载`,`表单提交请求`
  - 客户端提供`文件下载器(支持断点续传)`,`接口请求器`,`文件上传器`
  - 完整HTTP API服务器，可以作为web后台开发框架
  - 支持跨域访问
  - 支持http客户端、服务器cookie
  - 支持WebSocket服务器和客户端
  - 支持http文件访问鉴权

- GB28181
  - 支持UDP/TCP国标RTP(PS或TS)推流，可以转换成RTSP/RTMP/HLS等协议
 
- 点播
  - 支持录制为FLV/HLS/MP4
  - RTSP/RTMP/HTTP-FLV/WS-FLV支持MP4文件点播，支持seek
 
- 其他
  - 支持丰富的restful api以及web hook事件 
  - 支持简单的telnet调试
  - 支持配置文件热加载
  - 支持流量统计、推拉流鉴权等事件
  - 支持虚拟主机,可以隔离不同域名
  - 支持按需拉流，无人观看自动关断拉流
  - 支持先拉流后推流，提高及时推流画面打开率
  - 提供c api sdk
 


## 细节列表

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

## 编译以及测试
请参考wiki:[快速开始](https://github.com/xiongziliang/ZLMediaKit/wiki/%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)

## Docker 镜像
你可以从Docker Hub下载已经编译好的镜像并启动它：
```bash
docker run -id -p 1935:1935 -p 8080:80 gemfield/zlmediakit
```
你要可以在Ubuntu 16.04下根据Dockerfile编译镜像：
```bash
cd docker
docker build -t zlmediakit .
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

## 参考案例
 - [IOS摄像头实时录制,生成rtsp/rtmp/hls/http-flv](https://gitee.com/xiahcu/IOSMedia)
 - [IOS rtmp/rtsp播放器，视频推流器](https://gitee.com/xiahcu/IOSPlayer)
 - [支持linux、windows、mac的rtmp/rtsp播放器](https://github.com/xiongziliang/ZLMediaPlayer)
 - [配套的管理WEB网站](https://github.com/chenxiaolei/ZLMediaKit_NVR_UI)
 
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
 
## 致谢
感谢以下各位对本项目包括但不限于代码贡献、问题反馈、资金捐赠等各种方式的支持！以下排名不分先后：

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
[好心情](<409257224@qq.com>)

## 捐赠
欢迎捐赠以便更好的推动项目的发展，谢谢您的支持!

[支付宝](https://raw.githubusercontent.com/xiongziliang/other/master/IMG_3919.JPG)

[微信](https://raw.githubusercontent.com/xiongziliang/other/master/IMG_3920.JPG)



