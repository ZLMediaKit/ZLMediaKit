/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "IceSession.hpp"
#include "Util/util.h"
#include "Common/config.h"
#include "WebRtcTransport.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

static IceSession::Ptr queryIceTransport(uint8_t *data, size_t size) {
    auto packet = RTC::StunPacket::parse((const uint8_t *)data, size);
    if (!packet) {
        WarnL << "parse stun error";
        return nullptr;
    }

    auto username = packet->getUsername();
    return IceSessionManager::Instance().getItem(username);
}

////////////  IceSession //////////////////////////
IceSession::IceSession(const Socket::Ptr &sock) : Session(sock) {
    TraceL << getIdentifier();
    _over_tcp = sock->sockType() == SockNum::Sock_TCP;
    GET_CONFIG(string, iceUfrag, Rtc::kIceUfrag);
    GET_CONFIG(string, icePwd, Rtc::kIcePwd);
    _ice_transport = std::make_shared<IceServer>(this, iceUfrag, icePwd, getPoller());
    _ice_transport->initialize();
}

IceSession::~IceSession() {
    TraceL << getIdentifier();
}

EventPoller::Ptr IceSession::queryPoller(const Buffer::Ptr &buffer) {
    auto transport = queryIceTransport((uint8_t *)buffer->data(), buffer->size());
    return transport ? transport->getPoller() : nullptr;
}

void IceSession::onRecv(const Buffer::Ptr &buffer) {
    // TraceL;
    if (_over_tcp) {
        input(buffer->data(), buffer->size());
    }
    else{
        onRecv_l(buffer->data(), buffer->size());
    }
}

void IceSession::onRecv_l(const char* buffer, size_t size) {
    if (!_session_pair) {
        _session_pair = std::make_shared<IceTransport::Pair>(shared_from_this());
    }
    _ice_transport->processSocketData((const uint8_t *)buffer, size, _session_pair);
}

void IceSession::onError(const SockException &err) {
    InfoL;
    // 消除循环引用
    _session_pair = nullptr;
}

void IceSession::onManager() {
}

ssize_t IceSession::onRecvHeader(const char *data, size_t len) {
    onRecv_l(data + 2, len - 2);
    return 0;
}

const char *IceSession::onSearchPacketTail(const char *data, size_t len) {
    if (len < 2) {
        // Not enough data
        return nullptr;
    }
    uint16_t length = (((uint8_t *)data)[0] << 8) | ((uint8_t *)data)[1];
    if (len < (size_t)(length + 2)) {
        // Not enough data
        return nullptr;
    }
    // Return the end of the RTP packet
    return data + 2 + length;
}

void IceSession::onIceTransportRecvData(const toolkit::Buffer::Ptr& buffer, const IceTransport::Pair::Ptr& pair) {
    _ice_transport->processSocketData((const uint8_t *)buffer->data(), buffer->size(), pair);
}

void IceSession::onIceTransportGatheringCandidate(const IceTransport::Pair::Ptr& pair, const CandidateInfo& candidate) {
    DebugL << candidate.dumpString();
}

void IceSession::onIceTransportDisconnected() {
    InfoL << getIdentifier();
}

void IceSession::onIceTransportCompleted() {
    InfoL << getIdentifier();
}

////////////  IceSessionManager //////////////////////////

IceSessionManager &IceSessionManager::Instance() {
    static IceSessionManager s_instance;
    return s_instance;
}

void IceSessionManager::addItem(const std::string& key, const IceSession::Ptr &ptr) {
    std::lock_guard<std::mutex> lck(_mtx);
    _map[key] = ptr;
}

IceSession::Ptr IceSessionManager::getItem(const std::string& key) {
    assert(!key.empty());
    std::lock_guard<std::mutex> lck(_mtx);
    auto it = _map.find(key);
    if (it == _map.end()) {
        return nullptr;
    }
    return it->second.lock();
}

void IceSessionManager::removeItem(const std::string& key) {
    std::lock_guard<std::mutex> lck(_mtx);
    _map.erase(key);
}

}// namespace mediakit
