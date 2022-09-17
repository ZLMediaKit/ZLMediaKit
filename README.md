![logo](https://raw.githubusercontent.com/ZLMediaKit/ZLMediaKit/master/www/logo.png)

# 一个基于C++11的高性能运营级流媒体服务框架

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

## 项目特点

- 基于C++11开发，避免使用裸指针，代码稳定可靠，性能优越。
- 支持多种协议(RTSP/RTMP/HLS/HTTP-FLV/WebSocket-FLV/GB28181/HTTP-TS/WebSocket-TS/HTTP-fMP4/WebSocket-fMP4/MP4/WebRTC),支持协议互转。
- 使用多路复用/多线程/异步网络IO模式开发，并发性能优越，支持海量客户端连接。
- 代码经过长期大量的稳定性、性能测试，已经在线上商用验证已久。
- 支持linux、macos、ios、android、windows全平台。
- 支持画面秒开、极低延时([500毫秒内，最低可达100毫秒](https://github.com/ZLMediaKit/ZLMediaKit/wiki/%E5%BB%B6%E6%97%B6%E6%B5%8B%E8%AF%95))。
- 提供完善的标准[C API](https://github.com/ZLMediaKit/ZLMediaKit/tree/master/api/include),可以作SDK用，或供其他语言调用。
- 提供完整的[MediaServer](https://github.com/ZLMediaKit/ZLMediaKit/tree/master/server)服务器，可以免开发直接部署为商用服务器。
- 提供完善的[restful api](https://github.com/ZLMediaKit/ZLMediaKit/wiki/MediaServer%E6%94%AF%E6%8C%81%E7%9A%84HTTP-API)以及[web hook](https://github.com/ZLMediaKit/ZLMediaKit/wiki/MediaServer%E6%94%AF%E6%8C%81%E7%9A%84HTTP-HOOK-API)，支持丰富的业务逻辑。
- 打通了视频监控协议栈与直播协议栈，对RTSP/RTMP支持都很完善。
- 全面支持H265/H264/AAC/G711/OPUS。
- 功能完善，支持集群、按需转协议、按需推拉流、先播后推、断连续推等功能。
- 极致性能，单机10W级别播放器，100Gb/s级别io带宽能力。
- 极致体验，[独家特性](https://github.com/ZLMediaKit/ZLMediaKit/wiki/ZLMediakit%E7%8B%AC%E5%AE%B6%E7%89%B9%E6%80%A7%E4%BB%8B%E7%BB%8D)
- [谁在使用zlmediakit?](https://github.com/ZLMediaKit/ZLMediaKit/issues/511)
- 全面支持ipv6网络

## 项目定位

- 移动嵌入式跨平台流媒体解决方案。
- 商用级流媒体服务器。
- 网络编程二次开发SDK。


## 功能清单
### 功能一览
<img width="800" alt="功能一览" src="https://user-images.githubusercontent.com/11495632/190864440-91c45f8f-480f-43db-8110-5bb44e6300ff.png">

- RTSP[S]
  - RTSP[S] 服务器，支持RTMP/MP4/HLS转RTSP[S],支持亚马逊echo show这样的设备
  - RTSP[S] 播放器，支持RTSP代理，支持生成静音音频
  - RTSP[S] 推流客户端与服务器
  - 支持 `rtp over udp` `rtp over tcp` `rtp over http` `rtp组播`  四种RTP传输方式 
  - 服务器/客户端完整支持Basic/Digest方式的登录鉴权，全异步可配置化的鉴权接口
  - 支持H265编码
  - 服务器支持RTSP推流(包括`rtp over udp` `rtp over tcp`方式)
  - 支持H264/H265/AAC/G711/OPUS编码，其他编码能转发但不能转协议

- RTMP[S]
  - RTMP[S] 播放服务器，支持RTSP/MP4/HLS转RTMP
  - RTMP[S] 发布服务器，支持录制发布流
  - RTMP[S] 播放器，支持RTMP代理，支持生成静音音频
  - RTMP[S] 推流客户端
  - 支持http[s]-flv直播
  - 支持websocket-flv直播
  - 支持H264/H265/AAC/G711/OPUS编码，其他编码能转发但不能转协议
  - 支持[RTMP-H265](https://github.com/ksvc/FFmpeg/wiki)
  - 支持[RTMP-OPUS](https://github.com/ZLMediaKit/ZLMediaKit/wiki/RTMP%E5%AF%B9H265%E5%92%8COPUS%E7%9A%84%E6%94%AF%E6%8C%81)

- HLS
  - 支持HLS文件生成，自带HTTP文件服务器
  - 通过cookie追踪技术，可以模拟HLS播放为长连接，可以实现HLS按需拉流、播放统计等业务
  - 支持HLS播发器，支持拉流HLS转rtsp/rtmp/mp4
  - 支持H264/H265/AAC/G711/OPUS编码
  
- TS
  - 支持http[s]-ts直播
  - 支持ws[s]-ts直播
  - 支持H264/H265/AAC/G711/OPUS编码
  
- fMP4
  - 支持http[s]-fmp4直播
  - 支持ws[s]-fmp4直播
  - 支持H264/H265/AAC/G711/OPUS编码

- HTTP[S]与WebSocket
  - 服务器支持`目录索引生成`,`文件下载`,`表单提交请求`
  - 客户端提供`文件下载器(支持断点续传)`,`接口请求器`,`文件上传器`
  - 完整HTTP API服务器，可以作为web后台开发框架
  - 支持跨域访问
  - 支持http客户端、服务器cookie
  - 支持WebSocket服务器和客户端
  - 支持http文件访问鉴权

- GB28181与RTP推流
  - 支持UDP/TCP国标RTP(PS或TS)推流服务器，可以转换成RTSP/RTMP/HLS等协议
  - 支持RTSP/RTMP/HLS转国标推流客户端，支持TCP/UDP模式，提供相应restful api
  - 支持H264/H265/AAC/G711/OPUS编码
  - 支持海康ehome推流

- MP4点播与录制
  - 支持录制为FLV/HLS/MP4
  - RTSP/RTMP/HTTP-FLV/WS-FLV支持MP4文件点播，支持seek
  - 支持H264/H265/AAC/G711/OPUS编码
  
- WebRTC
  - 支持WebRTC推流，支持转其他协议
  - 支持WebRTC播放，支持其他协议转WebRTC
  - 支持双向echo test     
  - 支持simulcast推流
  - 支持上下行rtx/nack丢包重传
  - **支持单端口、多线程、客户端网络连接迁移(开源界唯一)**。
  - 支持TWCC rtcp动态调整码率
  - 支持remb/pli/sr/rr rtcp
  - 支持rtp扩展解析
  - 支持GOP缓冲，webrtc播放秒开
  - 支持datachannel
- [SRT支持](./srt/srt.md)
- 其他
  - 支持丰富的restful api以及web hook事件 
  - 支持简单的telnet调试
  - 支持配置文件热加载
  - 支持流量统计、推拉流鉴权等事件
  - 支持虚拟主机,可以隔离不同域名
  - 支持按需拉流，无人观看自动关断拉流
  - 支持先播放后推流，提高及时推流画面打开率
  - 提供c api sdk
  - 支持FFmpeg拉流代理任意格式的流
  - 支持http api生成并返回实时截图
  - 支持按需解复用、转协议，当有人观看时才开启转协议，降低cpu占用率
  - 支持溯源模式的集群部署，溯源方式支持rtsp/rtmp/hls/http-ts, 边沿站支持hls, 源站支持多个(采用round robin方式溯源)
  - rtsp/rtmp/webrtc推流异常断开后，可以在超时时间内重连推流，播放器无感知
  

## 编译以及测试
**编译前务必仔细参考wiki:[快速开始](https://github.com/ZLMediaKit/ZLMediaKit/wiki/%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)操作!!!**

## 怎么使用

 你有三种方法使用ZLMediaKit，分别是：

 - 1、使用c api，作为sdk使用，请参考[这里](https://github.com/ZLMediaKit/ZLMediaKit/tree/master/api/include).
 - 2、作为独立的流媒体服务器使用，不想做c/c++开发的，可以参考 [restful api](https://github.com/ZLMediaKit/ZLMediaKit/wiki/MediaServer支持的HTTP-API) 和 [web hook](https://github.com/ZLMediaKit/ZLMediaKit/wiki/MediaServer支持的HTTP-HOOK-API ).
 - 3、如果想做c/c++开发，添加业务逻辑增加功能，可以参考这里的[测试程序](https://github.com/ZLMediaKit/ZLMediaKit/tree/master/tests).

## Docker 镜像

你可以从Docker Hub下载已经编译好的镜像并启动它：

```bash
#此镜像为github持续集成自动编译推送，跟代码(master分支)保持最新状态
docker run -id -p 1935:1935 -p 8080:80 -p 8443:443 -p 8554:554 -p 10000:10000 -p 10000:10000/udp -p 8000:8000/udp -p 9000:9000/udp zlmediakit/zlmediakit:master
```

你也可以根据Dockerfile编译镜像：

```bash
bash build_docker_images.sh
```

## 合作项目

 - 可视化管理网站
    - [最新的前后端分离web项目,支持webrtc播放](https://github.com/langmansh/AKStreamNVR)
    - [基于ZLMediaKit主线的管理WEB网站](https://gitee.com/kkkkk5G/MediaServerUI) 
    - [基于ZLMediaKit分支的管理WEB网站](https://github.com/chenxiaolei/ZLMediaKit_NVR_UI)
    - [一个非常漂亮的可视化后台管理系统](https://github.com/MingZhuLiu/ZLMediaServerManagent)
    
 - 流媒体管理平台
   - [GB28181完整解决方案,自带web管理网站,支持webrtc、h265播放](https://github.com/648540858/wvp-GB28181-pro)
   - [功能强大的流媒体控制管理接口平台,支持GB28181](https://github.com/chatop2020/AKStream)
   - [Go实现的GB28181服务器](https://github.com/panjjo/gosip)
   - [node-js版本的GB28181平台](https://gitee.com/hfwudao/GB28181_Node_Http)
   - [Go实现的海康ehome服务器](https://github.com/tsingeye/FreeEhome)

 - 客户端
   - [c sdk完整c#包装库](https://github.com/malegend/ZLMediaKit.Autogen) 
   - [基于C SDK实现的推流客户端](https://github.com/hctym1995/ZLM_ApiDemo)
   - [C#版本的Http API与Hook](https://github.com/chengxiaosheng/ZLMediaKit.HttpApi)
   - [DotNetCore的RESTful客户端](https://github.com/MingZhuLiu/ZLMediaKit.DotNetCore.Sdk)
   
 - 播放器
   - [基于wasm支持H265的播放器](https://github.com/numberwolf/h265web.js)
   - [基于MSE的websocket-fmp4播放器](https://github.com/v354412101/wsPlayer) 
   - [全国产webrtc sdk(metaRTC)](https://github.com/metartc/metaRTC)
   
## 授权协议

本项目自有代码使用宽松的MIT协议，在保留版权信息的情况下可以自由应用于各自商用、非商业的项目。
但是本项目也零碎的使用了一些其他的[开源代码](https://github.com/ZLMediaKit/ZLMediaKit/wiki/%E4%BB%A3%E7%A0%81%E4%BE%9D%E8%B5%96%E4%B8%8E%E7%89%88%E6%9D%83%E5%A3%B0%E6%98%8E)，在商用的情况下请自行替代或剔除；
由于使用本项目而产生的商业纠纷或侵权行为一概与本项目及开发者无关，请自行承担法律风险。
在使用本项目代码时，也应该在授权协议中同时表明本项目依赖的第三方库的协议。

## 联系方式

 - 邮箱：<1213642868@qq.com>(本项目相关或流媒体相关问题请走issue流程，否则恕不邮件答复)
 - QQ群：qq群号在wiki中，请阅读wiki后再加群

## 怎么提问？

如果要对项目有相关疑问，建议您这么做：

 - 1、仔细看下readme、wiki，如果有必要可以查看下issue.
 - 2、如果您的问题还没解决，可以提issue.
 - 3、有些问题，如果不具备参考性的，无需在issue提的，可以在qq群提.
 - 4、QQ私聊一般不接受无偿技术咨询和支持([为什么不提倡QQ私聊](https://github.com/ZLMediaKit/ZLMediaKit/wiki/%E4%B8%BA%E4%BB%80%E4%B9%88%E4%B8%8D%E5%BB%BA%E8%AE%AEQQ%E7%A7%81%E8%81%8A%E5%92%A8%E8%AF%A2%E9%97%AE%E9%A2%98%EF%BC%9F)).

## 特别感谢

本项目采用了[老陈](https://github.com/ireader) 的 [media-server](https://github.com/ireader/media-server) 库，
本项目的 ts/fmp4/mp4/ps 容器格式的复用解复用都依赖media-server库。在实现本项目诸多功能时，老陈多次给予了无私热情关键的帮助，
特此对他表示诚挚的感谢！

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

## 使用案例

本项目已经得到不少公司和个人开发者的认可，据作者不完全统计，
使用本项目的公司包括知名的互联网巨头、国内排名前列的云服务公司、多家知名的AI独角兽公司，
以及一系列中小型公司。使用者可以通过在 [issue](https://github.com/ZLMediaKit/ZLMediaKit/issues/511) 上粘贴公司的大名和相关项目介绍为本项目背书，感谢支持！
