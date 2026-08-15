/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Common/config.h"
#include "Network/Socket.h"
#include "Poller/EventPoller.h"
#include "Util/NoticeCenter.h"
#include "../webrtc/IceTransport.hpp"

using namespace std;
using namespace toolkit;
using namespace RTC;
using namespace mediakit;

namespace {
void expect(bool cond, const string &message) {
    if (!cond) {
        throw runtime_error(message);
    }
}

class TestSocketHelper : public SocketHelper {
public:
    explicit TestSocketHelper(const Socket::Ptr &socket) : SocketHelper(socket) {}

    void onRecv(const Buffer::Ptr &) override {}
    void onError(const SockException &) override {}
    void onManager() override {}
};

class TestIceServer : public IceServer {
public:
    explicit TestIceServer(const EventPoller::Ptr &poller)
        : IceServer(nullptr, "testUfrag", "testPassword", poller) {}

    using IceServer::handleAllocateRequest;
    using IceServer::handleRefreshRequest;

    void sendSocketData(const Buffer::Ptr &buf, const Pair::Ptr &, bool = true) override {
        auto packet = StunPacket::parse(reinterpret_cast<const uint8_t *>(buf->data()), buf->size());
        expect(packet != nullptr, "handler must send a serialized STUN response");
        _responses.emplace_back(std::move(packet));
    }

    void seedAllocation(const Pair::Ptr &pair, const string &transaction_id, uint64_t update_time) {
        sockaddr_storage peer_addr;
        pair->get_peer_addr(peer_addr);
        auto socket = Socket::createSocket(getPoller());
        expect(socket->bindUdpSock(0, "127.0.0.1"), "test relay socket should bind locally");
        auto relay_socket = make_shared<TestSocketHelper>(std::move(socket));
        auto relay_pair = make_shared<Pair>(relay_socket);
        _relayed_pairs.emplace(peer_addr, make_pair(shared_ptr<uint16_t>(), relay_pair));
        _allocation_transaction_id = transaction_id;
        _allocation_update_time = update_time;
    }

