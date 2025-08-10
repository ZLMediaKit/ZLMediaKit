# WebRTC 使用说明

## WebRTC 架构

### 1. SFU 模式架构 (WHIP/WHEP)

SFU 模式通过服务器中继媒体流，支持多路复用和转码：

```
                    WebRTC SFU 模式 (WHIP/WHEP)
                         
     推流端 (WHIP)                                          拉流端 (WHEP)
   +----------------+                                 +-----------------+
   |   Encoder      |                                 |   Player        |
   |  (Browser/ZLM) |                                 |  (Browser/ZLM)  |
   +----------------+                                 +-----------------+
          |                                               |
          | WHIP Protocol                                 | WHEP Protocol
          | (WebRTC ingest)                               | (WebRTC playback)
          |                                               |
          v                                               v
    +-------------------------------------------------------------------+
    |                          ZLMediaKit Server                        |
    +-------------------------------------------------------------------+
    - WHIP: WebRTC-HTTP Ingestion Protocol (推流)
    - WHEP: WebRTC-HTTP Egress Protocol (拉流)
```

### 2. P2P 模式架构

P2P 模式允许客户端之间直接建立连接，减少服务器负载：
基于Websocket的自定义信令协议

```
                    WebRTt WC P2P 模式
                         
    客户端 A                                      客户端 B
   +------------+                                +-------------+
   | Browser/ZLM|                                | Browser/ZLM |
   +------------+                                +-------------+
        |                                               |
        | 1. 信令交换 (SDP Offer/Answer)                 |
        | 2. ICE Candidate 交换                         |
        +----------------  -----+-----------------------+
        |                       |                       |
        |           +-----------------------+           |
        |           | ZLMediaKit Server     |           |
        |           | 信令服务器 (WebSocket) |           |
        |           | STUN 服务器            |           |
        |           | TURN 服务器            |           |
        |           +-----------------------+           |
        |                                               |
        +-----------------------------------------------+
                            直接P2P连接
```

## HTTP API 接口

### 1. WebRTC 房间管理

#### `/index/api/addWebrtcRoomKeeper`
添加WebRTC到指定信令服务器，用于在信令服务器中维持房间连接。

**请求参数:**
- `secret`: 接口访问密钥  
- `server_host`: 信令服务器主机地址
- `server_port`: 信令服务器端口
- `room_id`: 房间ID，信令服务器会对该ID进行唯一性检查

#### `/index/api/delWebrtcRoomKeeper`  
删除指定的信令服务器。

**请求参数:**
- `secret`: 接口访问密钥
- `room_key`: 房间保持器的唯一标识符

#### `/index/api/listWebrtcRoomKeepers`
列出所有信令服务器。

**请求参数:**
- `secret`: 接口访问密钥

### 2. WebRTC 房间会话管理

#### `/index/api/listWebrtcRooms`
列出所有活跃的WebRTC Peer会话信息。

**请求参数:**
- `secret`: 接口访问密钥

### 3. WebRTC 推流和拉流接口

ZLMediaKit 支持通过标准的流代理接口来创建WebRTC推流和拉流，支持两种信令模式：

##### `/index/api/addStreamProxy` - WebRTC 拉流代理

通过此接口可以创建WebRTC拉流代理，支持两种信令协议模式。

**请求参数:**
- `secret`: 接口访问密钥
- `vhost`: 虚拟主机名，默认为 `__defaultVhost__`
- `app`: 应用名
- `stream`: 流ID
- `url`: WebRTC源URL，支持两种格式

**WebRTC URL 格式:**

1. **WHIP/WHEP 模式 (SFU)** - 标准HTTP信令协议:
   ```
   # HTTP
   webrtc://server_host:server_port/app/stream_id?signaling_protocols=0
   
   # HTTPS (暂未实现）
   webrtcs://server_host:server_port/app/stream_id?signaling_protocols=0
   ```

2. **WebSocket P2P 模式** - 自定义信令协议:
   ```
   # WebSocket
   webrtc://signaling_server_host:signaling_server_port/app/stream_id?signaling_protocols=1&peer_room_id=target_room_id
   
   # WebSocket Secure (暂未实现)
   webrtcs://signaling_server_host:signaling_server_port/app/stream_id?signaling_protocols=1&peer_room_id=target_room_id
   ```

