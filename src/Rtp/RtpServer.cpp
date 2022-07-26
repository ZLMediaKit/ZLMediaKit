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
#include "Util/uv_errno.h"
#include "RtpServer.h"
#include "RtpSelector.h"
#include "Rtcp/RtcpContext.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

RtpServer::RtpServer() {}

RtpServer::~RtpServer() {
    if(_on_clearup){
        _on_clearup();
    }
}

class RtcpHelper : public RtcpContextForRecv, public std::enable_shared_from_this<RtcpHelper> {
public:
    using Ptr = std::shared_ptr<RtcpHelper>;

    RtcpHelper(Socket::Ptr rtcp_sock, uint32_t sample_rate) {
        _rtcp_sock = std::move(rtcp_sock);
        _sample_rate = sample_rate;
    }

    void onRecvRtp(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len){
        //统计rtp接受情况，用于发送rr包
        auto header = (RtpHeader *) buf->data();
        onRtp(ntohs(header->seq), ntohl(header->stamp), 0/*不发送sr,所以可以设置为0*/ , _sample_rate, buf->size());
        sendRtcp(ntohl(header->ssrc), addr, addr_len);
    }

    void startRtcp(){
        weak_ptr<RtcpHelper> weak_self = shared_from_this();
        _rtcp_sock->setOnRead([weak_self](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            //用于接受rtcp打洞包
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            if (!strong_self->_rtcp_addr) {
                //只设置一次rtcp对端端口
                strong_self->_rtcp_addr = std::make_shared<struct sockaddr_storage>();
                memcpy(strong_self->_rtcp_addr.get(), addr, addr_len);
            }
            auto rtcps = RtcpHeader::loadFromBytes(buf->data(), buf->size());
            for (auto &rtcp : rtcps) {
                strong_self->onRtcp(rtcp);
            }
        });
    }

private:
    void sendRtcp(uint32_t rtp_ssrc, struct sockaddr *addr, int addr_len){
        //每5秒发送一次rtcp
        if (_ticker.elapsedTime() < 5000) {
            return;
        }
        _ticker.resetTime();

        auto rtcp_addr = (struct sockaddr *)_rtcp_addr.get();
        if (!rtcp_addr) {
            //默认的，rtcp端口为rtp端口+1
            switch(addr->sa_family){
                case AF_INET: ((sockaddr_in *) addr)->sin_port = htons(ntohs(((sockaddr_in *) addr)->sin_port) + 1); break;
                case AF_INET6: ((sockaddr_in6 *) addr)->sin6_port = htons(ntohs(((sockaddr_in6 *) addr)->sin6_port) + 1); break;
            }
            //未收到rtcp打洞包时，采用默认的rtcp端口
            rtcp_addr = addr;
        }
        _rtcp_sock->send(createRtcpRR(rtp_ssrc + 1, rtp_ssrc), rtcp_addr, addr_len);
    }

private:
    Ticker _ticker;
    Socket::Ptr _rtcp_sock;
    uint32_t _sample_rate;
    std::shared_ptr<struct sockaddr_storage> _rtcp_addr;
};

void RtpServer::start(uint16_t local_port, const string &stream_id, bool enable_tcp, const char *local_ip, bool re_use_port, uint32_t ssrc) {
    //创建udp服务器
    Socket::Ptr rtp_socket = Socket::createSocket(nullptr, true);
    Socket::Ptr rtcp_socket = Socket::createSocket(nullptr, true);
    if (local_port == 0) {
        //随机端口，rtp端口采用偶数
        auto pair = std::make_pair(rtp_socket, rtcp_socket);
        makeSockPair(pair, local_ip, re_use_port);
    } else if (!rtp_socket->bindUdpSock(local_port, local_ip, re_use_port)) {
        //用户指定端口
        throw std::runtime_error(StrPrinter << "创建rtp端口 " << local_ip << ":" << local_port << " 失败:" << get_uv_errmsg(true));
    } else if (!rtcp_socket->bindUdpSock(rtp_socket->get_local_port() + 1, local_ip, re_use_port)) {
        // rtcp端口
        throw std::runtime_error(StrPrinter << "创建rtcp端口 " << local_ip << ":" << rtp_socket->get_local_port() + 1 << " 失败:" << get_uv_errmsg(true));
    }

    //设置udp socket读缓存
    SockUtil::setRecvBuf(rtp_socket->rawFD(), 4 * 1024 * 1024);

    TcpServer::Ptr tcp_server;
    if (enable_tcp) {
        //创建tcp服务器
        tcp_server = std::make_shared<TcpServer>(rtp_socket->getPoller());
        (*tcp_server)[RtpSession::kStreamID] = stream_id;
        (*tcp_server)[RtpSession::kIsUDP] = 0;
        (*tcp_server)[RtpSession::kSSRC] = ssrc;
        tcp_server->start<RtpSession>(rtp_socket->get_local_port(), local_ip);
    }

    //创建udp服务器
    UdpServer::Ptr udp_server;
    RtpProcess::Ptr process;
    if (!stream_id.empty()) {
        //指定了流id，那么一个端口一个流(不管是否包含多个ssrc的多个流，绑定rtp源后，会筛选掉ip端口不匹配的流)
        process = RtpSelector::Instance().getProcess(stream_id, true);
        RtcpHelper::Ptr helper = std::make_shared<RtcpHelper>(std::move(rtcp_socket), 90000);
        process->setHelper(helper);
        helper->startRtcp();
        rtp_socket->setOnRead([rtp_socket, process, helper, ssrc](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            RtpHeader *header = (RtpHeader *)buf->data();
            auto rtp_ssrc = ntohl(header->ssrc);
            if (ssrc && rtp_ssrc != ssrc) {
                WarnL << "ssrc不匹配,rtp已丢弃:" << rtp_ssrc << " != " << ssrc;
            } else {
                process->inputRtp(true, rtp_socket, buf->data(), buf->size(), addr);
                helper->onRecvRtp(buf, addr, addr_len);
            }
        });
    } else {
#if 1
        //单端口多线程接收多个流，根据ssrc区分流
        udp_server = std::make_shared<UdpServer>(rtp_socket->getPoller());
        (*udp_server)[RtpSession::kIsUDP] = 1;
        udp_server->start<RtpSession>(rtp_socket->get_local_port(), local_ip);
        rtp_socket = nullptr;
#else
        //单端口单线程接收多个流
        auto &ref = RtpSelector::Instance();
        rtp_socket->setOnRead([&ref, rtp_socket](const Buffer::Ptr &buf, struct sockaddr *addr, int) {
            ref.inputRtp(rtp_socket, buf->data(), buf->size(), addr);
        });
#endif
    }

    _on_clearup = [rtp_socket, process, stream_id]() {
        if (rtp_socket) {
            //去除循环引用
            rtp_socket->setOnRead(nullptr);
        }
        if (process) {
            //删除rtp处理器
            RtpSelector::Instance().delProcess(stream_id, process.get());
        }
    };

    _tcp_server = tcp_server;
    _udp_server = udp_server;
    _rtp_socket = rtp_socket;
    _rtp_process = process;
}

void RtpServer::setOnDetach(const function<void()> &cb) {
    if (_rtp_process) {
        _rtp_process->setOnDetach(cb);
    }
}

uint16_t RtpServer::getPort() {
    return _udp_server ? _udp_server->getPort() : _rtp_socket->get_local_port();
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
