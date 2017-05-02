# 一个基于C++11简单易用的轻量级流媒体库
平台|编译状态
----|-------
Linux | [![Build Status](https://travis-ci.org/xiongziliang/ZLMediaKit.svg?branch=master)](https://travis-ci.org/xiongziliang/ZLMediaKit)

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
- 提供更多的示例代码
- 提供ios工程

## 编译(Linux)
- 我的编译环境
  - Ubuntu16.04 64 bit + gcc5.4(最低gcc4.7)
  - cmake 3.5.1
- 依赖
  - cmake：
  
    ```
    # 安装cmake
    sudo apt-get insatll cmake
    ```
     
  - libmysqlclient（使能ENABLE_MYSQL宏，非必备项）
  
    ```
    # 安装mysql客户端开发套件
    sudo apt-get install libmysqlclient-dev
    ```

  - libssl（使能ENABLE_OPENSSL宏，非必备项）
  
    ```
    # 安装openssl开发套件
    sudo apt-get install openssl
    sudo apt-get install libssl-dev
    ```
  
  - [ZLToolKit](https://github.com/xiongziliang/ZLToolKit)
  
    ```
    git clone --depth=50 https://github.com/xiongziliang/ZLToolKit.git
    cd ZLToolKit
    mkdir -p build
    cd build
    cmake ..
    make
    sudo make install
    ```
    
   - libfaac（使能ENABLE_FAAC宏，非必备项）
   
     ```
     # 安装faac开发套件
     sudo apt-get install libfaac-dev
     ```
    
   - libx264（使能ENABLE_X264宏，非必备项）
   
     ```
     # 安装x264开发套件
     sudo apt-get install libx264-dev
     ```
    
  - libmp4v2（使能ENABLE_MP4V2宏，非必备项）
  
    ```
    # 安装mp4v2开发套件
    sudo apt-get install libmp4v2-dev
    ```
    
- 编译
  
  ```
  cd ZLMediaKit
  mkdir -p build
  cd build
  cmake ..
  make
  make install
  ```  

## 联系方式
- 邮箱：<771730766@qq.com>
- QQ群：542509000

