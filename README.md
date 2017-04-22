# 一个基于C++11简单易用的轻量级流媒体库
## 项目优势
- 基于C++11开发，避免使用裸指针，代码稳定可靠；同时跨平台移植简单方便，代码清晰简洁。
- 打包多种流媒体协议(RTSP/RTMP/HLS），支持协议间的互相转换，提供一站式的服务。
- 使用epoll+线程池+异步网络IO模式开发，并发性能优越。
- 只实现主流的的H264+AAC流媒体方案，代码精简,脉络清晰，适合学习。

## 功能清单
- RTSP
  - RTSP 服务器，支持RTMP/MP4转RTSP。
  - RTSP 播放器，支持RTSP代理。
  - 支持 `rtp over udp` `rtp over tcp` `rtp over http` `rtp组播`  四种RTP传输方式 。

- RTMP
  - RTMP 播放服务器，支持RTSP/MP4转RTMP。
  - RTMP 发布服务器，支持录制发布流。
  - RTMP 播放器，支持RTMP代理。
  - RTMP 推流客户端。

- HLS
  - 支持HLS文件生成，自带HTTP文件服务器。

- 其他
  - 支持输入YUV+PCM自动生成RTSP/RTMP/HLS/MP4.
  - 支持简单的telnet调试。
  - 支持H264的解析，支持B帧的POC计算排序。
 
## 后续任务
- 提供cmake编译方式
- 提供更多的示例代码
- 提供ios工程

## 编译
- 我的编译环境
  - Ubuntu16.04 64 bit + gcc5.4(最低gcc4.7)
  - [eclipse for c++](https://www.eclipse.org/downloads/download.php?file=/oomph/epp/neon/R3/eclipse-inst-mac64.tar.gz)
- 依赖
  - 本项目依赖我的另一个项目[ZLToolKit](https://git.oschina.net/xiahcu/ZLToolKit)；编译时，请把两个工程放在同一目录下。
- 使用eclipse编译(Linux)
  - 1、点击菜单：File->Import->Git(Projects from git)-> Clone URI 
  - 2、输入git地址点击 Next 然后选择 master 分支然后一路点击 Next 直至导入项目。
  - 3、选中 ZLToolKit项目，点击鼠标右键在下拉菜单中点击 Build Configurations-> Set Active -> X64，选择编译X64版本目标文件。
  - 4、在ZLMediaKit项目右键菜单中点击 Clean Project 清理项目。
  - 5、在ZLMediaKit项目右键菜单中点击 Build Project 编译项目。
 
- 使用make编译

    如果没有安装eclipse可以使用已经生成的Makefile文件直接编译：

    ```
    # 根据makefile编译
    cd ZLMediaKit/X64
    make clean
    make
    ```

## 联系方式
- QQ群：542509000
