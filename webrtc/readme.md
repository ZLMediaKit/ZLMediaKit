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

- datachannel相关功能：
  - SctpAssociation.cpp
  - SctpAssociation.hpp

以上源码有一定的修改和裁剪，感谢MediaSoup开源项目及作者，
用户在使用本项目的同时，应该同时遵循MediaSoup的开源协议。

同时,在此也感谢开源项目[easy_webrtc_server](https://github.com/Mihawk086/easy_webrtc_server) 及作者，
在集成MediaSoup相关代码前期，主要参考这个项目。

另外，感谢[big panda](<2381267071@qq.com>) 开发并贡献的webrtc js测试客户端(www/webrtc目录下文件)，
其开源项目地址为：https://gitee.com/xiongguangjie/zlmrtcclient.js


