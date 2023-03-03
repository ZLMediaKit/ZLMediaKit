/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcSession.h"
#include "Util/util.h"
#include "Network/TcpServer.h"
#include "Common/config.h"
#include "IceServer.hpp"
#include "WebRtcTransport.h"

using namespace std;

namespace mediakit {

static string getUserName(const char *buf, size_t len) {
    if (!RTC::StunPacket::IsStun((const uint8_t *) buf, len)) {
        return "";
    }
    std::unique_ptr<RTC::StunPacket> packet(RTC::StunPacket::Parse((const uint8_t *) buf, len));
    if (!packet) {
        return "";
    }
    if (packet->GetClass() != RTC::StunPacket::Class::REQUEST ||
        packet->GetMethod() != RTC::StunPacket::Method::BINDING) {
        return "";
    }
    //收到binding request请求
    auto vec = split(packet->GetUsername(), ":");
    return vec[0];
}

EventPoller::Ptr WebRtcSession::queryPoller(const Buffer::Ptr &buffer) {
    auto user_name = getUserName(buffer->data(), buffer->size());
    if (user_name.empty()) {
        return nullptr;
    }
    auto ret = WebRtcTransportManager::Instance().getItem(user_name);
    return ret ? ret->getPoller() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////

WebRtcSession::WebRtcSession(const Socket::Ptr &sock) : Session(sock) {
    _over_tcp = sock->sockType() == SockNum::Sock_TCP;
}

WebRtcSession::~WebRtcSession() {
    InfoP(this);
}

void WebRtcSession::attachServer(const Server &server) {
    _server = std::dynamic_pointer_cast<toolkit::TcpServer>(const_cast<Server &>(server).shared_from_this());
}

void WebRtcSession::onRecv_l(const char *data, size_t len) {
    if (_find_transport) {
        // 只允许寻找一次transport
        _find_transport = false;
        auto user_name = getUserName(data, len);
        auto transport = WebRtcTransportManager::Instance().getItem(user_name);
        CHECK(transport);

        //WebRtcTransport在其他poller线程上，需要切换poller线程并重新创建WebRtcSession对象
        if (!transport->getPoller()->isCurrentThread()) {
            auto sock = Socket::createSocket(transport->getPoller(), false);
            //1、克隆socket(fd不变)，切换poller线程到WebRtcTransport所在线程
            sock->cloneFromPeerSocket(*(getSock()));
            auto server = _server;
            std::string str(data, len);
            sock->getPoller()->async([sock, server, str](){
                auto strong_server = server.lock();
                if (strong_server) {
                    auto session = static_pointer_cast<WebRtcSession>(strong_server->createSession(sock));
                    //2、创建新的WebRtcSession对象(绑定到WebRtcTransport所在线程)，重新处理一遍ice binding request命令
                    session->onRecv_l(str.data(), str.size());
                }
            });
            //3、销毁原先的socket和WebRtcSession(原先的对象跟WebRtcTransport不在同一条线程)
            throw std::runtime_error("webrtc over tcp change poller: " + getPoller()->getThreadName() + " -> " + sock->getPoller()->getThreadName());
        }
        _transport = std::move(transport);
        InfoP(this);
    }
    _ticker.resetTime();
    CHECK(_transport);
    _transport->inputSockData((char *)data, len, this);
}

void WebRtcSession::onRecv(const Buffer::Ptr &buffer) {
    if (_over_tcp) {
        input(buffer->data(), buffer->size());
    } else {
        onRecv_l(buffer->data(), buffer->size());
    }
}

void WebRtcSession::onError(const SockException &err) {
    //udp链接超时，但是rtc链接不一定超时，因为可能存在链接迁移的情况
    //在udp链接迁移时，新的WebRtcSession对象将接管WebRtcTransport对象的生命周期
    //本WebRtcSession对象将在超时后自动销毁
    WarnP(this) << err.what();

    if (!_transport) {
        return;
    }
    auto self = shared_from_this();
    auto transport = std::move(_transport);
    getPoller()->async([transport, self]() mutable {
        //延时减引用，防止使用transport对象时，销毁对象
        transport->removeTuple(self.get());
        //确保transport在Session对象前销毁，防止WebRtcTransport::onDestory()时获取不到Session对象
        transport = nullptr;
    }, false);
}

void WebRtcSession::onManager() {
    GET_CONFIG(float, timeoutSec, Rtc::kTimeOutSec);
    if (!_transport && _ticker.createdTime() > timeoutSec * 1000) {
        shutdown(SockException(Err_timeout, "illegal webrtc connection"));
        return;
    }
    if (_ticker.elapsedTime() > timeoutSec * 1000) {
        shutdown(SockException(Err_timeout, "webrtc connection timeout"));
        return;
    }
}

ssize_t WebRtcSession::onRecvHeader(const char *data, size_t len) {
    onRecv_l(data + 2, len - 2);
    return 0;
}

const char *WebRtcSession::onSearchPacketTail(const char *data, size_t len) {
    if (len < 2) {
        // 数据不够
        return nullptr;
    }
    uint16_t length = (((uint8_t *)data)[0] << 8) | ((uint8_t *)data)[1];
    if (len < (size_t)(length + 2)) {
        // 数据不够
        return nullptr;
    }
    // 返回rtp包末尾
    return data + 2 + length;
}

}// namespace mediakit


