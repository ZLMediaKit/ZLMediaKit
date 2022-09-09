/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPSERVER_H
#define ZLMEDIAKIT_RTPSERVER_H

#if defined(ENABLE_RTPPROXY)
#include <memory>
#include "Network/Socket.h"
#include "Network/TcpServer.h"
#include "Network/UdpServer.h"
#include "RtpSession.h"

namespace mediakit{

/**
 * RTP服务器，支持UDP/TCP
 */
class RtpServer : public std::enable_shared_from_this<RtpServer> {
public:
    using Ptr = std::shared_ptr<RtpServer>;
    using onRecv = std::function<void(const toolkit::Buffer::Ptr &buf)>;
    enum TcpMode { NONE = 0, PASSIVE, ACTIVE };

    RtpServer();
    ~RtpServer();

    /**
     * 开启服务器，可能抛异常
     * @param local_port 本地端口，0时为随机端口
     * @param stream_id 流id，置空则使用ssrc
     * @param tcp_mode tcp服务模式
     * @param local_ip 绑定的本地网卡ip
     * @param re_use_port 是否设置socket为re_use属性
     * @param ssrc 指定的ssrc
     */
    void start(uint16_t local_port, const std::string &stream_id = "", TcpMode tcp_mode = PASSIVE,
               const char *local_ip = "::", bool re_use_port = true, uint32_t ssrc = 0);

    /**
     * 连接到tcp服务(tcp主动模式)
     * @param url 服务器地址
     * @param port 服务器端口
     * @param cb 连接服务器是否成功的回调
     */
    void connectToServer(const std::string &url, uint16_t port, const std::function<void(const toolkit::SockException &ex)> &cb);

    /**
     * 获取绑定的本地端口
     */
    uint16_t getPort();

    /**
     * 设置RtpProcess onDetach事件回调
     */
    void setOnDetach(const std::function<void()> &cb);

private:
    // tcp主动模式连接服务器成功回调
    void onConnect();

protected:
    toolkit::Socket::Ptr _rtp_socket;
    toolkit::UdpServer::Ptr _udp_server;
    toolkit::TcpServer::Ptr _tcp_server;
    RtpProcess::Ptr _rtp_process;
    std::function<void()> _on_cleanup;

    //用于tcp主动模式
    TcpMode _tcp_mode = NONE;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSERVER_H
