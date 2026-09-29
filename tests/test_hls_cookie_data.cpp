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

} // namespace

int main() {
    return testCookieDoesNotOwnSocket() ? 0 : 1;
}
