# 致谢与声明
本文件夹下部分文件提取自[MediaSoup](https://github.com/versatica/mediasoup) ，分别为：

- ice相关功能：
  - IceServer.cpp
  - IceServer.hpp
  - StunPacket.cpp
  - StunPacket.hpp
  - Utils.hpp
  
- dtls相关功能：
   - DtlsTransport.cpp
   - DtlsTransport.hpp
   
- srtp相关功能：
   - SrtpSession.cpp
   - SrtpSession.hpp
   

以上源码有一定的修改和裁剪，感谢MediaSoup开源项目及作者，
用户在使用本项目的同时，应该同时遵循MediaSoup的开源协议。

同时,在此也感谢开源项目[easy_webrtc_server](https://github.com/Mihawk086/easy_webrtc_server) 及作者，
在集成MediaSoup相关代码前期，主要参考这个项目。

另外，感谢[big panda](<2381267071@qq.com>) 开发并贡献的webrtc js测试客户端(www/webrtc目录下文件)，
其开源项目地址为：https://gitee.com/xiongguangjie/zlmrtcclient.js

# 现状与规划
ZLMediaKit的WebRTC相关功能目前仅供测试与开发，现在还不成熟，后续主要工作有：

- 1、完善webrtc rtcp相关功能，包括丢包重传、带宽检测等功能。
- 2、实现rtp重传等相关功能。
- 3、实现simulcast相关功能。
- 4、fec、rtp扩展等其他功能。
- 5、如果精力允许，逐步替换MediaSoup相关代码，改用自有版权代码。

