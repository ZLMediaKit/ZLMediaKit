#include "LsquicBackend.h"

#include "Util/logger.h"
#include "Util/onceToken.h"

namespace mediakit {
namespace quic {

toolkit::EventPoller::Ptr LsquicBackend::queryPoller(const toolkit::Buffer::Ptr &) {
    return nullptr;
}

void LsquicBackend::inputPacket(const QuicPacket &packet) {
    static toolkit::onceToken token([]() {
        WarnL << "QUIC listener is enabled, but the LSQUIC backend is only scaffolded in this branch";
    });
    (void) token;
    TraceL << "recv quic packet from " << packet.peer_ip << ":" << packet.peer_port
           << ", size=" << (packet.buffer ? packet.buffer->size() : 0);
}

void LsquicBackend::onManager(const toolkit::EventPoller::Ptr &) {}

} // namespace quic
} // namespace mediakit
