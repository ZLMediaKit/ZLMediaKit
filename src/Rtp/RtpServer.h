/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
#include "RtpSession.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

/**
 * RTP服务器，支持UDP/TCP
 */
class RtpServer {
public:
    typedef std::shared_ptr<RtpServer> Ptr;
    typedef function<void(const Buffer::Ptr &buf)> onRecv;

    RtpServer();
    ~RtpServer();

    /**
     * 开启服务器，可能抛异常
     * @param local_port 本地端口，0时为随机端口
     * @param stream_id 流id，置空则使用ssrc
     * @param enable_tcp 是否启用tcp服务器
     * @param local_ip 绑定的本地网卡ip
     */
    void start(uint16_t local_port, const string &stream_id = "", bool enable_tcp = true, const char *local_ip = "0.0.0.0");

    /**
     * 获取绑定的本地端口
     */
    uint16_t getPort();

    /**
     * 获取绑定的线程
     */
    EventPoller::Ptr getPoller();

    /**
     * 设置RtpProcess onDetach事件回调
     */
    void setOnDetach(const function<void()> &cb);

protected:
    Socket::Ptr _udp_server;
    TcpServer::Ptr _tcp_server;
    RtpProcess::Ptr _rtp_process;
    function<void()> _on_clearup;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSERVER_H