    const StunPacket::Ptr &response() const {
        expect(_responses.size() == 1, "handler must send exactly one response");
        return _responses.front();
    }
    const string &allocationTransactionId() const { return _allocation_transaction_id; }
    uint64_t allocationUpdateTime() const { return _allocation_update_time; }
    size_t allocationCount() const { return _relayed_pairs.size(); }
    SocketHelper::Ptr relaySocket(const Pair::Ptr &pair) const {
        auto peer_addr = SockUtil::make_sockaddr(pair->get_peer_ip().data(), pair->get_peer_port());
        auto it = _relayed_pairs.find(peer_addr);
        return it == _relayed_pairs.end() ? nullptr : it->second.second->_socket;
    }

private:
    vector<StunPacket::Ptr> _responses;
};

IceTransport::Pair::Ptr makePair(const EventPoller::Ptr &poller, const string &ip, uint16_t port) {
    auto socket = make_shared<TestSocketHelper>(Socket::createSocket(poller));
    return make_shared<IceTransport::Pair>(socket, ip, port);
}

StunPacket::Ptr makeRequest(StunPacket::Method method, const char *transaction_id, int lifetime = -1) {
    auto packet = make_shared<StunPacket>(StunPacket::Class::REQUEST, method, transaction_id);
    if (lifetime >= 0) {
        auto attr = make_shared<StunAttrLifeTime>();
        attr->setLifetime(static_cast<uint32_t>(lifetime));
        packet->addAttribute(std::move(attr));
    }
    return packet;
}

void expectResponse(const StunPacket::Ptr &response, StunPacket::Class klass,
                    StunPacket::Method method, StunAttrErrorCode::Code error = StunAttrErrorCode::Code::Invalid) {
    expect(response->getClass() == klass, "unexpected STUN response class");
    expect(response->getMethod() == method, "unexpected STUN response method");
    expect(response->getErrorCode() == error, "unexpected STUN error code");
}

void testAllocateHandlers() {
    auto poller = EventPollerPool::Instance().getPoller();
    auto server = make_shared<TestIceServer>(poller);
    auto owner = makePair(poller, "127.0.0.1", 40000);
    const string original_id = "allocate0001";

    server->handleAllocateRequest(makeRequest(StunPacket::Method::ALLOCATE, original_id.c_str()), owner);
    expectResponse(server->response(), StunPacket::Class::SUCCESS_RESPONSE, StunPacket::Method::ALLOCATE);
    expect(server->allocationCount() == 1, "first Allocate should create one relay allocation");
    expect(server->allocationTransactionId() == original_id,
           "successful first Allocate should record its transaction ID");
    auto original_socket = server->relaySocket(owner);
    auto original_time = server->allocationUpdateTime();

    auto retransmission = make_shared<TestIceServer>(poller);
    retransmission->seedAllocation(owner, original_id, original_time);
    auto seeded_socket = retransmission->relaySocket(owner);
    retransmission->handleAllocateRequest(makeRequest(StunPacket::Method::ALLOCATE, original_id.c_str()), owner);
    expectResponse(retransmission->response(), StunPacket::Class::SUCCESS_RESPONSE, StunPacket::Method::ALLOCATE);
    expect(retransmission->relaySocket(owner) == seeded_socket,
           "Allocate retransmission should reuse the existing relay socket");
    expect(retransmission->allocationUpdateTime() == original_time,
           "Allocate retransmission must not refresh allocation lifetime");

    auto mismatch = make_shared<TestIceServer>(poller);
    mismatch->seedAllocation(owner, original_id, original_time);
    mismatch->handleAllocateRequest(makeRequest(StunPacket::Method::ALLOCATE, "allocate0002"), owner);
    expectResponse(mismatch->response(), StunPacket::Class::ERROR_RESPONSE, StunPacket::Method::ALLOCATE,
                   StunAttrErrorCode::Code::AllocationMismatch);
    expect(mismatch->allocationCount() == 1 && mismatch->relaySocket(owner) != nullptr,
           "mismatched Allocate must preserve the allocation");
    expect(mismatch->allocationUpdateTime() == original_time && mismatch->allocationTransactionId() == original_id,
           "mismatched Allocate must preserve lifetime and transaction ID");
    expect(original_socket != nullptr, "first Allocate should expose a live relay socket");
}

void testRefreshHandlers() {
    auto poller = EventPollerPool::Instance().getPoller();
    auto owner = makePair(poller, "127.0.0.1", 40001);
    auto other = makePair(poller, "127.0.0.1", 40002);
    const string allocation_id = "allocate0003";
    const uint64_t original_time = UINT64_MAX;

    for (int lifetime : {600, 0}) {
        auto server = make_shared<TestIceServer>(poller);
        server->seedAllocation(owner, allocation_id, original_time);
        server->handleRefreshRequest(makeRequest(StunPacket::Method::REFRESH,
                                                 lifetime ? "refresh00001" : "refresh00002", lifetime), other);
        expectResponse(server->response(), StunPacket::Class::ERROR_RESPONSE, StunPacket::Method::REFRESH,
                       StunAttrErrorCode::Code::AllocationMismatch);
        expect(server->allocationCount() == 1, "non-owner Refresh must not release the allocation");
        expect(server->allocationUpdateTime() == original_time,
               "non-owner Refresh must not refresh allocation lifetime");
    }

    auto refresh = make_shared<TestIceServer>(poller);
    refresh->seedAllocation(owner, allocation_id, original_time);
    refresh->handleRefreshRequest(makeRequest(StunPacket::Method::REFRESH, "refresh00003", 600), owner);
    expectResponse(refresh->response(), StunPacket::Class::SUCCESS_RESPONSE, StunPacket::Method::REFRESH);
    expect(refresh->allocationUpdateTime() != original_time, "owner Refresh should update allocation lifetime");
    expect(refresh->allocationCount() == 1, "owner Refresh should preserve the allocation");

    auto remove = make_shared<TestIceServer>(poller);
    remove->seedAllocation(owner, allocation_id, original_time);
    remove->handleRefreshRequest(makeRequest(StunPacket::Method::REFRESH, "refresh00004", 0), owner);
    expectResponse(remove->response(), StunPacket::Class::SUCCESS_RESPONSE, StunPacket::Method::REFRESH);
    expect(remove->allocationCount() == 0, "owner zero-lifetime Refresh should release the allocation");
    expect(remove->allocationTransactionId().empty(),
           "owner zero-lifetime Refresh should clear the Allocate transaction ID");
}

void testAllocateCapacityFailure() {
    auto poller = EventPollerPool::Instance().getPoller();
    vector<TestIceServer::Ptr> allocations;
    for (uint16_t index = 0; index < 36; ++index) {
        auto server = make_shared<TestIceServer>(poller);
        auto pair = makePair(poller, "127.0.0.1", static_cast<uint16_t>(41000 + index));
        server->handleAllocateRequest(makeRequest(StunPacket::Method::ALLOCATE, "fillport0001"), pair);
        expect(server->hasAllocation(), "test setup should consume every configured relay port");
        allocations.emplace_back(std::move(server));
    }

    auto server = make_shared<TestIceServer>(poller);
    auto pair = makePair(poller, "127.0.0.1", 42000);
    server->handleAllocateRequest(makeRequest(StunPacket::Method::ALLOCATE, "allocate0004"), pair);
    expectResponse(server->response(), StunPacket::Class::ERROR_RESPONSE, StunPacket::Method::ALLOCATE,
                   StunAttrErrorCode::Code::InsuficientCapacity);
    expect(!server->hasAllocation(), "capacity failure must not create an allocation");
    expect(server->allocationTransactionId().empty(), "capacity failure must not record a transaction ID");
}
} // namespace

int main() {
    try {
        mINI::Instance()["rtc.port_range"] = "61000-61036";
        NOTICE_EMIT(BroadcastReloadConfigArgs, Broadcast::kBroadcastReloadConfig);

        testAllocateHandlers();
        testRefreshHandlers();
        testAllocateCapacityFailure();
        cout << "test_turn_allocation passed" << endl;
        return 0;
    } catch (const exception &ex) {
        cerr << "test_turn_allocation failed: " << ex.what() << endl;
        return EXIT_FAILURE;
    }
}
