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

WebRtcSession::WebRtcSession(const Socket::Ptr &sock) : UdpSession(sock) {
    socklen_t addr_len = sizeof(_peer_addr);
    getpeername(sock->rawFD(), &_peer_addr, &addr_len);
    InfoP(this);
}

WebRtcSession::~WebRtcSession() {
    InfoP(this);
}

void WebRtcSession::onRecv(const Buffer::Ptr &buffer) {
    auto buf = buffer->data();
    auto len = buffer->size();

    if (!_transport && RTC::StunPacket::IsStun((const uint8_t *) buf, len)) {
        std::unique_ptr<RTC::StunPacket> packet(RTC::StunPacket::Parse((const uint8_t *) buf, len));
        if (!packet) {
            WarnL << "parse stun error";
            return;
        }
        if (packet->GetClass() == RTC::StunPacket::Class::REQUEST &&
            packet->GetMethod() == RTC::StunPacket::Method::BINDING) {
            //收到binding request请求
            _transport = createTransport(packet->GetUsername());
        }
    }

    if (_transport) {
        _transport->inputSockData(buf, len, &_peer_addr);
    }
}

void WebRtcSession::onError(const SockException &err) {
    if (_transport) {
        _transport->unrefSelf(err);
        _transport = nullptr;
    }
}

void WebRtcSession::onManager() {

}

std::shared_ptr<WebRtcTransport> WebRtcSession::createTransport(const string &user_name) {
    if (user_name.empty()) {
        return nullptr;
    }
    auto vec = split(user_name, ":");
    auto ret = WebRtcTransportImp::getTransport(vec[0]);
    ret->setSession(this);
    return ret;
}