**请求示例:**
```bash
# WHIP/WHEP 模式拉流
curl -X POST "http://127.0.0.1/index/api/addStreamProxy" \
  -d "secret=your_secret" \
  -d "vhost=__defaultVhost__" \
  -d "app=live" \
  -d "stream=test" \
  -d "url=webrtc://source.server.com:80/live/source_stream?signaling_protocols=0"

# P2P 模式拉流
curl -X POST "http://127.0.0.1/index/api/addStreamProxy" \
  -d "secret=your_secret" \
  -d "vhost=__defaultVhost__" \
  -d "app=live" \
  -d "stream=test" \
  -d "url=webrtc://signaling.server.com:3000/live/source_stream??signaling_protocols=1%26peer_room_id=target_room_id"
```

#### `/index/api/addStreamPusherProxy` - WebRTC 推流代理  (暂未实现)

通过此接口可以创建WebRTC推流代理，将现有流推送到WebRTC目标服务器。

**请求参数:**
- `secret`: 接口访问密钥
- `schema`: 源流协议 (如: rtmp, rtsp, hls等)
- `vhost`: 虚拟主机名
- `app`: 应用名  
- `stream`: 源流ID
- `dst_url`: WebRTC目标推流URL

**WebRTC 推流 URL 格式:**

1. **WHIP 模式 (SFU)** - 推流到支持WHIP的服务器:
   ```
   # HTTP
   webrtc://target_server:port/app/stream_id?signaling_protocols=0
   
   # HTTPS (暂未实现)
   webrtcs://target_server:port/app/stream_id?signaling_protocols=0
   ```

2. **WebSocket P2P 模式** - 推流到P2P房间 
   ```
   # WebSocket 
   webrtc://signaling_server:port/app/stream_id?signaling_protocols=1&peer_room_id=target_room
   # WebSocket Secure
   webrtcs://signaling_server:port/app/stream_id?signaling_protocols=1&peer_room_id=target_room
   ```

**请求示例:**
```bash
# 将RTSP流推送到WHIP服务器
curl -X POST "http://127.0.0.1/index/api/addStreamPusherProxy" \
  -d "secret=your_secret" \
  -d "schema=rtsp" \
  -d "vhost=__defaultVhost__" \
  -d "app=live" \
  -d "stream=test" \
  -d "dst_url=webrtc://target.server.com:80/live/target_stream?signaling_protocols=0"

# 将RTSP流推送到P2P房间
curl -X POST "http://127.0.0.1/index/api/addStreamPusherProxy" \
  -d "secret=your_secret" \
  -d "schema=rtsp" \
  -d "vhost=__defaultVhost__" \
  -d "app=live" \
  -d "stream=test" \
  -d "dst_url=webrtc://signaling.server.com:3000/live/room_stream?signaling_protocols=1%26peer_room_id=target_room_id"
```

#### URL 参数说明

- `signaling_protocols`: 信令协议类型
  - `0`: WHIP/WHEP 模式（默认）
    - **协议**: 基于HTTP的标准WebRTC信令协议
    - **应用场景**: SFU（选择性转发单元）模式，适合广播和多人会议
  - `1`: WebSocket P2P 模式
    - **协议**: 基于WebSocket的自定义信令协议
    - **应用场景**: 点对点直连，适合低延迟通话和私人通信
- `peer_room_id`: P2P模式下的目标房间ID（仅P2P模式需要）

### 4. WebRTC 代理播放器信息查询

#### `/index/api/getWebrtcProxyPlayerInfo`
获取WebRTC代理播放器的连接信息和状态。

**请求参数:**
- `secret`: 接口访问密钥
- `key`: 代理播放器标识符


## WebRTC 相关配置项

在 `config.ini` 中的 `[rtc]` 配置段：

``` ini
[rtc]
#webrtc 信令服务器端口
signalingPort=3000
#STUN/TURN服务器端口
icePort=3478
#STUN/TURN端口是否使能TURN服务
enableTurn=1

#TURN服务分配端口池
portRange=50000-65000

#ICE传输策略：0=不限制(默认)，1=仅支持Relay转发，2=仅支持P2P直连
iceTransportPolicy=0

#STUN/TURN 服务Ice密码
iceUfrag=ZLMediaKit
icePwd=ZLMediaKit
```

## Examples
- [zlm_peerconnection](https://gitee.com/libwebrtc_develop/libwebrtc/tree/feature-zlm/examples/zlm_peerconnection)
  一个基于libwebrtc 实现的zlm p2p 代理拉流简单示例

## 注意事项

1. **防火墙配置**: 确保 WebRTC 相关端口已开放
   - 信令端口: 3000 (默认)
   - STUN/TURN 端口: 3478 (默认)
   - TURN Alloc 端口范围: 50000-65000(默认)

## 暂未实现的功能:
- Webrtc信令服务的安全校验
- 自定义外部STUN/TURN 服务器的配置
- webrtc代理推流
