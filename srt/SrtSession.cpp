#include "SrtSession.hpp"
#include "Packet.hpp"
#include "SrtTransportImp.hpp"

#include "Common/config.h"

namespace SRT {
using namespace mediakit;

SrtSession::SrtSession(const Socket::Ptr &sock)
    : Session(sock) {
    socklen_t addr_len = sizeof(_peer_addr);
    memset(&_peer_addr, 0, addr_len);
    // TraceL<<"before addr len "<<addr_len;
    getpeername(sock->rawFD(), (struct sockaddr *)&_peer_addr, &addr_len);
    // TraceL<<"after addr len "<<addr_len<<" family "<<_peer_addr.ss_family;
}

void SrtSession::attachServer(const toolkit::Server &server) {
    SockUtil::setRecvBuf(getSock()->rawFD(), 1024 * 1024);
}

extern SrtTransport::Ptr querySrtTransport(uint8_t *data, size_t size, const EventPoller::Ptr& poller);

EventPoller::Ptr SrtSession::queryPoller(const Buffer::Ptr &buffer) {
    auto transport = querySrtTransport((uint8_t *)buffer->data(), buffer->size(), nullptr);
    return transport ? transport->getPoller() : nullptr;
}

void SrtSession::onRecv(const Buffer::Ptr &buffer) {
    uint8_t *data = (uint8_t *)buffer->data();
    size_t size = buffer->size();

    if (_find_transport) {
        //只允许寻找一次transport
        _find_transport = false;
        _transport = querySrtTransport(data, size, getPoller());
        if (_transport) {
            _transport->setSession(static_pointer_cast<Session>(shared_from_this()));
        }
        InfoP(this);
    }
    _ticker.resetTime();

    if (_transport) {
        _transport->inputSockData(data, size, &_peer_addr);
    } else {
        // WarnL<< "ingore  data";
    }
}

void SrtSession::onError(const SockException &err) {
    // udp链接超时，但是srt链接不一定超时，因为可能存在udp链接迁移的情况
    //在udp链接迁移时，新的SrtSession对象将接管SrtSession对象的生命周期
    //本SrtSession对象将在超时后自动销毁
    WarnP(this) << err;

    if (!_transport) {
        return;
    }

    // 防止互相引用导致不释放
    auto transport = std::move(_transport);
    getPoller()->async(
        [transport] {
            //延时减引用，防止使用transport对象时，销毁对象
            //transport->onShutdown(err);
        },
        false);
}

void SrtSession::onManager() {
    GET_CONFIG(float, timeoutSec, kTimeOutSec);
    if (_ticker.elapsedTime() > timeoutSec * 1000) {
        shutdown(SockException(Err_timeout, "srt connection timeout"));
        return;
    }
}

} // namespace SRT