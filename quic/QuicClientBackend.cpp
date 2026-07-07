#include "QuicClientBackend.h"

#include "Network/Buffer.h"
#include "Network/sockutil.h"
#include "QuicSocketBufferConfig.h"
#include "Util/logger.h"
#include "Util/util.h"

namespace mediakit {

using namespace toolkit;

namespace {

std::string quicSliceToString(QuicSlice slice) {
    if (!slice.data || !slice.len) {
        return std::string();
    }
    return std::string(slice.data, slice.len);
}

QuicSlice makeSlice(const std::string &str) {
    QuicSlice slice;
    slice.data = str.data();
    slice.len = str.size();
    return slice;
}

QuicPacketView makePacketView(const Buffer::Ptr &buf, const std::string &local_ip,
                              uint16_t local_port, const std::string &peer_ip, uint16_t peer_port) {
    QuicPacketView packet;
    packet.payload.data = reinterpret_cast<const uint8_t *>(buf->data());
    packet.payload.len = buf->size();
    packet.local_ip.data = local_ip.data();
    packet.local_ip.len = local_ip.size();
    packet.peer_ip.data = peer_ip.data();
    packet.peer_ip.len = peer_ip.size();
    packet.local_port = local_port;
    packet.peer_port = peer_port;
    return packet;
}

std::pair<std::string, uint16_t> getSocketLocalEndpoint(const Socket::Ptr &sock) {
    if (!sock) {
        return std::make_pair(std::string(), uint16_t(0));
    }

    auto fd = sock->rawFD();
    if (fd >= 0) {
        auto local_ip = SockUtil::get_local_ip(fd);
        auto local_port = SockUtil::get_local_port(fd);
        if (!local_ip.empty() && local_port) {
            return std::make_pair(std::move(local_ip), local_port);
        }
    }
    return std::make_pair(sock->get_local_ip(), sock->get_local_port());
}

void detachSocketCallbacks(const std::shared_ptr<toolkit::UdpClient> &transport) {
    if (!transport) {
        return;
    }
    auto sock = transport->getSock();
    if (!sock) {
        return;
    }
    sock->setOnRead(nullptr);
    sock->setOnErr(nullptr);
    sock->setOnFlush(nullptr);
    sock->setOnSendResult(nullptr);
}

bool resolveUdpEndpoints(const std::string &host, uint16_t port,
                         std::vector<QuicClientPeerEndpoint> &endpoints) {
    endpoints.clear();

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo *result = nullptr;
    auto port_text = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0 || !result) {
        return false;
    }

    for (auto *it = result; it; it = it->ai_next) {
        if (it->ai_family != AF_INET && it->ai_family != AF_INET6) {
            continue;
        }

        QuicClientPeerEndpoint endpoint;
        endpoint.ip = SockUtil::inet_ntoa(it->ai_addr);
        endpoint.port = SockUtil::inet_port(it->ai_addr);
        endpoint.family = it->ai_family;
        if (endpoint.ip.empty() || endpoint.port == 0) {
            continue;
        }

        auto duplicate = std::find_if(endpoints.begin(), endpoints.end(),
                                      [&endpoint](const QuicClientPeerEndpoint &item) {
                                          return item.family == endpoint.family &&
                                                 item.port == endpoint.port &&
                                                 item.ip == endpoint.ip;
                                      });
        if (duplicate == endpoints.end()) {
            endpoints.emplace_back(std::move(endpoint));
        }
    }

    freeaddrinfo(result);
    return !endpoints.empty();
}

bool isIpLiteral(const std::string &host) {
    return !host.empty() && toolkit::isIP(host.c_str());
}

} // namespace

