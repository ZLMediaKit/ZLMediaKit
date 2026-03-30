#ifndef ZLMEDIAKIT_QUICSOCKETBUFFERCONFIG_H
#define ZLMEDIAKIT_QUICSOCKETBUFFERCONFIG_H

#include "Network/BufferSock.h"

namespace mediakit {

static constexpr size_t kQuicRecvPacketCount = 32;
static constexpr size_t kQuicRecvBufferCapacity = 64 * 1024u;

inline toolkit::SocketRecvBuffer::Ptr makeQuicSocketReadBuffer() {
    return toolkit::SocketRecvBuffer::create(true, kQuicRecvPacketCount, kQuicRecvBufferCapacity);
}

} // namespace mediakit

#endif // ZLMEDIAKIT_QUICSOCKETBUFFERCONFIG_H
