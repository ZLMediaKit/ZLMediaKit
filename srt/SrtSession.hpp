#ifndef ZLMEDIAKIT_SRT_SESSION_H
#define ZLMEDIAKIT_SRT_SESSION_H

#include "Network/Session.h"
#include "SrtTransport.hpp"

namespace SRT {

using namespace toolkit;

class SrtSession : public UdpSession {
public:
    SrtSession(const Socket::Ptr &sock);
    ~SrtSession() override;

    void onRecv(const Buffer::Ptr &) override;
    void onError(const SockException &err) override;
    void onManager() override;
    void attachServer(const toolkit::Server &server) override;
    static EventPoller::Ptr queryPoller(const Buffer::Ptr &buffer);

private:
    bool _find_transport = true;
    Ticker _ticker;
    struct sockaddr_storage _peer_addr;
    SrtTransport::Ptr _transport;
};

} // namespace SRT
#endif // ZLMEDIAKIT_SRT_SESSION_H