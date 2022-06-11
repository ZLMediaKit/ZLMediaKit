## feature
- NACK support
- listener support
- push stream payload must ts
- pull stream payload is ts
- protocol impliment [reference](https://haivision.github.io/srt-rfc/draft-sharabayko-srt.html)
- version support (>=1.3.0)
- fec and encriyped not support 

## usage 

zlm get vhost,app,streamid and push or play by streamid of srt like this 
`#!::key1=value1,key2=value2,key3=value4......`

h and r is special key,to get vhost app streamid, if h not exist ,vhost is default value

m is special key, to judge is push or pull, if vaule is publish the mode is push，otherwise is play, if m not exist, mode is play

other key and m ,can use by webhook to auth for play or push


like：
  #!::h=zlmediakit.com,r=live/test,m=publish

  vhost = zlmediakit.com

  app = live

  streamid = test

  mode is push

- OBS push stream url

    `srt://192.168.1.105:9000?streamid=#!::r=live/test,m=publish`
- ffmpeg push 

   `ffmpeg -re -stream_loop -1 -i test.ts -c:v copy -c:a copy -f mpegts srt://192.168.1.105:9000?streamid=#!::r=live/test,m=publish`
- ffplay pull 

    `ffplay -i srt://192.168.1.105:9000?streamid=#!::r=live/test`

- vlc not support ,because can't set stream id [reference](https://github.com/Haivision/srt/issues/1015)