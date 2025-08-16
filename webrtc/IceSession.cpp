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
#include "server/WebApi.h"
#include "WebRtcTransport.h"

using namespace std;

namespace mediakit {

IceSession::Ptr queryIceTransport(uint8_t *data, size_t size, const EventPoller::Ptr& poller) {
    auto packet = RTC::StunPacket::parse((const uint8_t *)data, size);
    if (!packet) {
        WarnL << "parse stun error";
        return nullptr;
    }

    auto username = packet->getUsername();
    return IceSessionManager::Instance().getItem(username);
}

////////////  IceSession //////////////////////////
///
IceSession::IceSession(const Socket::Ptr &sock) : Session(sock) {
    TraceL;

    GET_CONFIG(string, iceUfrag, Rtc::kIceUfrag);
    GET_CONFIG(string, icePwd, Rtc::kIcePwd);
    _ice_transport = std::make_shared<IceServer>(this, iceUfrag, icePwd, getPoller());
    _ice_transport->initialize();
}

EventPoller::Ptr IceSession::queryPoller(const Buffer::Ptr &buffer) {
    auto transport = queryIceTransport((uint8_t *)buffer->data(), buffer->size(), nullptr);
    return transport ? transport->getPoller() : nullptr;
}

void IceSession::onRecv(const Buffer::Ptr &buffer) {
    // TraceL;
    auto pair = std::make_shared<IceTransport::Pair>(shared_from_this());
    _ice_transport->processSocketData((const uint8_t *)buffer->data(), buffer->size(), pair);
    return;
}

void IceSession::onError(const SockException &err) {
    InfoL;
    return;
}

void IceSession::onManager() {
    return;
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

void IceSession::onIceTransportRecvData(const toolkit::Buffer::Ptr& buffer, IceTransport::Pair::Ptr pair) {
    _ice_transport->processSocketData((const uint8_t *)buffer->data(), buffer->size(), pair);
}

void IceSession::onIceTransportGatheringCandidate(IceTransport::Pair::Ptr pair, CandidateInfo candidate) {
    DebugL;
}

void IceSession::onIceTransportDisconnected() {
    InfoL << getIdentifier();
}

void IceSession::onIceTransportCompleted() {
    InfoL << getIdentifier();
}



}// namespace mediakit
