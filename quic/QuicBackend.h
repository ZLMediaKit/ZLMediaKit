#ifndef ZLMEDIAKIT_QUICBACKEND_H
#define ZLMEDIAKIT_QUICBACKEND_H

#include <memory>
#include <string>

#include "Network/Buffer.h"
#include "Network/Socket.h"
#include "Poller/EventPoller.h"

namespace mediakit {

struct QuicPacket {
    toolkit::Buffer::Ptr buffer;
    toolkit::Socket::Ptr sock;
    toolkit::EventPoller::Ptr poller;
    std::string local_ip;
    std::string peer_ip;
    uint16_t local_port = 0;
    uint16_t peer_port = 0;
};

class QuicBackend {
public:
    using Ptr = std::shared_ptr<QuicBackend>;
    virtual ~QuicBackend() = default;

    virtual toolkit::EventPoller::Ptr queryPoller(const toolkit::Buffer::Ptr &buffer) = 0;
    virtual void inputPacket(const QuicPacket &packet) = 0;
    virtual void onManager(const toolkit::EventPoller::Ptr &poller) = 0;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_QUICBACKEND_H
