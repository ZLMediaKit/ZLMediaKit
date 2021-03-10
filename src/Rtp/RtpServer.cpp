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
#include "Rtcp/RtcpContext.h"
namespace mediakit{

RtpServer::RtpServer() {
}

RtpServer::~RtpServer() {
    if(_on_clearup){
        _on_clearup();
    }
}

class RtcpHelper : public RtcpContext, public std::enable_shared_from_this<RtcpHelper> {
public:
    using Ptr = std::shared_ptr<RtcpHelper>;

    RtcpHelper(Socket::Ptr rtcp_sock, uint32_t sample_rate) : RtcpContext(sample_rate, true){
        _rtcp_sock = std::move(rtcp_sock);
        _sample_rate = sample_rate;
    }

    void onRecvRtp(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len){
        //统计rtp接受情况，用于发送rr包
        auto header = (RtpHeader *) buf->data();
        onRtp(ntohs(header->seq), ntohl(header->stamp) * uint64_t(1000) / _sample_rate, buf->size());
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
                strong_self->_rtcp_addr = std::make_shared<struct sockaddr>();
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

        auto rtcp_addr = _rtcp_addr.get();
        if (!rtcp_addr) {
            //默认的，rtcp端口为rtp端口+1
            ((sockaddr_in *) addr)->sin_port = htons(ntohs(((sockaddr_in *) addr)->sin_port) + 1);
            //未收到rtcp打洞包时，采用默认的rtcp端口
            rtcp_addr = addr;
        }
        _rtcp_sock->send(createRtcpRR(rtp_ssrc + 1, rtp_ssrc), rtcp_addr, addr_len);
    }

private:
    Ticker _ticker;
    Socket::Ptr _rtcp_sock;
    uint32_t _sample_rate;
    std::shared_ptr<struct sockaddr> _rtcp_addr;
};

void RtpServer::start(uint16_t local_port, const string &stream_id,  bool enable_tcp, const char *local_ip) {
    //创建udp服务器
    Socket::Ptr udp_server = Socket::createSocket(nullptr, true);
    Socket::Ptr rtcp_server = Socket::createSocket(nullptr, true);
    if (local_port == 0) {
        //随机端口，rtp端口采用偶数
        auto pair = std::make_pair(udp_server, rtcp_server);
        makeSockPair(pair, local_ip);
        //取偶数端口
        udp_server = pair.first;
        rtcp_server = pair.second;
    } else if (!udp_server->bindUdpSock(local_port, local_ip)) {
        //用户指定端口
        throw std::runtime_error(StrPrinter << "创建rtp端口 " << local_ip << ":" << local_port << " 失败:" << get_uv_errmsg(true));
    } else if(!rtcp_server->bindUdpSock(udp_server->get_local_port() + 1, local_ip)) {
        // rtcp端口
        throw std::runtime_error(StrPrinter << "创建rtcp端口 " << local_ip << ":" << local_port << " 失败:" << get_uv_errmsg(true));
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
        RtcpHelper::Ptr helper = std::make_shared<RtcpHelper>(std::move(rtcp_server), 90000);
        helper->startRtcp();
        udp_server->setOnRead([udp_server, process, helper](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            helper->onRecvRtp(buf, addr, addr_len);
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

void RtpServer::pauseRtpCheck(const string &stream_id){    
    if(_rtp_process)
        _rtp_process->setStopCheckRtp(true);
    else{
        if(!stream_id.empty()){
            auto rtp_process = RtpSelector::Instance().getProcess(stream_id,false);
            if(rtp_process)
                rtp_process->setStopCheckRtp(true);
        }
    }
    
}

void RtpServer::resumeRtpCheck(const string &stream_id){
    if(_rtp_process)
        _rtp_process->setStopCheckRtp(false);
    else{
        //解决不指定流或者TCP收流无法暂停
        if(!stream_id.empty()){
        auto rtp_process = RtpSelector::Instance().getProcess(stream_id,false);
        if(rtp_process)
            rtp_process->setStopCheckRtp(false);
        }
    }
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)