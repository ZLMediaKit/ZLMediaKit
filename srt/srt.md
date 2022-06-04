## 特性
- NACK(重传)
- listener 支持
- 推流只支持ts推流
- 拉流只支持ts拉流
- 协议实现 [参考](https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html)
- 版本支持(>=1.3.0)
- fec与加密没有实现

## 使用

zlm中的srt更加streamid 来确定是推流还是拉流，来确定vhost,app,streamid(ZLM中的)
srt中的streamid 为 `<vhost>/<app>/<streamid>?type=<push|play>& <other>=<other>`

- OBS 推流地址

    `srt://192.168.1.105:9000?streamid=__defaultVhost__/live/test?type=push`
- ffmpeg 推流

    `ffmpeg -re -stream_loop -1 -i test.ts -c:v copy -c:a copy -f mpegts srt://192.168.1.105:9000?streamid="__defaultVhost__/live/test?type=push"`
- ffplay 拉流

    `ffplay -i srt://192.168.1.105:9000?streamid=__defaultVhost__/live/test`

- vlc 不支持,因为无法指定streamid[参考](https://github.com/Haivision/srt/issues/1015)