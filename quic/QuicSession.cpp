#include "QuicSession.h"

#include "Common/config.h"
#include "QuicDispatcher.h"
#include "QuicSocketBufferConfig.h"

using namespace toolkit;

namespace mediakit {

QuicSession::QuicSession(const Socket::Ptr &sock) : Session(sock) {}

void QuicSession::attachServer(const toolkit::Server &) {
    if (auto sock = getSock()) {
        // Public QUIC traffic can arrive through UDP GRO/coalescing and exceed the
        // default shared userspace packet buffer even when the logical QUIC packets
        // are valid, so QUIC installs a larger recv path on its own sockets only.
        // `attachServer()` runs before this UDP session starts receiving packets,
        // which matches the setup-time contract of Socket::setReadBuffer().
        sock->setReadBuffer(makeQuicSocketReadBuffer());
    }
}

EventPoller::Ptr QuicSession::queryPoller(const Buffer::Ptr &buffer) {
    return QuicDispatcher::Instance().queryPoller(buffer);
}

void QuicSession::onRecv(const Buffer::Ptr &buffer) {
    _ticker.resetTime();
    if (!buffer) {
        return;
    }

    QuicPacket packet;
    packet.buffer = buffer;
    packet.sock = getSock();
    packet.poller = getPoller();
    packet.local_ip = get_local_ip();
    packet.peer_ip = get_peer_ip();
    packet.local_port = get_local_port();
    packet.peer_port = get_peer_port();
    QuicDispatcher::Instance().inputPacket(packet);
}

void QuicSession::onError(const SockException &err) {
    WarnP(this) << err;
}

void QuicSession::onManager() {
    GET_CONFIG(uint32_t, keepAliveSec, Http::kKeepAliveSecond);
    QuicDispatcher::Instance().onManager(getPoller());
    if (_ticker.elapsedTime() > keepAliveSec * 1000) {
        shutdown(SockException(Err_timeout, "quic session timeout"));
    }
}

} // namespace mediakit