QuicClientBackend::UdpTransport::UdpTransport(QuicClientBackend &owner, const EventPoller::Ptr &poller)
    : UdpClient(poller), _owner(&owner) {
    setOnCreateSocket([this](const EventPoller::Ptr &socket_poller) {
        auto sock = Socket::createSocket(socket_poller, true);
        // Install the QUIC-specific socket tuning at creation time so the read
        // path never needs runtime buffer reconfiguration.
        configureQuicSocket(sock);
        sock->setOnSendResult([this](const Buffer::Ptr &, bool send_success) {
            if (send_success || !_owner) {
                return;
            }
            // UDP send failures are otherwise only logged by Socket.cpp. Surface
            // them as transport errors so HTTP/3 requests fail fast instead of
            // waiting for the generic response timeout.
            _owner->onTransportError(SockException(Err_refused, "udp send failed"));
        });
        return sock;
    });
}

void QuicClientBackend::UdpTransport::detachOwner() {
    _owner = nullptr;
}

void QuicClientBackend::UdpTransport::closeQuietly() {
    detachSocketCallbacks(std::static_pointer_cast<toolkit::UdpClient>(shared_from_this()));
    _owner = nullptr;
    setSock(nullptr);
}

void QuicClientBackend::UdpTransport::onRecvFrom(const Buffer::Ptr &buf, struct sockaddr *addr, int) {
    if (_owner) {
        _owner->onTransportPacket(buf, addr);
    }
}

void QuicClientBackend::UdpTransport::onError(const SockException &err) {
    UdpClient::onError(err);
    if (_owner) {
        _owner->onTransportError(err);
    }
}

QuicClientBackend::QuicClientBackend(const EventPoller::Ptr &poller)
    : _poller(poller ? poller : EventPollerPool::Instance().getPoller()) {
    _plugin_raw = zlm_quic_plugin_create();
    _plugin = QuicPluginRef(_plugin_raw);
    if (!_plugin.valid()) {
        WarnL << "failed to create QUIC plugin instance for client backend";
        return;
    }
    if (!_plugin.hasClient()) {
        WarnL << "QUIC plugin does not expose a client engine";
    }
}

QuicClientBackend::~QuicClientBackend() {
    shutdown();
    if (_plugin_raw) {
        zlm_quic_plugin_destroy(_plugin_raw);
        _plugin_raw = nullptr;
    }
}

void QuicClientBackend::setCallbacks(Callbacks callbacks) {
    if (_poller && !_poller->isCurrentThread()) {
        auto callbacks_ptr = std::make_shared<Callbacks>(std::move(callbacks));
        _poller->sync([this, callbacks_ptr]() {
            setCallbacks(std::move(*callbacks_ptr));
        });
        return;
    }

    _callbacks = std::move(callbacks);
}

void QuicClientBackend::setNetAdapter(std::string local_ip) {
    if (_poller && !_poller->isCurrentThread()) {
        auto local_ip_copy = std::move(local_ip);
        _poller->sync([this, local_ip_copy]() mutable {
            setNetAdapter(std::move(local_ip_copy));
        });
        return;
    }

    _net_adapter = std::move(local_ip);
    if (_transport) {
        _transport->setNetAdapter(_net_adapter);
    }
}

const EventPoller::Ptr &QuicClientBackend::getPoller() const {
    return _poller;
}

bool QuicClientBackend::available() const {
    return _plugin.valid() && _plugin.hasClient();
}

bool QuicClientBackend::alive() const {
    if (_poller && !_poller->isCurrentThread()) {
        bool ret = false;
        _poller->sync([this, &ret]() {
            ret = alive();
        });
        return ret;
    }
    return _engine != nullptr && _transport && _transport->alive();
}

int QuicClientBackend::start(const std::string &peer_host, uint16_t peer_port, const QuicClientConfig &config) {
    if (_poller && !_poller->isCurrentThread()) {
        int ret = -1;
        auto peer_host_copy = peer_host;
        auto config_copy = config;
        _poller->sync([this, &ret, peer_host_copy, peer_port, config_copy]() {
            ret = startOnPoller(peer_host_copy, peer_port, config_copy);
        });
        return ret;
    }
    return startOnPoller(peer_host, peer_port, config);
}

