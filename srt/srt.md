## 特性
- NACK(重传)
- listener 支持
- 推流只支持ts推流
- 拉流只支持ts拉流
- 协议实现 [参考](https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html)
- 版本支持(>=1.3.0)
- fec与加密没有实现

## 使用

zlm中的srt根据streamid 来确定是推流还是拉流，来确定vhost,app,streamid(ZLM中的)、

srt中的streamid 为 `#!::key1=value1,key2=value2,key3=value4......`

h,r为特殊的key,来确定vhost,app,streamid,如果没有h则vhost为默认值

m 为特殊key来确定是推流还是拉流，如果为publish 则为推流，否则为拉流 ,如果不存在m,则默认为拉流

其他key与m会作为webhook的鉴权参数

如：
  #!::h=zlmediakit.com,r=live/test,m=publish

  vhost = zlmediakit.com

  app = live

  streamid = test

  是推流


- OBS 推流地址

    `srt://192.168.1.105:9000?streamid=#!::r=live/test,m=publish`
- ffmpeg 推流

    `ffmpeg -re -stream_loop -1 -i test.ts -c:v copy -c:a copy -f mpegts srt://192.168.1.105:9000?streamid=#!::r=live/test,m=publish`
- ffplay 拉流

    `ffplay -i srt://192.168.1.105:9000?streamid=#!::r=live/test`

- vlc 拉流
    - vlc拉流需要在偏好设置->串流输出->访问输出->SRT中设置streamid,例如`#!::r=live/test`
    - 拉流时只需填入`srt://192.168.1.105:9000`即可