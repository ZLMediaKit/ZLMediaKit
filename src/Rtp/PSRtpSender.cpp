/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "PSRtpSender.h"
#include "Rtsp/RtspSession.h"
#include "Thread/WorkThreadPool.h"

namespace mediakit{

PSRtpSender::PSRtpSender(uint32_t ssrc, uint8_t payload_type) {
    GET_CONFIG(uint32_t,video_mtu,Rtp::kVideoMtuSize);
    _rtp_encoder = std::make_shared<CommonRtpEncoder>(CodecInvalid, ssrc, video_mtu, 90000, payload_type, 0);
    _rtp_encoder->setRtpRing(std::make_shared<RtpRing::RingType>());
    _rtp_encoder->getRtpRing()->setDelegate(std::make_shared<RingDelegateHelper>([this](const RtpPacket::Ptr &rtp, bool is_key){
        onRtp(rtp, is_key);
    }));
    _poller = EventPollerPool::Instance().getPoller();
    InfoL << this << " " << printSSRC(_rtp_encoder->getSsrc());
}

PSRtpSender::~PSRtpSender() {
    InfoL << this << " " << printSSRC(_rtp_encoder->getSsrc());
}

void PSRtpSender::onPS(uint32_t stamp, void *packet, size_t bytes) {
    _rtp_encoder->inputFrame(std::make_shared<FrameFromPtr>((char *) packet, bytes, stamp));
}

void PSRtpSender::startSend(const string &dst_url, uint16_t dst_port, bool is_udp, const function<void(const SockException &ex)> &cb){
    _is_udp = is_udp;
    _socket = std::make_shared<Socket>();
    _dst_url = dst_url;
    _dst_port = dst_port;
    weak_ptr<PSRtpSender> weak_self = shared_from_this();
    if (is_udp) {
        _socket->bindUdpSock(0);
        WorkThreadPool::Instance().getPoller()->async([cb, dst_url, dst_port, weak_self]() {
            //切换线程目的是为了dns解析放在后台线程执行
            struct sockaddr addr;
            if (!SockUtil::getDomainIP(dst_url.data(), dst_port, addr)) {
                cb(SockException(Err_dns, StrPrinter << "dns解析域名失败:" << dst_url));
                return;
            }
            cb(SockException());
            auto strong_self = weak_self.lock();
            if (strong_self) {
                //dns解析成功
                strong_self->_socket->setSendPeerAddr(&addr);
                strong_self->onConnect();
            }
        });
    } else {
        _socket->connect(dst_url, dst_port, [cb, weak_self](const SockException &err) {
            cb(err);
            auto strong_self = weak_self.lock();
            if (strong_self && !err) {
                //tcp连接成功
                strong_self->onConnect();
            }
        });
    }
}

void PSRtpSender::onConnect(){
    _is_connect = true;
    //加大发送缓存,防止udp丢包之类的问题
    SockUtil::setSendBuf(_socket->rawFD(), 4 * 1024 * 1024);
    if(!_is_udp){
        //关闭tcp no_delay并开启MSG_MORE, 提高发送性能
        SockUtil::setNoDelay(_socket->rawFD(), false);
        _socket->setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
    }
    //连接建立成功事件
    weak_ptr<PSRtpSender> weak_self = shared_from_this();
    _socket->setOnErr([weak_self](const SockException &err) {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->onErr(err);
        }
    });
    InfoL << "开始发送 ps rtp:" << _socket->get_peer_ip() << ":" << _socket->get_peer_port() << ", 是否为udp方式:" << _is_udp;
}

void PSRtpSender::onRtp(const RtpPacket::Ptr &rtp, bool) {
    if(!_is_connect){
        return;
    }

    //开启合并写提高发送性能
    PacketCache<RtpPacket>::inputPacket(true, rtp, false);
}

void PSRtpSender::onFlush(shared_ptr<List<RtpPacket::Ptr>> &rtp_list, bool key_pos) {
    int i = 0;
    int size = rtp_list->size();
    rtp_list->for_each([&](const RtpPacket::Ptr &packet) {
        if (_is_udp) {
            //udp模式，rtp over tcp前4个字节可以忽略
            _socket->send(std::make_shared<BufferRtp>(packet, 4), nullptr, 0, ++i == size);
        } else {
            //tcp模式, rtp over tcp前2个字节可以忽略,只保留后续rtp长度的2个字节
            _socket->send(std::make_shared<BufferRtp>(packet, 2), nullptr, 0, ++i == size);
        }
    });
}

void PSRtpSender::onErr(const SockException &ex, bool is_connect) {
    _is_connect = false;

    //监听socket断开事件，方便重连
    if (is_connect) {
        WarnL << "重连" << _dst_url << ":" << _dst_port << "失败, 原因为:" << ex.what();
    } else {
        WarnL << "停止发送 ps rtp:" <<  _dst_url << ":" << _dst_port << ", 原因为:" << ex.what();
    }

    weak_ptr<PSRtpSender> weak_self = shared_from_this();
    _connect_timer = std::make_shared<Timer>(10.0, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->startSend(strong_self->_dst_url, strong_self->_dst_port, strong_self->_is_udp, [weak_self](const SockException &ex){
            auto strong_self = weak_self.lock();
            if (strong_self && ex) {
                //连接失败且本对象未销毁，那么重试连接
                strong_self->onErr(ex, true);
            }
        });
        return false;
    }, _poller);
}

}//namespace mediakit