int QuicClientBackend::startOnPoller(const std::string &peer_host, uint16_t peer_port, const QuicClientConfig &config) {
    if (!_plugin.valid() || !_plugin.hasClient()) {
        WarnL << "QUIC client backend is unavailable because the plugin has no client engine";
        return -1;
    }

    resetEngine();
    _peer_resolve_host = peer_host;
    _peer_resolve_port = peer_port;
    _peer_endpoints.clear();
    if (!resolveUdpEndpoints(peer_host, peer_port, _peer_endpoints)) {
        WarnL << "failed to resolve QUIC peer host: " << peer_host << ":" << peer_port;
        return -1;
    }
    _peer_endpoint_index = 0;
    _alpn = quicSliceToString(config.alpn);
    _sni = quicSliceToString(config.sni);
    _ca_file = quicSliceToString(config.ca_file);
    _bind_ip = quicSliceToString(config.bind_ip);
    _connect_timeout_ms = config.connect_timeout_ms;
    _idle_timeout_ms = config.idle_timeout_ms;
    _configured_local_port = config.local_port;
    _verify_peer = config.verify_peer;
    _congestion_algo = config.congestion_algo;
    _handshake_completed = false;

    return startResolvedEndpoint(_peer_endpoints[_peer_endpoint_index]);
}

int QuicClientBackend::startResolvedEndpoint(const QuicClientPeerEndpoint &endpoint) {
    {
        std::string msg = StrPrinter << "quic client peer resolved: " << _peer_resolve_host << ":" << _peer_resolve_port
                                     << " -> " << endpoint.ip << ":" << endpoint.port;
        log(QuicLogLevel::Info, makeSlice(msg));
    }

    _peer_host = endpoint.ip;
    _peer_port = endpoint.port;
    _handshake_completed = false;

    QuicClientConfig owned_config;
    if (!_alpn.empty()) {
        owned_config.alpn = makeSlice(_alpn);
    }
    if (!_sni.empty()) {
        owned_config.sni = makeSlice(_sni);
    } else if (!isIpLiteral(_peer_resolve_host)) {
        _sni = _peer_resolve_host;
        owned_config.sni = makeSlice(_sni);
    }
    if (!_ca_file.empty()) {
        owned_config.ca_file = makeSlice(_ca_file);
    }
    if (!_bind_ip.empty()) {
        owned_config.bind_ip = makeSlice(_bind_ip);
    } else if (!_net_adapter.empty()) {
        _bind_ip = _net_adapter;
        owned_config.bind_ip = makeSlice(_bind_ip);
    }
    owned_config.connect_timeout_ms = _connect_timeout_ms;
    owned_config.idle_timeout_ms = _idle_timeout_ms;
    owned_config.local_port = _configured_local_port;
    owned_config.verify_peer = _verify_peer;
    owned_config.congestion_algo = _congestion_algo;
    owned_config.peer_host = makeSlice(_peer_host);
    owned_config.peer_port = _peer_port;

    _transport = std::make_shared<UdpTransport>(*this, _poller);
    // Queue packets during one engine drive so ZLToolKit can batch UDP writes via sendmsg/sendmmsg.
    _transport->setSendFlushFlag(false);
    if (!_bind_ip.empty()) {
        _transport->setNetAdapter(_bind_ip);
    } else if (!_net_adapter.empty()) {
        _transport->setNetAdapter(_net_adapter);
    } else {
        _transport->setNetAdapter(endpoint.family == AF_INET6 ? "::" : "0.0.0.0");
    }
    _transport->startConnect(endpoint.ip, endpoint.port, owned_config.local_port);
    if (auto sock = _transport->getSock()) {
        if (auto *peer_addr = sock->get_peer_addr()) {
            if (!sock->bindPeerAddr(peer_addr, 0, false)) {
                WarnL << "failed to hard-connect QUIC client UDP socket to "
                      << endpoint.ip << ":" << endpoint.port;
            }
        }
    }

    auto local = getSocketLocalEndpoint(_transport->getSock());
    if (local.first.empty() || !local.second) {
        // UdpClient::startConnect() only logs bind errors. Refuse to create the
        // QUIC engine when the UDP transport never obtained a concrete local
        // endpoint, otherwise later request failures look like mysterious QUIC
        // handshake issues instead of an immediate transport startup failure.
        WarnL << "failed to determine QUIC client UDP local endpoint after bind";
        closeTransport();
        return -1;
    }
    _local_ip = local.first;
    _local_port = local.second;
    _bind_ip = local.first;
    owned_config.bind_ip = makeSlice(_bind_ip);
    owned_config.local_port = local.second;
    std::string msg = StrPrinter << "quic client UDP bound: " << _local_ip << ":" << _local_port;
    log(QuicLogLevel::Info, makeSlice(msg));

    _engine = _plugin.createClientEngine(*this, owned_config);
    if (!_engine) {
        WarnL << "failed to create QUIC client engine from plugin: " << _plugin.pluginName();
        closeTransport();
        return -1;
    }

    flushTransport();
    return 0;
}

