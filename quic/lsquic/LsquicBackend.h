#ifndef ZLMEDIAKIT_LSQUICBACKEND_H
#define ZLMEDIAKIT_LSQUICBACKEND_H

#include "../QuicBackend.h"

namespace mediakit {
namespace quic {

class LsquicBackend final : public QuicBackend {
public:
    toolkit::EventPoller::Ptr queryPoller(const toolkit::Buffer::Ptr &buffer) override;
    void inputPacket(const QuicPacket &packet) override;
    void onManager(const toolkit::EventPoller::Ptr &poller) override;
};

} // namespace quic
} // namespace mediakit

#endif // ZLMEDIAKIT_LSQUICBACKEND_H
