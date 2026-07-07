#ifndef ZLMEDIAKIT_QUICSOCKETBUFFERCONFIG_H
#define ZLMEDIAKIT_QUICSOCKETBUFFERCONFIG_H

#include "Network/BufferSock.h"
#include "Network/Socket.h"

namespace mediakit {

static constexpr size_t kQuicRecvPacketCount = 32;
static constexpr size_t kQuicRecvBufferCapacity = 64 * 1024u;

inline toolkit::SocketRecvBuffer::Ptr makeQuicSocketReadBuffer() {
    return toolkit::SocketRecvBuffer::create(true, kQuicRecvPacketCount, kQuicRecvBufferCapacity);
}

inline void configureQuicSocket(const toolkit::Socket::Ptr &sock) {
    if (!sock) {
        return;
    }
    // Keep the customization explicit and creation-time only so ordinary
    // ZLToolKit users stay on the default shared-buffer path.
    sock->setUdpRecvBuffer(makeQuicSocketReadBuffer());
}

inline void configureQuicServerSocket(const toolkit::Socket::Ptr &sock) {
    configureQuicSocket(sock);
    // Server-side connected UDP sessions can receive late ICMP port-unreachable
    // noise after a QUIC peer has already gone away. Client sockets keep the
    // default behavior so connection-refused still surfaces quickly.
    sock->setIgnoreUdpConnRefused(true);
}

} // namespace mediakit

#endif // ZLMEDIAKIT_QUICSOCKETBUFFERCONFIG_H
