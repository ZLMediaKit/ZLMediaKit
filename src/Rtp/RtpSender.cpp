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
#include "Util/uv_errno.h"
#include "RtpCache.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

RtpSender::RtpSender() {
    _poller = EventPollerPool::Instance().getPoller();
    _socket = Socket::createSocket(_poller, false);
}

void RtpSender::startSend(const MediaSourceEvent::SendRtpArgs &args, const function<void(uint16_t local_port, const SockException &ex)> &cb){
    _args = args;
    if (!_interface) {
        //重连时不重新创建对象
        auto lam = [this](std::shared_ptr<List<Buffer::Ptr>> list) { onFlushRtpList(std::move(list)); };
        if (args.use_ps) {
            _interface = std::make_shared<RtpCachePS>(lam, atoi(args.ssrc.data()), args.pt);
        } else {
            _interface = std::make_shared<RtpCacheRaw>(lam, atoi(args.ssrc.data()), args.pt, args.only_audio);
        }
    }

    weak_ptr<RtpSender> weak_self = shared_from_this();
    if (args.passive) {
        // tcp被动发流模式
        _args.is_udp = false;
        try {
            auto tcp_listener = Socket::createSocket(_poller, false);
            if (args.src_port) {
                //指定端口
                if (!tcp_listener->listen(args.src_port)) {
                    throw std::invalid_argument(StrPrinter << "open tcp passive server failed on port:" << args.src_port
                                                           << ", err:" << get_uv_errmsg(true));
                }
            } else {
                auto pr = std::make_pair(tcp_listener, Socket::createSocket(_poller, false));
                //从端口池获取随机端口
                makeSockPair(pr, "::", false, false);
            }
            // tcp服务器默认开启5秒
            auto delay_task = _poller->doDelayTask(5 * 1000, [tcp_listener, cb]() mutable {
                cb(0, SockException(Err_timeout, "wait tcp connection timeout"));
                tcp_listener = nullptr;
                return 0;
            });
            tcp_listener->setOnAccept([weak_self, cb, delay_task](Socket::Ptr &sock, std::shared_ptr<void> &complete) {
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    return;
                }
                //立即关闭tcp服务器
                delay_task->cancel();
                strong_self->_socket = sock;
                strong_self->onConnect();
                cb(sock->get_local_port(), SockException());
                InfoL << "accept connection from:" << sock->get_peer_ip() << ":" << sock->get_peer_port();
            });
            InfoL << "start tcp passive server on:" << tcp_listener->get_local_port();
        } catch (std::exception &ex) {
            cb(0, SockException(Err_other, ex.what()));
            return;
        }
        return;
    }
    if (args.is_udp) {
        auto poller = _poller;
        WorkThreadPool::Instance().getPoller()->async([cb, args, weak_self, poller]() {
            struct sockaddr_storage addr;
            //切换线程目的是为了dns解析放在后台线程执行
            if (!SockUtil::getDomainIP(args.dst_url.data(), args.dst_port, addr, AF_INET, SOCK_DGRAM, IPPROTO_UDP)) {
                poller->async([args, cb]() {
                    //切回自己的线程
                    cb(0, SockException(Err_dns, StrPrinter << "dns解析域名失败:" << args.dst_url));
                });
                return;
            }

            //dns解析成功
            poller->async([args, addr, weak_self, cb]() {
                //切回自己的线程
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    return;
                }
                string ifr_ip = addr.ss_family == AF_INET ? "0.0.0.0" : "::";
                try {
                    if (args.src_port) {
                        //指定端口
                        if (!strong_self->_socket->bindUdpSock(args.src_port, ifr_ip)) {
                            throw std::invalid_argument(StrPrinter << "bindUdpSock failed on port:" << args.src_port
                                                                   << ", err:" << get_uv_errmsg(true));
                        }
                    } else {
                        auto pr = std::make_pair(strong_self->_socket, Socket::createSocket(strong_self->_poller, false));
                        //从端口池获取随机端口
                        makeSockPair(pr, ifr_ip, true);
                    }
                } catch (std::exception &ex) {
                    cb(0, SockException(Err_other, ex.what()));
                    return;
                }
                strong_self->_socket->bindPeerAddr((struct sockaddr *)&addr);
                strong_self->onConnect();
                cb(strong_self->_socket->get_local_port(), SockException());
            });
        });
    } else {
        _socket->connect(args.dst_url, args.dst_port, [cb, weak_self](const SockException &err) {
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
        }, 5.0F, "::", args.src_port);
    }
}

void RtpSender::onConnect(){
    _is_connect = true;
    //加大发送缓存,防止udp丢包之类的问题
    SockUtil::setSendBuf(_socket->rawFD(), 4 * 1024 * 1024);
    if (!_args.is_udp) {
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
    _args.src_port = _socket->get_local_port();
    InfoL << "开始发送 rtp:" << _socket->get_peer_ip() << ":" << _socket->get_peer_port() << ", 是否为udp方式:" << _args.is_udp;
}

bool RtpSender::addTrack(const Track::Ptr &track){
    return _interface->addTrack(track);
}

void RtpSender::addTrackCompleted(){
    _interface->addTrackCompleted();
}

void RtpSender::resetTracks(){
    _interface->resetTracks();
}

//此函数在其他线程执行
bool RtpSender::inputFrame(const Frame::Ptr &frame) {
    //连接成功后才做实质操作(节省cpu资源)
    return _is_connect ? _interface->inputFrame(frame) : false;
}

//此函数在其他线程执行
void RtpSender::onFlushRtpList(shared_ptr<List<Buffer::Ptr> > rtp_list) {
    if(!_is_connect){
        //连接成功后才能发送数据
        return;
    }

    auto is_udp = _args.is_udp;
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

    if (_args.passive) {
        WarnL << "tcp passive connection lost: " << ex.what();
    } else {
        //监听socket断开事件，方便重连
        if (is_connect) {
            WarnL << "重连" << _args.dst_url << ":" << _args.dst_port << "失败, 原因为:" << ex.what();
        } else {
            WarnL << "停止发送 rtp:" << _args.dst_url << ":" << _args.dst_port << ", 原因为:" << ex.what();
        }
    }

    weak_ptr<RtpSender> weak_self = shared_from_this();
    _connect_timer = std::make_shared<Timer>(10.0f, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->startSend(strong_self->_args, [weak_self](uint16_t local_port, const SockException &ex){
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