int QuicClientBackend::startRequest(const QuicClientRequest &request) {
    if (_poller && !_poller->isCurrentThread()) {
        int ret = -1;
        auto request_copy = request;
        _poller->sync([this, &ret, request_copy]() {
            ret = startRequestOnPoller(request_copy);
        });
        return ret;
    }
    return startRequestOnPoller(request);
}

int QuicClientBackend::startRequestOnPoller(const QuicClientRequest &request) {
    if (!_engine) {
        return -1;
    }

    auto &state = _requests[request.request_id];
    state.request = request;
    state.header_views.clear();
    state.header_views.reserve(state.request.headers.size());
    for (auto &header : state.request.headers) {
        QuicHeader view;
        view.name = makeSlice(header.name);
        view.value = makeSlice(header.value);
        state.header_views.emplace_back(view);
    }

    QuicClientRequestDesc desc;
    desc.request_id = state.request.request_id;
    desc.scheme = makeSlice(state.request.scheme);
    desc.authority = makeSlice(state.request.authority);
    desc.path = makeSlice(state.request.path);
    desc.method = makeSlice(state.request.method);
    desc.headers = state.header_views.empty() ? nullptr : state.header_views.data();
    desc.header_count = state.header_views.size();
    desc.end_stream = state.request.end_stream;

    auto ret = _engine->startRequest(desc);
    flushTransport();
    refreshTimer();
    if (ret != 0) {
        _requests.erase(request.request_id);
    }
    return ret;
}

int QuicClientBackend::sendBody(uint64_t request_id, const uint8_t *data, size_t len, bool fin) {
    if (_poller && !_poller->isCurrentThread()) {
        int ret = -1;
        std::string body;
        if (data && len) {
            body.assign(reinterpret_cast<const char *>(data), len);
        }
        _poller->sync([this, &ret, request_id, body, fin]() {
            ret = sendBodyOnPoller(request_id,
                                   body.empty() ? nullptr : reinterpret_cast<const uint8_t *>(body.data()),
                                   body.size(),
                                   fin);
        });
        return ret;
    }
    return sendBodyOnPoller(request_id, data, len, fin);
}

int QuicClientBackend::sendBodyOnPoller(uint64_t request_id, const uint8_t *data, size_t len, bool fin) {
    if (!_engine) {
        return -1;
    }
    auto ret = _engine->sendBody(request_id, data, len, fin);
    flushTransport();
    refreshTimer();
    return ret;
}

int QuicClientBackend::cancelRequest(uint64_t request_id, uint64_t app_error_code) {
    if (_poller && !_poller->isCurrentThread()) {
        int ret = -1;
        _poller->sync([this, &ret, request_id, app_error_code]() {
            ret = cancelRequestOnPoller(request_id, app_error_code);
        });
        return ret;
    }
    return cancelRequestOnPoller(request_id, app_error_code);
}

