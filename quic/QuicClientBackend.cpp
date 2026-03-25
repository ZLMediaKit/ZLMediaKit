#include "QuicClientBackend.h"

#include "Network/Buffer.h"
#include "Network/sockutil.h"
#include "Util/logger.h"
#include "Util/util.h"

namespace mediakit {

using namespace toolkit;

namespace {

static std::string quicSliceToString(QuicSlice slice) {
    if (!slice.data || !slice.len) {
        return std::string();
    }
    return std::string(slice.data, slice.len);
}

static QuicSlice makeSlice(const std::string &str) {
    QuicSlice slice;
    slice.data = str.data();
    slice.len = str.size();
    return slice;
}

static QuicPacketView makePacketView(const Buffer::Ptr &buf, const std::string &local_ip,
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

static std::pair<std::string, uint16_t> getSocketLocalEndpoint(const Socket::Ptr &sock) {
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

static void detachSocketCallbacks(const std::shared_ptr<toolkit::UdpClient> &transport) {
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
}

} // namespace

QuicClientBackend::UdpTransport::UdpTransport(QuicClientBackend &owner, const EventPoller::Ptr &poller)
    : UdpClient(poller), _owner(&owner) {}

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
    _callbacks = std::move(callbacks);
}

void QuicClientBackend::setNetAdapter(std::string local_ip) {
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
    return _engine != nullptr && _transport && _transport->alive();
}

int QuicClientBackend::start(const std::string &peer_host, uint16_t peer_port, const QuicClientConfig &config) {
    if (!_plugin.valid() || !_plugin.hasClient()) {
        WarnL << "QUIC client backend is unavailable because the plugin has no client engine";
        return -1;
    }

    resetEngine();
    _peer_host = peer_host;
    _peer_port = peer_port;
    _alpn = quicSliceToString(config.alpn);
    _sni = quicSliceToString(config.sni);
    _ca_file = quicSliceToString(config.ca_file);
    _bind_ip = quicSliceToString(config.bind_ip);

    QuicClientConfig owned_config = config;
    if (!_alpn.empty()) {
        owned_config.alpn = makeSlice(_alpn);
    }
    if (!_sni.empty()) {
        owned_config.sni = makeSlice(_sni);
    } else {
        _sni = peer_host;
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
    owned_config.peer_host = makeSlice(_peer_host);
    owned_config.peer_port = _peer_port;

    _transport = std::make_shared<UdpTransport>(*this, _poller);
    if (!_net_adapter.empty()) {
        _transport->setNetAdapter(_net_adapter);
    }
    _transport->startConnect(peer_host, peer_port, owned_config.local_port);
    if (auto sock = _transport->getSock()) {
        if (auto *peer_addr = sock->get_peer_addr()) {
            if (!sock->bindPeerAddr(peer_addr, 0, false)) {
                WarnL << "failed to hard-connect QUIC client UDP socket to "
                      << peer_host << ":" << peer_port;
            }
        }
    }

    auto local = getSocketLocalEndpoint(_transport->getSock());
    if (!local.first.empty() && local.second) {
        _bind_ip = local.first;
        owned_config.bind_ip = makeSlice(_bind_ip);
        owned_config.local_port = local.second;
    }

    _engine = _plugin.createClientEngine(*this, owned_config);
    if (!_engine) {
        WarnL << "failed to create QUIC client engine from plugin: " << _plugin.pluginName();
        if (_transport) {
            auto transport = _transport;
            auto cleanup = [transport]() {
                transport->closeQuietly();
            };
            if (auto poller = transport->getPoller()) {
                poller->sync(cleanup);
            } else {
                cleanup();
            }
            _transport.reset();
        }
        return -1;
    }

    armTimer(0);
    return 0;
}

int QuicClientBackend::startRequest(const QuicClientRequest &request) {
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
    armTimer(0);
    if (ret != 0) {
        _requests.erase(request.request_id);
    }
    return ret;
}

int QuicClientBackend::sendBody(uint64_t request_id, const uint8_t *data, size_t len, bool fin) {
    if (!_engine) {
        return -1;
    }
    auto ret = _engine->sendBody(request_id, data, len, fin);
    armTimer(0);
    return ret;
}

int QuicClientBackend::cancelRequest(uint64_t request_id, uint64_t app_error_code) {
    if (!_engine) {
        return -1;
    }
    auto ret = _engine->cancelRequest(request_id, app_error_code);
    armTimer(0);
    return ret;
}

int QuicClientBackend::shutdown(uint64_t app_error_code, const std::string &reason) {
    if (!_engine) {
        return 0;
    }

    if (_poller && !_poller->isCurrentThread()) {
        auto reason_copy = reason;
        int ret = 0;
        _poller->sync([this, app_error_code, &ret, reason_copy]() {
            if (!_engine) {
                ret = 0;
                return;
            }
            ret = _engine->shutdown(app_error_code, makeSlice(reason_copy));
            resetEngine();
        });
        return ret;
    }

    auto ret = _engine->shutdown(app_error_code, makeSlice(reason));
    resetEngine();
    return ret;
}

void QuicClientBackend::log(QuicLogLevel level, QuicSlice message) {
    auto text = quicSliceToString(message);
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
    _callbacks.on_headers(response);
}

void QuicClientBackend::onBody(uint64_t request_id, const uint8_t *data, size_t len, bool fin) {
    if (_callbacks.on_body) {
        _callbacks.on_body(request_id, data, len, fin);
    }
}

void QuicClientBackend::onRequestClosed(const QuicClientClose &close_info) {
    _requests.erase(close_info.request_id);

    if (_callbacks.on_close) {
        QuicClientCloseInfo close;
        close.request_id = close_info.request_id;
        close.state = close_info.state;
        close.app_error_code = close_info.app_error_code;
        close.reason = quicSliceToString(close_info.reason);
        _callbacks.on_close(close);
    }
}

void QuicClientBackend::onTransportPacket(const Buffer::Ptr &buf, struct sockaddr *addr) {
    if (!_engine || !buf || !addr) {
        return;
    }

    auto local = _transport ? getSocketLocalEndpoint(_transport->getSock()) : std::make_pair(std::string(), uint16_t(0));
    auto peer_ip = SockUtil::inet_ntoa(addr);
    auto peer_port = SockUtil::inet_port(addr);
    auto packet = makePacketView(buf, local.first, local.second, peer_ip, peer_port);
    auto rc = _engine->handlePacket(packet);
    TraceL << "client transport received QUIC packet, size=" << packet.payload.len
           << ", local=" << local.first << ":" << local.second
           << ", peer=" << peer_ip << ":" << peer_port
           << ", rc=" << rc;
    armTimer(0);
}

void QuicClientBackend::onTransportError(const SockException &err) {
    if (!_engine) {
        return;
    }

    QuicClientCloseInfo close;
    close.state = QuicClientState::Failed;
    close.reason = err.what();
    for (auto &entry : _requests) {
        close.request_id = entry.first;
        if (_callbacks.on_close) {
            _callbacks.on_close(close);
        }
    }
    _requests.clear();
}

void QuicClientBackend::resetEngine() {
    ++_timer_generation;
    _requests.clear();

    auto *engine = _engine;
    _engine = nullptr;

    if (_transport) {
        auto transport = _transport;
        auto cleanup = [transport]() {
            transport->closeQuietly();
        };
        if (auto poller = transport->getPoller()) {
            poller->sync(cleanup);
        } else {
            cleanup();
        }
        _transport.reset();
    }

    if (engine) {
        _plugin.destroyClientEngine(engine);
    }
}

void QuicClientBackend::armTimer(uint64_t delay_ms) {
    auto poller = _poller;
    if (!poller || !_engine) {
        return;
    }

    auto generation = ++_timer_generation;
    std::weak_ptr<QuicClientBackend> weak_self;
    try {
        weak_self = std::weak_ptr<QuicClientBackend>(shared_from_this());
    } catch (const std::bad_weak_ptr &) {
        return;
    }
    poller->doDelayTask(delay_ms, [weak_self, generation]() -> uint64_t {
        auto self = weak_self.lock();
        if (!self) {
            return 0;
        }
        return self->runTimerTask(generation);
    });
}

uint64_t QuicClientBackend::runTimerTask(uint64_t generation) {
    if (generation != _timer_generation.load() || !_engine) {
        return 0;
    }

    auto now = nowMS();
    _engine->onTimer(now);
    auto next = _engine->nextTimeoutMS(nowMS());
    auto current = nowMS();
    if (next <= current) {
        return 1;
    }
    return next - current;
}

} // namespace mediakit
