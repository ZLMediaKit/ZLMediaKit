```
src
|
|-- Codec                           # 编码模块
|	|-- AACEncoder.cpp          # 对libfaac的封装
|	|-- AACEncoder.h
|	|-- H264Encoder.cpp         # 对libx264的封装
|	|-- H264Encoder.h
|
|-- Common                          # 杂项，一般文件
|	|-- config.cpp              # 主要功能是生成默认配置
|	|-- config.h                # 定义了一些宏、广播名常量、配置名常量
|	|-- MediaSender.h           # 一条专门的后台线程用来发送RTP、RTMP包
|
|-- Device                          # 原本这个文件夹是拿来做各种IPC(海康/大华/汉邦)适配的
|	|-- base64.cpp              # base64编解码
|	|-- base64.h
|	|-- Device.cpp              # 输入YUV+PCM或H264+AAC生成流媒体源(rtmp/rtsp/hls/mp4)。
|	|-- Device.h
|	|-- PlayerProxy.cpp         # 拉取rtsp/rtmp流生成流媒体源(rtmp/rtsp/hls/mp4)。
|	|-- PlayerProxy.h
|
|-- H264                            # H264帧处理代码，包括SPS/PPS的解析，POC计算等
|	|-- h264_bit_reader.cpp     # 移植于chrome
|	|-- h264_bit_reader.h       # 移植于chrome
|	|-- h264_parser.cpp         # 移植于chrome
|	|-- h264_parser.h           # 移植于chrome
|	|-- h264_poc.cpp            # 移植于chrome
|	|-- h264_poc.h              # 移植于chrome
|	|-- macros.h                # 移植于chrome
|	|-- ranges.h                # 移植于chrome
|	|-- H264Parser.cpp          # H264的POC、pts计算等
|	|-- H264Parser.h
|	|-- SPSParser.c             # 移植于FFmpeg的sps/pps解析代码
|	|-- SPSParser.h
|		 
|-- Http                            # Http[s]的服务器和客户端实现
|	|-- HttpClient.cpp          # Http客户端，可复用
|	|-- HttpClient.h
|	|-- HttpClientImp.cpp       # Https客户端，派生于HttpClient
|	|-- HttpClientImp.h
|	|-- HttpDownloader.cpp      # Http[s]文件下载器，支持断点续传
|	|-- HttpDownloader.h	
|	|-- HttpRequester.cpp       # 实现Http[s]API接口客户端，派生于HttpClientImp
|	|-- HttpRequester.h
|	|-- HttpSession.cpp         # Http服务器，支持GET/POST命令。GET只能用于文件下载
|	|-- HttpSession.h			
|	|-- HttpsSession.h          # https服务器，派生于HttpSession
|	|-- strCoding.cpp           # http url转义和反转义
|	|-- strCoding.h	
|
|-- MediaFile                       # 媒体文件相关，包括mp4文件的读写，hls文件的生成
|	|-- crc32.cpp               # crc32计算，用于ts文件生成
|	|-- crc32.h
|	|-- HLSMaker.cpp            # HLS文件生成，包括m3u8和ts文件生成
|	|-- HLSMaker.h	
|	|-- MediaReader.cpp         # mp4文件(只支持h264+aac)解析，转换成流媒体
|	|-- MediaReader.h			
|	|-- MediaRecorder.cpp       # 录制流媒体为mp4和hls
|	|-- MediaRecorder.h
|	|-- Mp4Maker.cpp            # mp4文件生成，只支持h264+aac
|	|-- Mp4Maker.h			
|	|-- TSMaker.cpp             # ts文件生成，只支持h264+aac
|	|-- TSMaker.h	
|	
|-- Player                          # 播放器
|	|-- MediaPlayer.cpp	    # rtsp/rtmp播放器，只支持h264+acc
|	|-- MediaPlayer.h 
|	|-- Player.cpp              # 播放器音视频帧定义以及一些h264/aac处理工具函数
|	|-- Player.h
|	|-- PlayerBase.cpp	 	
|	|-- PlayerBase.h            # 播放器基类，定义了一些虚函数
|	
|-- Rtmp                            # rtmp协议支持
|	|-- amf.cpp                 # amf格式的解析及编码
|	|-- amf.h
|	|-- Rtmp.h                  # rtmp包定义以及一些rtmp常量宏定义
|	|-- RtmpMediaSource.cpp     # rtmp流媒体源
|	|-- RtmpMediaSource.h	
|	|-- RtmpParser.cpp          # 解析rtmp媒体格式以及提取h264+aac
|	|-- RtmpParser.h	
|	|-- RtmpPlayer.cpp          # rtmp播放器
|	|-- RtmpPlayer.h
|	|-- RtmpPlayerImp.cpp       # 派生于RtmpPlayer，结合RtmpParser
|	|-- RtmpPlayerImp.h		
|	|-- RtmpProtocol.cpp        # rtmp包序列化以及反序列化
|	|-- RtmpProtocol.h	
|	|-- RtmpPusher.cpp          # rtmp推流客户端
|	|-- RtmpPusher.h
|	|-- RtmpSession.cpp         # rtmp服务器，支持播放及推流协议
|	|-- RtmpSession.h
|	|-- RtmpToRtspMediaSource.cpp # rtmp转rtsp实现
|	|-- RtmpToRtspMediaSource.h
|	|-- utils.cpp               # 网络字节序与整形间的互转
|	|-- utils.h	
|	
|-- RTP                             # RTP打包
|	|-- RtpMaker.h              # 打包类基类
|	|-- RtpMakerAAC.cpp         # aac的rtp打包实现
|	|-- RtpMakerAAC.h
|	|-- RtpMakerH264.cpp        # h264的rtp打包实现
|	|-- RtpMakerH264.h
|	
|-- Rtsp                            # rtsp协议支持
|	|-- RtpBroadCaster.cpp      # rtp组播实现
|	|-- RtpBroadCaster.h
|	|-- RtpParser.cpp           # 完成SDP解析以及rtp组包(提取h264+aac)
|	|-- RtpParser.h
|	|-- Rtsp.cpp                # 定义了rtsp里面一些基本的方法及对象
|	|-- Rtsp.h
|	|-- RtspMediaSource.cpp     # rtsp媒体源
|	|-- RtspMediaSource.h
|	|-- RtspPlayer.cpp          # rtsp播放器实现
|	|-- RtspPlayer.h
|	|-- RtspPlayerImp.cpp       # 派生于RtspPlayer，结合了RtpParser
|	|-- RtspPlayerImp.h
|	|-- RtspSession.cpp         # rtsp服务器协议实现
|	|-- RtspSession.h
|	|-- RtspToRtmpMediaSource.cpp
|	|-- RtspToRtmpMediaSource.h # rtsp转rtmp实现
|	|-- UDPServer.cpp
|	|-- UDPServer.h             # udp端口分配器，用来实现rtp over udp
|
|-- Shell                           # 远程shell实现，可以实现简单的远程调试
|	|-- CMD.cpp                 # 抽象了一些shell命令，可以简单的添加命令
|	|-- CMD.h
|	|-- ShellSession.cpp        # shell会话类
|	|-- ShellSession.h	
|
|-- win32                           # windows下命令行解析工具(unix下自带)
	|-- getopt.c
	|-- getopt.h
	|-- tailor.h
``` 
