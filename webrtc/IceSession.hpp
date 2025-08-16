/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */


#ifndef ZLMEDIAKIT_WEBRTC_ICE_SESSION_H
#define ZLMEDIAKIT_WEBRTC_ICE_SESSION_H

#include "Network/Session.h"
#include "IceTransport.hpp"

using namespace toolkit;
using namespace RTC;
namespace mediakit {

class IceSession : public Session, public RTC::IceTransport::Listener {
public:
    using Ptr = std::shared_ptr<IceSession>;
    using WeakPtr = std::weak_ptr<IceSession>;
    IceSession(const Socket::Ptr &sock);
    ~IceSession() override {DebugL;};

    static EventPoller::Ptr queryPoller(const Buffer::Ptr &buffer);

    //// Session override////
    // void attachServer(const Server &server) override;
    void onRecv(const Buffer::Ptr &) override;
    void onError(const SockException &err) override;
    void onManager() override;

    // ice related callbacks ///
    void onIceTransportRecvData(const toolkit::Buffer::Ptr& buffer, IceTransport::Pair::Ptr pair) override;
    void onIceTransportGatheringCandidate(IceTransport::Pair::Ptr pair, CandidateInfo candidate) override;
    void onIceTransportDisconnected() override;
    void onIceTransportCompleted() override;

protected:
    IceServer::Ptr _ice_transport;
};

class IceSessionManager {
public:
    static IceSessionManager &Instance();
    IceSession::Ptr getItem(const std::string& key);
    void addItem(const std::string& key, const IceSession::Ptr &ptr);
    void removeItem(const std::string& key);

private:
    IceSessionManager() = default;

private:
    std::mutex _mtx;
    std::unordered_map<std::string, std::weak_ptr<IceSession>> _map;
};
}// namespace mediakit

#endif //ZLMEDIAKIT_WEBRTC_ICE_SESSION_H