int QuicClientBackend::cancelRequestOnPoller(uint64_t request_id, uint64_t app_error_code) {
    if (!_engine) {
        return -1;
    }
    auto ret = _engine->cancelRequest(request_id, app_error_code);
    flushTransport();
    refreshTimer();
    return ret;
}

int QuicClientBackend::shutdown(uint64_t app_error_code, const std::string &reason) {
    if (!_engine) {
        return 0;
    }

    if (_poller && !_poller->isCurrentThread()) {
        auto reason_copy = reason;
        int ret = 0;
        // The QUIC engine owns timer state and pending UDP writes, so shutdown must
        // run on the owner poller thread before transport callbacks are detached.
        _poller->sync([this, app_error_code, &ret, reason_copy]() {
            if (!_engine) {
                ret = 0;
                return;
            }
            ret = _engine->shutdown(app_error_code, makeSlice(reason_copy));
            flushTransport();
            resetEngine();
        });
        return ret;
    }

    auto ret = _engine->shutdown(app_error_code, makeSlice(reason));
    flushTransport();
    resetEngine();
    return ret;
}

void QuicClientBackend::log(QuicLogLevel level, QuicSlice message) {
    auto text = quicSliceToString(message);
    if (text == "lsquic client handshake completed") {
        _handshake_completed = true;
    }
    if (_callbacks.on_log) {
        _callbacks.on_log(level, text);
    }

    switch (level) {
    case QuicLogLevel::Trace:
        TraceL << "[quic-client] " << text;
        break;
    case QuicLogLevel::Debug:
        DebugL << "[quic-client] " << text;
        break;
    case QuicLogLevel::Info:
        InfoL << "[quic-client] " << text;
        break;
    case QuicLogLevel::Warn:
        WarnL << "[quic-client] " << text;
        break;
    case QuicLogLevel::Error:
        ErrorL << "[quic-client] " << text;
        break;
    }
}

uint64_t QuicClientBackend::nowMS() {
    return getCurrentMillisecond();
}

int QuicClientBackend::sendPacket(const QuicPacketView &packet) {
    if (!_transport) {
        return -1;
    }

    auto buffer = BufferRaw::create();
    buffer->assign(reinterpret_cast<const char *>(packet.payload.data), packet.payload.len);
    auto ret = _transport->send(std::move(buffer)) > 0 ? 0 : -1;
    TraceL << "client transport sent QUIC packet, size=" << packet.payload.len
           << ", local=" << quicSliceToString(packet.local_ip) << ":" << packet.local_port
           << ", peer=" << quicSliceToString(packet.peer_ip) << ":" << packet.peer_port
           << ", rc=" << ret;
    return ret;
}

void QuicClientBackend::wakeEngine(uint64_t delay_ms) {
    armTimer(delay_ms);
}

void QuicClientBackend::onHeaders(const QuicClientHeaders &headers) {
    if (!_callbacks.on_headers) {
        return;
    }

    QuicClientResponseHeaders response;
    response.request_id = headers.request_id;
    response.status_code = headers.status_code;
    response.fin = headers.fin;
    response.headers.reserve(headers.header_count);
    for (size_t i = 0; i < headers.header_count; ++i) {
        QuicClientOwnedHeader header;
        header.name = quicSliceToString(headers.headers[i].name);
        header.value = quicSliceToString(headers.headers[i].value);
        response.headers.emplace_back(std::move(header));
    }
    auto weak_self = std::weak_ptr<QuicClientBackend>(shared_from_this());
    // The lsquic engine can invoke host callbacks while its internal mutex is
    // still held. Dispatch back onto the poller so higher layers can safely
    // cancel/restart requests (for example on HTTP redirect) without re-entering
    // the engine under the same lock.
    _poller->async([weak_self, response]() mutable {
        auto self = weak_self.lock();
        if (!self || !self->_callbacks.on_headers) {
            return;
        }
        self->_callbacks.on_headers(response);
    }, false);
}

