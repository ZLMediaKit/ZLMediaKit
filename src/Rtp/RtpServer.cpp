/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
    _udp_server.reset(new Socket(nullptr, false));
    //创建udp服务器
    if (!_udp_server->bindUdpSock(local_port, local_ip)) {
        _udp_server = nullptr;
        string err = (StrPrinter << "bindUdpSock on " << local_ip << ":" << local_port << " failed:" << get_uv_errmsg(true));
        throw std::runtime_error(err);
    }

    //设置udp socket读缓存
    SockUtil::setRecvBuf(_udp_server->rawFD(), 4 * 1024 * 1024);

    if (enable_tcp) {
        try {
            //创建tcp服务器
            _tcp_server = std::make_shared<TcpServer>(_udp_server->getPoller());
            (*_tcp_server)[RtpSession::kStreamID] = stream_id;
            _tcp_server->start<RtpSession>(_udp_server->get_local_port(), local_ip);
        } catch (...) {
            _tcp_server = nullptr;
            _udp_server = nullptr;
            throw;
        }
    }

    auto sock = _udp_server;
    RtpProcess::Ptr process;
    if (!stream_id.empty()) {
        //指定了流id，那么一个端口一个流(不管是否包含多个ssrc的多个流，绑定rtp源后，会筛选掉ip端口不匹配的流)
        process = RtpSelector::Instance().getProcess(stream_id, true);
        _udp_server->setOnRead([sock, process](const Buffer::Ptr &buf, struct sockaddr *addr, int) {
            process->inputRtp(sock, buf->data(), buf->size(), addr);
        });
    } else {
        //未指定流id，一个端口多个流，通过ssrc来分流
        auto &ref = RtpSelector::Instance();
        _udp_server->setOnRead([&ref, sock](const Buffer::Ptr &buf, struct sockaddr *addr, int) {
            ref.inputRtp(sock, buf->data(), buf->size(), addr);
        });
    }

    _on_clearup = [sock, process, stream_id]() {
        //去除循环引用
        sock->setOnRead(nullptr);
        if (process) {
            //删除rtp处理器
            RtpSelector::Instance().delProcess(stream_id, process.get());
        }
    };
}

EventPoller::Ptr RtpServer::getPoller() {
    return _udp_server->getPoller();
}

uint16_t RtpServer::getPort() {
    return _udp_server ? _udp_server->get_local_port() : 0;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)