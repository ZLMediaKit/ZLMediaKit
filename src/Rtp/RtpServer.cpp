/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "RtpServer.h"
#include "RtpSelector.h"
namespace mediakit{

RtpServer::RtpServer() {
}

RtpServer::~RtpServer() {
    if(_on_clearup){
        _on_clearup();
    }
}

void RtpServer::start(uint16_t local_port, const string &stream_id,  bool enable_tcp, const char *local_ip) {
    //创建udp服务器
    Socket::Ptr udp_server = Socket::createSocket(nullptr, true);
    if (local_port == 0) {
        //随机端口，rtp端口采用偶数
        Socket::Ptr rtcp_server = Socket::createSocket(nullptr, true);
        auto pair = std::make_pair(udp_server, rtcp_server);
        makeSockPair(pair, local_ip);
        //取偶数端口
        udp_server = pair.first;
    } else if (!udp_server->bindUdpSock(local_port, local_ip)) {
        //用户指定端口
        throw std::runtime_error(StrPrinter << "bindUdpSock on " << local_ip << ":" << local_port << " failed:" << get_uv_errmsg(true));
    }
    //设置udp socket读缓存
    SockUtil::setRecvBuf(udp_server->rawFD(), 4 * 1024 * 1024);

    TcpServer::Ptr tcp_server;
    if (enable_tcp) {
        //创建tcp服务器
        tcp_server = std::make_shared<TcpServer>(udp_server->getPoller());
        (*tcp_server)[RtpSession::kStreamID] = stream_id;
        tcp_server->start<RtpSession>(udp_server->get_local_port(), local_ip);
    }

    RtpProcess::Ptr process;
    if (!stream_id.empty()) {
        //指定了流id，那么一个端口一个流(不管是否包含多个ssrc的多个流，绑定rtp源后，会筛选掉ip端口不匹配的流)
        process = RtpSelector::Instance().getProcess(stream_id, true);
        udp_server->setOnRead([udp_server, process](const Buffer::Ptr &buf, struct sockaddr *addr, int) {
            process->inputRtp(true, udp_server, buf->data(), buf->size(), addr);
        });
    } else {
        //未指定流id，一个端口多个流，通过ssrc来分流
        auto &ref = RtpSelector::Instance();
        udp_server->setOnRead([&ref, udp_server](const Buffer::Ptr &buf, struct sockaddr *addr, int) {
            ref.inputRtp(udp_server, buf->data(), buf->size(), addr);
        });
    }

    _on_clearup = [udp_server, process, stream_id]() {
        //去除循环引用
        udp_server->setOnRead(nullptr);
        if (process) {
            //删除rtp处理器
            RtpSelector::Instance().delProcess(stream_id, process.get());
        }
    };

    _tcp_server = tcp_server;
    _udp_server = udp_server;
    _rtp_process = process;
}

void RtpServer::setOnDetach(const function<void()> &cb){
    if(_rtp_process){
        _rtp_process->setOnDetach(cb);
    }
}

EventPoller::Ptr RtpServer::getPoller() {
    return _udp_server->getPoller();
}

uint16_t RtpServer::getPort() {
    return _udp_server ? _udp_server->get_local_port() : 0;
}

void RtpServer::pauseRtpCheck(){    
    if(_rtp_process)
        _rtp_process->setStopCheckRtp(true);
}

void RtpServer::resumeRtpCheck(){
    if(_rtp_process)
        _rtp_process->setStopCheckRtp(false);

}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)