void QuicClientBackend::onBody(uint64_t request_id, const uint8_t *data, size_t len, bool fin) {
    if (!_callbacks.on_body) {
        return;
    }

    auto body = std::make_shared<std::string>();
    if (data && len) {
        body->assign(reinterpret_cast<const char *>(data), len);
    }
    auto weak_self = std::weak_ptr<QuicClientBackend>(shared_from_this());
    // Body payload is copied before the async hop because the lsquic read buffer
    // is only valid for the duration of the callback.
    _poller->async([weak_self, request_id, body, fin]() {
        auto self = weak_self.lock();
        if (!self || !self->_callbacks.on_body) {
            return;
        }
        self->_callbacks.on_body(request_id,
                                 body->empty() ? nullptr : reinterpret_cast<const uint8_t *>(body->data()),
                                 body->size(),
                                 fin);
    }, false);
}

void QuicClientBackend::onRequestClosed(const QuicClientClose &close_info) {
    _requests.erase(close_info.request_id);

    if (_callbacks.on_close) {
        QuicClientCloseInfo close;
        close.request_id = close_info.request_id;
        close.state = close_info.state;
        close.app_error_code = close_info.app_error_code;
        close.reason = quicSliceToString(close_info.reason);
        auto weak_self = std::weak_ptr<QuicClientBackend>(shared_from_this());
        _poller->async([weak_self, close]() {
            auto self = weak_self.lock();
            if (!self || !self->_callbacks.on_close) {
                return;
            }
            self->_callbacks.on_close(close);
        }, false);
    }
}

void QuicClientBackend::onTransportPacket(const Buffer::Ptr &buf, struct sockaddr *addr) {
    if (!_engine || !buf || !addr) {
        return;
    }

    auto peer_ip = SockUtil::inet_ntoa(addr);
    auto peer_port = SockUtil::inet_port(addr);
    auto packet = makePacketView(buf, _local_ip, _local_port, peer_ip, peer_port);
    auto rc = _engine->handlePacket(packet);
    flushTransport();
    TraceL << "client transport received QUIC packet, size=" << packet.payload.len
           << ", local=" << _local_ip << ":" << _local_port
           << ", peer=" << peer_ip << ":" << peer_port
           << ", rc=" << rc;
    refreshTimer();
}

void QuicClientBackend::onTransportError(const SockException &err) {
    if (!_engine) {
        return;
    }

    if (shouldRetryNextEndpoint() && restartOnNextEndpoint()) {
        return;
    }

    std::vector<QuicClientCloseInfo> closes;
    closes.reserve(_requests.size());
    for (auto &entry : _requests) {
        QuicClientCloseInfo close;
        close.request_id = entry.first;
        close.state = QuicClientState::Failed;
        close.reason = err.what();
        closes.emplace_back(std::move(close));
    }
    _requests.clear();
    // UdpClient only reports the socket error; it does not tear the transport
    // down for us. Once send/recv has failed at the UDP layer, keep the client
    // backend from reusing the stale engine/transport pair for the next request.
    resetEngine();
    if (!_callbacks.on_close || closes.empty()) {
        return;
    }

    auto weak_self = std::weak_ptr<QuicClientBackend>(shared_from_this());
    _poller->async([weak_self, closes]() {
        auto self = weak_self.lock();
        if (!self || !self->_callbacks.on_close) {
            return;
        }
        for (auto &close : closes) {
            self->_callbacks.on_close(close);
        }
    }, false);
}

bool QuicClientBackend::shouldRetryNextEndpoint() const {
    if (_handshake_completed) {
        return false;
    }
    if (_peer_endpoint_index + 1 >= _peer_endpoints.size()) {
        return false;
    }
    if (_requests.empty()) {
        return false;
    }
    for (const auto &entry : _requests) {
        if (!entry.second.request.end_stream) {
            return false;
        }
    }
    return true;
}

