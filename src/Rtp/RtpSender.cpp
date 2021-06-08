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
#include "RtpSender.h"
#include "Rtsp/RtspSession.h"
#include "Thread/WorkThreadPool.h"
#include "RtpCache.h"

namespace mediakit{

RtpSender::RtpSender(uint32_t ssrc, uint8_t payload_type) {
    _poller = EventPollerPool::Instance().getPoller();
    _interface = std::make_shared<RtpCachePS>([this](std::shared_ptr<List<Buffer::Ptr> > list) {
        onFlushRtpList(std::move(list));
    }, ssrc, payload_type);
}

RtpSender::~RtpSender() {
}

void RtpSender::startSend(const string &dst_url, uint16_t dst_port, bool is_udp, uint16_t src_port, const function<void(uint16_t local_port, const SockException &ex)> &cb){
    _is_udp = is_udp;
    _socket = Socket::createSocket(_poller, false);
    _dst_url = dst_url;
    _dst_port = dst_port;
	_src_port = src_port;
    weak_ptr<RtpSender> weak_self = shared_from_this();
    if (is_udp) {
        _socket->bindUdpSock(src_port);
        auto poller = _poller;
        auto local_port = _socket->get_local_port();
        WorkThreadPool::Instance().getPoller()->async([cb, dst_url, dst_port, weak_self, poller, local_port]() {
            struct sockaddr addr;
            //切换线程目的是为了dns解析放在后台线程执行
            if (!SockUtil::getDomainIP(dst_url.data(), dst_port, addr)) {
                poller->async([dst_url, cb, local_port]() {
                    //切回自己的线程
                    cb(local_port, SockException(Err_dns, StrPrinter << "dns解析域名失败:" << dst_url));
                });
                return;
            }

            //dns解析成功
            poller->async([addr, weak_self, cb, local_port]() {
                //切回自己的线程
                cb(local_port, SockException());
                auto strong_self = weak_self.lock();
                if (strong_self) {
                    strong_self->_socket->bindPeerAddr(&addr);
                    strong_self->onConnect();
                }
            });
        });
    } else {
        _socket->connect(dst_url, dst_port, [cb, weak_self](const SockException &err) {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                if (!err) {
                    //tcp连接成功
                    strong_self->onConnect();
                }
                cb(strong_self->_socket->get_local_port(), err);
            } else {
                cb(0, err);
            }
        }, 5.0F, "0.0.0.0", src_port);
    }
}

void RtpSender::onConnect(){
    _is_connect = true;
    //加大发送缓存,防止udp丢包之类的问题
    SockUtil::setSendBuf(_socket->rawFD(), 4 * 1024 * 1024);
    if (!_is_udp) {
        //关闭tcp no_delay并开启MSG_MORE, 提高发送性能
        SockUtil::setNoDelay(_socket->rawFD(), false);
        _socket->setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
    }
    //连接建立成功事件
    weak_ptr<RtpSender> weak_self = shared_from_this();
    _socket->setOnErr([weak_self](const SockException &err) {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->onErr(err);
        }
    });
    //获取本地端口，断开重连后确保端口不变
    _src_port = _socket->get_local_port();
    InfoL << "开始发送 rtp:" << _socket->get_peer_ip() << ":" << _socket->get_peer_port() << ", 是否为udp方式:" << _is_udp;
}

void RtpSender::addTrack(const Track::Ptr &track){
    _interface->addTrack(track);
}

void RtpSender::addTrackCompleted(){
    _interface->addTrackCompleted();
}

void RtpSender::resetTracks(){
    _interface->resetTracks();
}

//此函数在其他线程执行
void RtpSender::inputFrame(const Frame::Ptr &frame) {
    if (_is_connect) {
        //连接成功后才做实质操作(节省cpu资源)
        _interface->inputFrame(frame);
    }
}

//此函数在其他线程执行
void RtpSender::onFlushRtpList(shared_ptr<List<Buffer::Ptr> > rtp_list) {
    if(!_is_connect){
        //连接成功后才能发送数据
        return;
    }

    auto is_udp = _is_udp;
    auto socket = _socket;
    _poller->async([rtp_list, is_udp, socket]() {
        size_t i = 0;
        auto size = rtp_list->size();
        rtp_list->for_each([&](Buffer::Ptr &packet) {
            if (is_udp) {
                //udp模式，rtp over tcp前4个字节可以忽略
                socket->send(std::make_shared<BufferRtp>(std::move(packet), 4), nullptr, 0, ++i == size);
            } else {
                //tcp模式, rtp over tcp前2个字节可以忽略,只保留后续rtp长度的2个字节
                socket->send(std::make_shared<BufferRtp>(std::move(packet), 2), nullptr, 0, ++i == size);
            }
        });
    });
}

void RtpSender::onErr(const SockException &ex, bool is_connect) {
    _is_connect = false;

    //监听socket断开事件，方便重连
    if (is_connect) {
        WarnL << "重连" << _dst_url << ":" << _dst_port << "失败, 原因为:" << ex.what();
    } else {
        WarnL << "停止发送 rtp:" <<  _dst_url << ":" << _dst_port << ", 原因为:" << ex.what();
    }

    weak_ptr<RtpSender> weak_self = shared_from_this();
    _connect_timer = std::make_shared<Timer>(10.0f, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->startSend(strong_self->_dst_url, strong_self->_dst_port, strong_self->_is_udp, strong_self->_src_port, [weak_self](uint16_t local_port, const SockException &ex){
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
#endif// defined(ENABLE_RTPPROXY)