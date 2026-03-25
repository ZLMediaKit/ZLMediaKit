#include "QuicSession.h"

#include "Common/config.h"
#include "QuicDispatcher.h"

using namespace toolkit;

namespace mediakit {

QuicSession::QuicSession(const Socket::Ptr &sock) : Session(sock) {}

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