bool QuicClientBackend::restartOnNextEndpoint() {
    auto pending_requests = _requests;
    while (_peer_endpoint_index + 1 < _peer_endpoints.size()) {
        ++_peer_endpoint_index;
        resetEngine();
        if (startResolvedEndpoint(_peer_endpoints[_peer_endpoint_index]) != 0) {
            continue;
        }

        bool replay_ok = true;
        for (const auto &entry : pending_requests) {
            if (startRequestOnPoller(entry.second.request) != 0) {
                replay_ok = false;
                break;
            }
        }

        if (replay_ok) {
            std::string msg = StrPrinter << "quic client retried next resolved endpoint: "
                                         << _peer_host << ":" << _peer_port;
            log(QuicLogLevel::Warn, makeSlice(msg));
            return true;
        }
    }
    return false;
}

void QuicClientBackend::resetEngine() {
    {
        std::lock_guard<std::mutex> lock(_timer_mutex);
        ++_timer_seq;
        _timer_due_ms = 0;
        if (_timer_task) {
            _timer_task->cancel();
            _timer_task.reset();
        }
    }
    _requests.clear();
    _local_ip.clear();
    _local_port = 0;
    _handshake_completed = false;

    auto *engine = _engine;
    _engine = nullptr;

    closeTransport();

    if (engine) {
        _plugin.destroyClientEngine(engine);
    }
}

void QuicClientBackend::closeTransport() {
    auto transport = _transport;
    if (!transport) {
        return;
    }

    _transport.reset();
    auto cleanup = [transport]() {
        transport->closeQuietly();
    };
    if (auto poller = transport->getPoller()) {
        poller->sync(cleanup);
    } else {
        cleanup();
    }
}

void QuicClientBackend::flushTransport() {
    if (!_transport) {
        return;
    }
    auto ret = _transport->flushAll();
    if (ret != 0 && _transport->alive()) {
        DebugL << "client transport flush returned: " << ret;
    }
}

void QuicClientBackend::refreshTimer() {
    if (!_engine) {
        return;
    }

    auto now = nowMS();
    auto next = _engine->nextTimeoutMS(now);
    if (next <= now) {
        armTimer(0);
    } else {
        armTimer(next - now);
    }
}

void QuicClientBackend::armTimer(uint64_t delay_ms) {
    auto poller = _poller;
    if (!poller || !_engine) {
        return;
    }

    auto due_ms = nowMS() + delay_ms;
    std::weak_ptr<QuicClientBackend> weak_self;
    try {
        weak_self = std::weak_ptr<QuicClientBackend>(shared_from_this());
    } catch (const std::bad_weak_ptr &) {
        return;
    }

    uint64_t timer_seq = 0;
    {
        std::lock_guard<std::mutex> lock(_timer_mutex);
        if (_timer_task && _timer_due_ms <= due_ms) {
            return;
        }
        if (_timer_task) {
            _timer_task->cancel();
            _timer_task.reset();
        }
        // Only keep the earliest timer task armed. Later callbacks are invalidated
        // through _timer_seq so delayed tasks that race with shutdown become no-ops.
        timer_seq = ++_timer_seq;
        _timer_due_ms = due_ms;
        _timer_task = poller->doDelayTask(delay_ms, [weak_self, timer_seq]() -> uint64_t {
            auto self = weak_self.lock();
            if (!self) {
                return 0;
            }
            return self->runTimerTask(timer_seq);
        });
    }
}

uint64_t QuicClientBackend::runTimerTask(uint64_t timer_seq) {
    {
        std::lock_guard<std::mutex> lock(_timer_mutex);
        if (timer_seq != _timer_seq) {
            return 0;
        }
        _timer_due_ms = 0;
        _timer_task.reset();
    }

    if (!_engine) {
        return 0;
    }

    auto now = nowMS();
    _engine->onTimer(now);
    flushTransport();
    refreshTimer();
    return 0;
}

} // namespace mediakit
