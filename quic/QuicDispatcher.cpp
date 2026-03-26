#include "QuicDispatcher.h"

#include "QuicPluginBackend.h"

namespace mediakit {

QuicDispatcher::QuicDispatcher() {
    _backend = std::make_shared<QuicPluginBackend>();
}

QuicDispatcher &QuicDispatcher::Instance() {
    static QuicDispatcher instance;
    return instance;
}

toolkit::EventPoller::Ptr QuicDispatcher::queryPoller(const toolkit::Buffer::Ptr &buffer) {
    if (!_backend) {
        return nullptr;
    }
    return _backend->queryPoller(buffer);
}

void QuicDispatcher::inputPacket(const QuicPacket &packet) {
    if (_backend) {
        _backend->inputPacket(packet);
    }
}

void QuicDispatcher::onManager(const toolkit::EventPoller::Ptr &poller) {
    if (_backend) {
        _backend->onManager(poller);
    }
}

} // namespace mediakit
