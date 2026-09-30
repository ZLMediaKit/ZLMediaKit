/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <memory>

#include "Network/Session.h"
#include "Record/HlsMediaSource.h"

using namespace mediakit;
using namespace toolkit;

namespace {

class TestSession : public Session {
public:
    explicit TestSession(const Socket::Ptr &sock) : Session(sock) {}

    std::string get_local_ip() override { return "192.0.2.1"; }
    uint16_t get_local_port() override { return 8080; }
    std::string get_peer_ip() override { return "198.51.100.1"; }
    uint16_t get_peer_port() override { return 54321; }
    std::string getIdentifier() const override { return "hls-cookie-test"; }

    void onRecv(const Buffer::Ptr &buf) override {}
    void onError(const SockException &err) override {}
    void onManager() override {}
};

bool testCookieDoesNotOwnSocket() {
    auto socket = Socket::createSocket();
    std::weak_ptr<Socket> weak_socket = socket;
    auto session = std::make_shared<TestSession>(socket);
    socket.reset();

    auto cookie_data = std::make_shared<HlsCookieData>(MediaInfo(), session);
    session.reset();

    // HLS cookie metadata may outlive the HTTP session, but must not extend the
    // lifetime of its network socket. Retaining it delays descriptor closure.
    if (!weak_socket.expired()) {
        std::cerr << "[FAIL] HlsCookieData retained the HTTP session socket" << std::endl;
        return false;
    }

    std::cout << "[ OK ] HlsCookieData retains only copied socket metadata" << std::endl;
    return true;
}

bool testPlayerInfoPreservesSession() {
    auto poller = EventPollerPool::Instance().getPoller();
    auto socket = Socket::createSocket(poller);
    std::weak_ptr<Socket> weak_socket = socket;
    auto session = std::make_shared<TestSession>(socket);
    socket.reset();

    MediaInfo info("hls://127.0.0.1/test/cookie-player-info");
    auto source = std::make_shared<HlsMediaSource>(info.schema, info);
    source->setIndexFile("#EXTM3U\n");
    auto cookie_data = std::make_shared<HlsCookieData>(info, session);
    cookie_data->setMediaSource(source);
    cookie_data->addByteUsage(1);
    session.reset();

    std::list<Any> players;
    bool called = false;
    source->getPlayerList([&](const std::list<Any> &info_list) {
        players = info_list;
        called = true;
    }, [](Any &&player) { return std::move(player); });

    // The reader and query run on the same poller, so the callback is synchronous.
    if (!called || players.size() != 1 || !players.front().is<Session>()) {
        std::cerr << "[FAIL] HLS player info must remain Any<Session>" << std::endl;
        return false;
    }

    auto &player = players.front().get<Session>();
    if (player.getSock() || !weak_socket.expired()) {
        std::cerr << "[FAIL] HLS player metadata retained the HTTP socket" << std::endl;
        return false;
    }
    if (player.getIdentifier() != "hls-cookie-test" || player.get_local_ip() != "192.0.2.1"
        || player.get_local_port() != 8080 || player.get_peer_ip() != "198.51.100.1"
        || player.get_peer_port() != 54321) {
        std::cerr << "[FAIL] HLS player metadata changed after HTTP session destruction" << std::endl;
        return false;
    }
    if (player.getPoller() != poller) {
        std::cerr << "[FAIL] HLS player metadata lost its poller" << std::endl;
        return false;
    }

    std::cout << "[ OK ] HLS player info preserves Session and copied metadata without a socket" << std::endl;
    return true;
}

} // namespace

int main() {
    EventPollerPool::setPoolSize(1);
    bool passed = false;
    EventPollerPool::Instance().getPoller()->sync([&]() {
        try {
            bool ownership = testCookieDoesNotOwnSocket();
            bool player_info = testPlayerInfoPreservesSession();
            passed = ownership && player_info;
        } catch (const std::exception &ex) {
            std::cerr << "[FAIL] " << ex.what() << std::endl;
        }
    });
    return passed ? 0 : 1;
}
