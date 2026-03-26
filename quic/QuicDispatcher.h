#ifndef ZLMEDIAKIT_QUICDISPATCHER_H
#define ZLMEDIAKIT_QUICDISPATCHER_H

#include "QuicBackend.h"

namespace mediakit {

class QuicDispatcher {
public:
    static QuicDispatcher &Instance();

    toolkit::EventPoller::Ptr queryPoller(const toolkit::Buffer::Ptr &buffer);
    void inputPacket(const QuicPacket &packet);
    void onManager(const toolkit::EventPoller::Ptr &poller);

private:
    QuicDispatcher();

private:
    QuicBackend::Ptr _backend;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_QUICDISPATCHER_H
