## feature
- NACK support
- listener support
- push stream payload must ts
- pull stream payload is ts
- protocol impliment [reference](https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html)
- version support (>=1.3.0)
- fec and encriyped not support 

## usage 

zlm get vhost,app,streamid and push or play by streamid of srt like this `<vhost>/<app>/<streamid>?type=<push|play>& <other>=<other>`

- OBS push stream url

    `srt://192.168.1.105:9000?streamid=__defaultVhost__/live/test?type=push`
- ffmpeg push 

    `ffmpeg -re -stream_loop -1 -i test.ts -c:v copy -c:a copy -f mpegts srt://192.168.1.105:9000?streamid="__defaultVhost__/live/test?type=push"`
- ffplay pull 

    `ffplay -i srt://192.168.1.105:9000?streamid=__defaultVhost__/live/test`

- vlc not support ,because can't set stream id [reference](https://github.com/Haivision/srt/issues/1015)