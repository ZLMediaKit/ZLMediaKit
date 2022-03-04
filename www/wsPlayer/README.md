# ![Logo](wsPlayerlogo.svg) wsPlayer

​       wsPlayer是一款专注于WebSocket-fmp4协议的web视频播放器，HTTP/WebSocket-fmp4协议与RTMP、HLS、HTTP-FLV相比，具有播放延时短，HTML5兼容性好等优点；

见各流媒体协议对比：


|    协议名称    | 网络传输协议 | 延时  |        编码类型        |         HTML5支持情况          |
| :------------: | :----------: | :---: | :--------------------: | :----------------------------: |
|      RTSP      | TCP/UDP/组播 | 0~3s  |       H264/H265        | 不支持，（RTSP over HTTP除外） |
|      RTMP      |     TCP      | 0~3s  | H264/H265(CodecID =12) |             不支持             |
|      HLS       |  HTTP短连接  | 1~10s |       H264/H265        |         video标签支持          |
|    HTTP-FLV    |  HTTP长连接  | 0~3s  | H264/H265(CodecID =12) |     flv → fmp4 → video标签     |
|   HTTP-fmp4    |  HTTP长连接  | 0~3s  |       H264/H265        |       video标签原生支持        |
| WebSocket-FLV  |  WebSocket   | 0~3s  | H264/H265(CodecID =12) |     flv → fmp4 → video标签     |
| WebSocket-fmp4 |  WebSocket   | 0~3s  |       H264/H265        |     使用MSE，vidoe标签播放     |

备注：浏览器对单个页面的HTTP长连接的并发数限制为6个，这意味着HTTP-FLV、HTTP-fmp4只能同时播放6个视频画面；但浏览器对WebSocket的连接数没有限制；



## 项目依赖

需要使用[mp4box.js](https://github.com/gpac/mp4box.js)来解析fmp4 moov中的codecs；



## 快速开始

推荐使用[ZLMediaKit](https://github.com/ZLMediaKit/ZLMediaKit)作为WebSocket-fmp4协议的后端流媒体服务器

1. 部署后端流媒体服务器

```shell
docker pull panjjo/zlmediakit
docker run -id -p 8080:80 -p 554:554 panjjo/zlmediakit
```

2. 使用ffmpeg命令，向ZLMediaKit添加一路RTSP推流
```shell
ffmpeg -re -stream_loop -1 -i test.mp4 -an -vcodec copy -f rtsp -rtsp_transport tcp rtsp://100.100.154.29/live/test
```

3. 根据ZLMediaKit的[播放url规则](https://github.com/zlmediakit/ZLMediaKit/wiki/%E6%92%AD%E6%94%BEurl%E8%A7%84%E5%88%99)得知，WebSocket-fmp4协议的URL格式为：
```shell
ws://100.100.154.29:8080/live/test.live.mp4
```

4. 然后调用播放器代码：

```html
<html>
<head>
</head>
<body>
    <script type="text/javascript" src="mp4box.all.min.js"></script>
    <script type="text/javascript" src="wsPlayer.js"></script>
    <video muted autoplay id="video"></video>
    <script>
        document.addEventListener('DOMContentLoaded', function () {
            var player = new wsPlayer("video", "ws://100.100.154.29:8083/live/test.live.mp4");
            player.open();
        });
    </script>
</body>
</html>
```

## 播放器原理
​       将WebSocket收到的fmp4 Segment片段`appendBuffer`到`MediaSource`中，此时`video.buffered`会记录当前已经`appendBuffer`的视频时间段，然后将`video.buffered`的起始时间设置给`video.currentTime`，然后浏览器就会从`video.buffered`缓存的视频开始播放

## 项目计划
* v1.0 实现用video标签，调用MSE播放WebSocket-fmp4（H.264）直播流，并把播放器封装为标准的npm组件；
* v2.0 实现用WebAssembly FFmpeg解码H.265，然后用canvas标签WebGL渲染YUV，从而实现播放WebSocket-fmp4（H.265）直播流，并实现动态切换H.264、H.265这两种播放机制；
* v3.0 实现视频流SEI信息的callback回调

## 联系方式
- QQ交流群：群名称：wsPlayer  群号：710185138