#ifndef ZLMEDIAKIT_QUICSESSION_H
#define ZLMEDIAKIT_QUICSESSION_H

#include "Network/Session.h"
#include "Util/TimeTicker.h"

namespace mediakit {

class QuicSession : public toolkit::Session {
public:
    using Ptr = std::shared_ptr<QuicSession>;

    explicit QuicSession(const toolkit::Socket::Ptr &sock);
    ~QuicSession() override = default;

    static toolkit::EventPoller::Ptr queryPoller(const toolkit::Buffer::Ptr &buffer);

    void attachServer(const toolkit::Server &server) override;
    void onRecv(const toolkit::Buffer::Ptr &buffer) override;
    void onError(const toolkit::SockException &err) override;
    void onManager() override;

private:
    toolkit::Ticker _ticker;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_QUICSESSION_H
