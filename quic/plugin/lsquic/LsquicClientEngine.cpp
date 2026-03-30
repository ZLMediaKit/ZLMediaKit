#include "LsquicClientEngine.h"

#include "LsquicCommon.h"

#include <lsxpack_header.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mediakit {

namespace {

using namespace lsquicplugin;

class LsquicClientEngine final : public IQuicClientEngine {
public:
    explicit LsquicClientEngine(IQuicClientHost &host, const QuicClientConfig &config)
        : _host(host),
          _alpn(toString(config.alpn).empty() ? std::string("h3") : toString(config.alpn)),
          _sni(toString(config.sni)),
          _ca_file(toString(config.ca_file)),
          _bind_ip(toString(config.bind_ip)),
          _peer_host(toString(config.peer_host)),
          _local_port(config.local_port),
          _peer_port(config.peer_port),
          _verify_peer(config.verify_peer) {
        if (!initTlsContext()) {
            return;
        }

        auto flags = LSENG_HTTP;
        lsquic_engine_init_settings(&_settings, flags);
        _settings.es_idle_timeout = std::max<unsigned>(1, config.idle_timeout_ms ? (config.idle_timeout_ms + 999) / 1000 : 30);
        _settings.es_support_push = 0;
        _settings.es_cc_algo = static_cast<unsigned>(config.congestion_algo);

        char err_buf[256] = {0};
        if (lsquic_engine_check_settings(&_settings, flags, err_buf, sizeof(err_buf)) != 0) {
            std::string message = "invalid lsquic client settings";
            if (err_buf[0]) {
                message += ": ";
                message += err_buf;
            }
            _host.log(QuicLogLevel::Error, makeSlice(message));
            return;
        }

        std::memset(&_api, 0, sizeof(_api));
        _api.ea_settings = &_settings;
        _api.ea_stream_if = &streamInterface();
        _api.ea_stream_if_ctx = this;
        _api.ea_packets_out = &LsquicClientEngine::packetsOutShim;
        _api.ea_packets_out_ctx = this;
        _api.ea_get_ssl_ctx = &LsquicClientEngine::getSslCtxShim;
        _api.ea_hsi_if = &headerSetInterface();
        if (_verify_peer) {
            _api.ea_verify_cert = &LsquicClientEngine::verifyCertShim;
            _api.ea_verify_ctx = this;
        }

        _engine = lsquic_engine_new(flags, &_api);
        if (!_engine) {
            _host.log(QuicLogLevel::Error, makeSlice("lsquic_engine_new failed for client"));
            return;
        }

        {
            auto msg = std::string("lsquic client congestion control: ") + quicCongestionAlgoName(config.congestion_algo);
            _host.log(QuicLogLevel::Info, makeSlice(msg));
        }
        _host.log(QuicLogLevel::Info, makeSlice("lsquic client engine initialized"));
        _available = true;
    }

    ~LsquicClientEngine() override {
        if (_engine) {
            lsquic_engine_destroy(_engine);
            _engine = nullptr;
        }
        if (_conn_ctx) {
            delete _conn_ctx;
            _conn_ctx = nullptr;
        }
        _requests.clear();
        _route.reset();
    }

    bool available() const {
        return _available;
    }

    int handlePacket(const QuicPacketView &packet) override {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_engine || !_route) {
            return -1;
        }

        updateRoute(packet);
        auto rc = lsquic_engine_packet_in(_engine,
                                          packet.payload.data,
                                          packet.payload.len,
                                          reinterpret_cast<const sockaddr *>(&_route->local_addr),
                                          reinterpret_cast<const sockaddr *>(&_route->peer_addr),
                                          _route.get(),
                                          packet.ecn);
        if (rc >= 0) {
            driveEngine();
        }
        return rc;
    }

    int onTimer(uint64_t) override {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_engine) {
            return -1;
        }
        driveEngine();
        return 0;
    }

    uint64_t nextTimeoutMS(uint64_t now_ms) const override {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_engine) {
            return now_ms + 1000;
        }

        int diff = 0;
        if (!lsquic_engine_earliest_adv_tick(_engine, &diff)) {
            return now_ms + 1000;
        }
        if (diff <= 0) {
            return now_ms;
        }
        return now_ms + static_cast<uint64_t>((diff + 999) / 1000);
    }

    int startRequest(const QuicClientRequestDesc &request) override {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_engine) {
            return -1;
        }

        auto req = std::make_shared<ClientRequestCtx>();
        req->request_id = request.request_id;
        req->scheme = toString(request.scheme).empty() ? std::string("https") : toString(request.scheme);
        req->authority = toString(request.authority);
        req->path = toString(request.path).empty() ? std::string("/") : toString(request.path);
        req->method = toString(request.method).empty() ? std::string("GET") : toString(request.method);
        req->request_fin = request.end_stream;
        req->state = QuicClientState::Connecting;
        req->headers.reserve(request.header_count);
        for (size_t i = 0; i < request.header_count; ++i) {
            req->headers.emplace_back(toString(request.headers[i].name), toString(request.headers[i].value));
        }

        if (!ensureConnection(req)) {
            return -1;
        }

        _requests[req->request_id] = req;
        _conn_ctx->pending_requests.emplace_back(req->request_id);
        lsquic_conn_make_stream(_conn_ctx->conn);
        driveEngine();
        return 0;
    }

    int sendBody(uint64_t request_id, const uint8_t *data, size_t len, bool fin) override {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _requests.find(request_id);
        if (it == _requests.end()) {
            return -1;
        }

        auto &req = it->second;
        if (len) {
            req->request_body.emplace_back(reinterpret_cast<const char *>(data), len);
        }
        if (fin) {
            req->request_fin = true;
        }

        if (req->stream) {
            lsquic_stream_wantwrite(req->stream, 1);
            driveEngine();
        }
        return 0;
    }

    int cancelRequest(uint64_t request_id, uint64_t) override {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _requests.find(request_id);
        if (it == _requests.end()) {
            return -1;
        }

        auto req = it->second;
        if (req->stream) {
            lsquic_stream_close(req->stream);
            driveEngine();
        } else {
            reportClose(req, QuicClientState::Failed, 0, "request canceled before stream creation");
            _requests.erase(it);
        }
        return 0;
    }

    int shutdown(uint64_t, QuicSlice reason) override {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_engine) {
            return 0;
        }

        auto close_reason = toString(reason);
        if (_conn_ctx && _conn_ctx->conn) {
            _shutdown_reason = close_reason;
            _shutdown_requested = true;
            lsquic_conn_close(_conn_ctx->conn);
            driveEngine();
        }
        return 0;
    }

private:
    struct RouteKey {
        std::string local_ip;
        std::string peer_ip;
        uint16_t local_port = 0;
        uint16_t peer_port = 0;
    };

    struct PacketRoute {
        LsquicClientEngine *engine = nullptr;
        RouteKey key;
        sockaddr_storage local_addr;
        sockaddr_storage peer_addr;
        socklen_t local_addr_len = 0;
        socklen_t peer_addr_len = 0;
    };

    struct HeaderSlot {
        lsxpack_header xhdr;
        char *buffer = nullptr;
        size_t capacity = 0;
    };

    struct ClientHeaderSet {
        std::vector<HeaderSlot *> slots;
        int32_t status_code = 0;
        std::vector<std::pair<std::string, std::string>> headers;
    };

    struct ClientRequestCtx {
        uint64_t request_id = 0;
        std::string scheme;
        std::string authority;
        std::string path;
        std::string method;
        std::vector<std::pair<std::string, std::string>> headers;
        std::deque<std::string> request_body;
        size_t request_body_offset = 0;
        lsquic_stream_t *stream = nullptr;
        ClientHeaderSet *header_set = nullptr;
        QuicClientState state = QuicClientState::Connecting;
        bool request_fin = false;
        bool headers_sent = false;
        bool response_headers_emitted = false;
        bool response_fin = false;
        bool close_reported = false;
        bool write_shutdown = false;
        int reset_how = -1;
        std::string close_reason;
    };

    struct ClientConnCtx {
        LsquicClientEngine *engine = nullptr;
        lsquic_conn_t *conn = nullptr;
        std::deque<uint64_t> pending_requests;
        bool handshake_done = false;
    };

    struct ClientStreamCtx {
        LsquicClientEngine *engine = nullptr;
        std::shared_ptr<ClientRequestCtx> request;
    };

    bool initTlsContext() {
        _ssl_ctx.reset(SSL_CTX_new(TLS_method()));
        if (!_ssl_ctx) {
            _host.log(QuicLogLevel::Error, makeSlice("SSL_CTX_new failed for client"));
            return false;
        }

        SSL_CTX_set_min_proto_version(_ssl_ctx.get(), TLS1_3_VERSION);
        SSL_CTX_set_max_proto_version(_ssl_ctx.get(), TLS1_3_VERSION);
        SSL_CTX_set_default_verify_paths(_ssl_ctx.get());
#if defined(SSL_CTX_set_early_data_enabled)
        SSL_CTX_set_early_data_enabled(_ssl_ctx.get(), 1);
#endif

        if (!_ca_file.empty() && SSL_CTX_load_verify_locations(_ssl_ctx.get(), _ca_file.c_str(), nullptr) != 1) {
            auto msg = std::string("SSL_CTX_load_verify_locations failed: ") + _ca_file;
            _host.log(QuicLogLevel::Error, makeSlice(msg));
            return false;
        }

        return true;
    }

    bool ensureConnection(const std::shared_ptr<ClientRequestCtx> &req) {
        if (_conn_ctx && _conn_ctx->conn) {
            // The current host-side client backend intentionally reuses one QUIC
            // connection per engine/requester, so authority changes require a new
            // backend instead of opportunistically coalescing here.
            if (!sameAuthority(req->authority)) {
                _host.log(QuicLogLevel::Warn, makeSlice("lsquic client engine currently supports only one authority per connection"));
                return false;
            }
            return true;
        }

        auto route = std::make_shared<PacketRoute>();
        if (!resolveRoute(req->authority, route)) {
            _host.log(QuicLogLevel::Error, makeSlice("failed to resolve HTTP/3 authority"));
            return false;
        }

        _route = route;
        _route->engine = this;
        _authority = req->authority;
        _conn_ctx = new ClientConnCtx();
        _conn_ctx->engine = this;
        _conn_ctx->conn = lsquic_engine_connect(_engine,
                                               N_LSQVER,
                                               reinterpret_cast<const sockaddr *>(&_route->local_addr),
                                               reinterpret_cast<const sockaddr *>(&_route->peer_addr),
                                               _route.get(),
                                               reinterpret_cast<lsquic_conn_ctx_t *>(_conn_ctx),
                                               _sni.empty() ? hostFromAuthority(req->authority).c_str() : _sni.c_str(),
                                               0,
                                               nullptr,
                                               0,
                                               nullptr,
                                               0);
        if (!_conn_ctx->conn) {
            auto err = ERR_get_error();
            auto msg = std::string("lsquic_engine_connect failed: ") + toErrorString(static_cast<int>(err));
            _host.log(QuicLogLevel::Error, makeSlice(msg));
            delete _conn_ctx;
            _conn_ctx = nullptr;
            _route.reset();
            return false;
        }
        return true;
    }

    bool sameAuthority(const std::string &authority) const {
        if (_authority.empty()) {
            return true;
        }
        return authority == _authority;
    }

    std::string hostFromAuthority(const std::string &authority) const {
        std::string host;
        uint16_t port = 443;
        parseAuthority(authority, host, port);
        return host;
    }

    bool parseAuthority(const std::string &authority, std::string &host, uint16_t &port) const {
        port = 443;
        if (authority.empty()) {
            return false;
        }

        if (authority.front() == '[') {
            auto pos = authority.find(']');
            if (pos == std::string::npos) {
                return false;
            }
            host = authority.substr(1, pos - 1);
            if (pos + 1 < authority.size() && authority[pos + 1] == ':') {
                try {
                    port = static_cast<uint16_t>(std::stoul(authority.substr(pos + 2)));
                } catch (...) {
                    return false;
                }
            }
            return true;
        }

        auto pos = authority.rfind(':');
        if (pos != std::string::npos && authority.find(':') == pos) {
            host = authority.substr(0, pos);
            try {
                port = static_cast<uint16_t>(std::stoul(authority.substr(pos + 1)));
            } catch (...) {
                return false;
            }
            return true;
        }

        host = authority;
        return true;
    }

    bool resolveRoute(const std::string &authority, const std::shared_ptr<PacketRoute> &route) {
        std::string host = _peer_host;
        uint16_t port = _peer_port;
        if (host.empty() || !port) {
            port = 443;
            if (!parseAuthority(authority, host, port)) {
                return false;
            }
        }

        addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_family = AF_UNSPEC;
        addrinfo *result = nullptr;
        auto port_text = std::to_string(port);
        if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0 || !result) {
            return false;
        }

        bool ok = false;
        for (auto *it = result; it; it = it->ai_next) {
            if (it->ai_family != AF_INET && it->ai_family != AF_INET6) {
                continue;
            }

            std::memcpy(&route->peer_addr, it->ai_addr, it->ai_addrlen);
            route->peer_addr_len = static_cast<socklen_t>(it->ai_addrlen);
            if (!sockaddrToEndpoint(reinterpret_cast<const sockaddr *>(&route->peer_addr),
                                    route->key.peer_ip,
                                    route->key.peer_port)) {
                continue;
            }

            auto local_ip = _bind_ip;
            if (local_ip.empty()) {
                local_ip = it->ai_family == AF_INET6 ? "::" : "0.0.0.0";
            }
            QuicSlice local_slice = makeSlice(local_ip);
            if (!sliceToSockaddr(local_slice, _local_port, route->local_addr, route->local_addr_len)) {
                continue;
            }
            route->key.local_ip = local_ip;
            route->key.local_port = _local_port;
            ok = true;
            break;
        }

        freeaddrinfo(result);
        return ok;
    }

    void updateRoute(const QuicPacketView &packet) {
        if (!_route) {
            return;
        }

        auto local_ip = toString(packet.local_ip);
        auto peer_ip = toString(packet.peer_ip);
        if (!local_ip.empty() && !peer_ip.empty()) {
            _route->key.local_ip = local_ip;
            _route->key.peer_ip = peer_ip;
            _route->key.local_port = packet.local_port;
            _route->key.peer_port = packet.peer_port;
            sliceToSockaddr(packet.local_ip, packet.local_port, _route->local_addr, _route->local_addr_len);
            sliceToSockaddr(packet.peer_ip, packet.peer_port, _route->peer_addr, _route->peer_addr_len);
        }
    }

    void driveEngine() {
        lsquic_engine_process_conns(_engine);
        while (lsquic_engine_has_unsent_packets(_engine)) {
            lsquic_engine_send_unsent_packets(_engine);
        }
    }

    int packetsOut(const struct lsquic_out_spec *out_spec, unsigned n_packets_out) {
        unsigned sent = 0;
        for (; sent < n_packets_out; ++sent) {
            const auto &spec = out_spec[sent];
            auto *route = static_cast<PacketRoute *>(spec.peer_ctx ? spec.peer_ctx : _route.get());
            if (!route) {
                break;
            }

            std::string local_ip = route->key.local_ip;
            std::string peer_ip = route->key.peer_ip;
            uint16_t local_port = route->key.local_port;
            uint16_t peer_port = route->key.peer_port;
            if ((local_ip.empty() || peer_ip.empty()) &&
                (!sockaddrToEndpoint(spec.local_sa, local_ip, local_port) ||
                 !sockaddrToEndpoint(spec.dest_sa, peer_ip, peer_port))) {
                break;
            }

            size_t payload_len = 0;
            for (size_t i = 0; i < spec.iovlen; ++i) {
                payload_len += spec.iov[i].iov_len;
            }

            std::string payload;
            payload.resize(payload_len);
            auto *dst = payload_len ? &payload[0] : nullptr;
            for (size_t i = 0; i < spec.iovlen; ++i) {
                if (spec.iov[i].iov_len == 0) {
                    continue;
                }
                std::memcpy(dst, spec.iov[i].iov_base, spec.iov[i].iov_len);
                dst += spec.iov[i].iov_len;
            }

            QuicPacketView packet;
            packet.payload.data = reinterpret_cast<const uint8_t *>(payload.data());
            packet.payload.len = payload.size();
            packet.local_ip = makeSlice(local_ip);
            packet.peer_ip = makeSlice(peer_ip);
            packet.local_port = local_port;
            packet.peer_port = peer_port;
            packet.ecn = static_cast<uint8_t>(spec.ecn);
            if (_host.sendPacket(packet) != 0) {
                break;
            }
        }

        if (sent == 0) {
            return -1;
        }
        return static_cast<int>(sent);
    }

    bool sendRequestHeaders(lsquic_stream_t *stream, const std::shared_ptr<ClientRequestCtx> &req) {
        std::vector<std::pair<std::string, std::string>> headers;
        headers.reserve(req->headers.size() + 4);
        headers.emplace_back(":method", req->method);
        headers.emplace_back(":scheme", req->scheme);
        headers.emplace_back(":path", req->path);
        headers.emplace_back(":authority", req->authority);
        headers.insert(headers.end(), req->headers.begin(), req->headers.end());

        std::vector<std::string> merged;
        std::vector<lsxpack_header> xhdr(headers.size());
        merged.reserve(headers.size());
        for (size_t i = 0; i < headers.size(); ++i) {
            merged.emplace_back(headers[i].first);
            merged.back().append(headers[i].second);
            lsxpack_header_set_offset2(&xhdr[i],
                                       merged[i].data(),
                                       0,
                                       headers[i].first.size(),
                                       headers[i].first.size(),
                                       headers[i].second.size());
        }

        lsquic_http_headers ls_headers = {
            static_cast<int>(xhdr.size()),
            xhdr.data(),
        };
        return lsquic_stream_send_headers(stream, &ls_headers, 0) == 0;
    }

    void emitHeaders(const std::shared_ptr<ClientRequestCtx> &req) {
        if (!req->header_set) {
            return;
        }

        std::vector<QuicHeader> headers;
        headers.reserve(req->header_set->headers.size());
        for (auto &header : req->header_set->headers) {
            QuicHeader item;
            item.name = makeSlice(header.first);
            item.value = makeSlice(header.second);
            headers.emplace_back(item);
        }

        QuicClientHeaders response;
        response.request_id = req->request_id;
        response.status_code = req->header_set->status_code;
        response.headers = headers.empty() ? nullptr : headers.data();
        response.header_count = headers.size();
        response.fin = false;
        _host.onHeaders(response);
        req->response_headers_emitted = true;
        req->state = QuicClientState::ResponseHeaders;
    }

    void onRead(lsquic_stream_t *stream, const std::shared_ptr<ClientRequestCtx> &req) {
        if (!req->response_headers_emitted) {
            req->header_set = static_cast<ClientHeaderSet *>(lsquic_stream_get_hset(stream));
            if (!req->header_set || req->header_set->status_code <= 0) {
                _host.log(QuicLogLevel::Warn, makeSlice("invalid HTTP/3 response headers"));
                lsquic_stream_close(stream);
                return;
            }
            emitHeaders(req);
        }

        char buf[16 * 1024];
        for (;;) {
            auto nread = lsquic_stream_read(stream, buf, sizeof(buf));
            if (nread > 0) {
                req->state = QuicClientState::ResponseBody;
                _host.onBody(req->request_id, reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(nread), false);
                continue;
            }
            if (nread == 0) {
                req->response_fin = true;
                req->state = QuicClientState::Completed;
                _host.onBody(req->request_id, nullptr, 0, true);
                lsquic_stream_shutdown(stream, 0);
                lsquic_stream_wantread(stream, 0);
                return;
            }
            if (errno == EWOULDBLOCK) {
                return;
            }
            _host.log(QuicLogLevel::Warn, makeSlice("lsquic client stream read failed"));
            lsquic_stream_close(stream);
            return;
        }
    }

    void onWrite(lsquic_stream_t *stream, const std::shared_ptr<ClientRequestCtx> &req) {
        if (!req->headers_sent) {
            if (!sendRequestHeaders(stream, req)) {
                _host.log(QuicLogLevel::Warn, makeSlice("lsquic client send headers failed"));
                lsquic_stream_close(stream);
                return;
            }
            req->headers_sent = true;
            if (req->request_fin && req->request_body.empty()) {
                req->write_shutdown = true;
                // Headers-only requests still need an explicit write shutdown so
                // the peer observes FIN and can produce a complete response.
                lsquic_stream_shutdown(stream, 1);
                lsquic_stream_wantwrite(stream, 0);
                lsquic_stream_wantread(stream, 1);
                return;
            }
        }

        while (!req->request_body.empty()) {
            auto &chunk = req->request_body.front();
            auto nwritten = lsquic_stream_write(stream,
                                                chunk.data() + req->request_body_offset,
                                                chunk.size() - req->request_body_offset);
            if (nwritten > 0) {
                req->request_body_offset += static_cast<size_t>(nwritten);
                if (req->request_body_offset == chunk.size()) {
                    req->request_body.pop_front();
                    req->request_body_offset = 0;
                }
                continue;
            }
            if (nwritten == 0) {
                // The lsquic write API reports backpressure on the write side
                // via a zero return value. Leave wantwrite armed and retry on
                // the next write callback instead of treating it as a failure.
                return;
            }
            if (errno == EWOULDBLOCK) {
                return;
            }
            _host.log(QuicLogLevel::Warn, makeSlice("lsquic client stream write failed"));
            lsquic_stream_close(stream);
            return;
        }

        if (req->request_fin && !req->write_shutdown) {
            req->write_shutdown = true;
            lsquic_stream_shutdown(stream, 1);
            lsquic_stream_wantread(stream, 1);
            lsquic_stream_wantwrite(stream, 0);
            return;
        }

        if (!req->request_fin) {
            lsquic_stream_wantwrite(stream, 0);
            lsquic_stream_wantread(stream, 1);
        }
    }

    void reportClose(const std::shared_ptr<ClientRequestCtx> &req, QuicClientState state,
                     uint64_t app_error_code, const std::string &reason) {
        if (!req || req->close_reported) {
            return;
        }

        req->close_reported = true;
        QuicClientClose close_info;
        close_info.request_id = req->request_id;
        close_info.state = state;
        close_info.app_error_code = app_error_code;
        close_info.reason = makeSlice(reason);
        _host.onRequestClosed(close_info);
    }

    static void destroyHeaderSet(ClientHeaderSet *header_set) {
        if (!header_set) {
            return;
        }
        for (auto *slot : header_set->slots) {
            delete[] slot->buffer;
            delete slot;
        }
        delete header_set;
    }

    static void *headerSetCreateShim(void *, lsquic_stream_t *, int) {
        return new ClientHeaderSet();
    }

    static lsxpack_header *headerSetPrepareDecodeShim(void *hset, lsxpack_header *hdr, size_t req_space) {
        auto *header_set = static_cast<ClientHeaderSet *>(hset);
        HeaderSlot *slot = nullptr;
        if (!hdr) {
            slot = new HeaderSlot();
            slot->capacity = req_space ? req_space : 0x100;
            slot->buffer = new char[slot->capacity];
            lsxpack_header_prepare_decode(&slot->xhdr, slot->buffer, 0, slot->capacity);
            header_set->slots.emplace_back(slot);
            return &slot->xhdr;
        }

        slot = reinterpret_cast<HeaderSlot *>(reinterpret_cast<char *>(hdr) - offsetof(HeaderSlot, xhdr));
        if (req_space <= slot->capacity) {
            return &slot->xhdr;
        }

        auto *new_buffer = new char[req_space];
        std::memcpy(new_buffer, slot->buffer, slot->capacity);
        delete[] slot->buffer;
        slot->buffer = new_buffer;
        slot->capacity = req_space;
        slot->xhdr.buf = slot->buffer;
        slot->xhdr.val_len = req_space;
        return &slot->xhdr;
    }

    static int headerSetProcessHeaderShim(void *hset, lsxpack_header *hdr) {
        auto *header_set = static_cast<ClientHeaderSet *>(hset);
        if (!hdr) {
            return header_set->status_code > 0 ? 0 : 1;
        }

        auto name = std::string(lsxpack_header_get_name(hdr), hdr->name_len);
        auto value = std::string(lsxpack_header_get_value(hdr), hdr->val_len);
        if (name == ":status") {
            header_set->status_code = std::atoi(value.c_str());
        } else if (!name.empty() && name[0] != ':') {
            header_set->headers.emplace_back(std::move(name), std::move(value));
        }
        return 0;
    }

    static void headerSetDiscardShim(void *hset) {
        destroyHeaderSet(static_cast<ClientHeaderSet *>(hset));
    }

    static lsquic_conn_ctx_t *onNewConnShim(void *ctx, lsquic_conn_t *conn) {
        auto *self = static_cast<LsquicClientEngine *>(ctx);
        auto *conn_ctx = reinterpret_cast<ClientConnCtx *>(lsquic_conn_get_ctx(conn));
        if (!conn_ctx) {
            conn_ctx = new ClientConnCtx();
            conn_ctx->engine = self;
        }
        conn_ctx->conn = conn;
        self->_conn_ctx = conn_ctx;
        return reinterpret_cast<lsquic_conn_ctx_t *>(conn_ctx);
    }

    static void onConnClosedShim(lsquic_conn_t *conn) {
        auto *conn_ctx = reinterpret_cast<ClientConnCtx *>(lsquic_conn_get_ctx(conn));
        if (!conn_ctx || !conn_ctx->engine) {
            return;
        }

        auto *engine = conn_ctx->engine;
        for (auto &entry : engine->_requests) {
            auto &req = entry.second;
            auto state = req->response_fin ? QuicClientState::Completed : QuicClientState::Failed;
            auto reason = engine->_shutdown_requested ? engine->_shutdown_reason
                                                      : (!req->close_reason.empty() ? req->close_reason
                                                                                    : (!engine->_peer_close_reason.empty()
                                                                                           ? engine->_peer_close_reason
                                                                                           : std::string("connection closed")));
            engine->reportClose(req, state, 0, reason);
        }
        engine->_requests.clear();
        engine->_conn_ctx = nullptr;
        engine->_route.reset();
        engine->_authority.clear();
        engine->_shutdown_requested = false;
        engine->_shutdown_reason.clear();
        engine->_peer_close_reason.clear();
        delete conn_ctx;
    }

    static lsquic_stream_ctx_t *onNewStreamShim(void *ctx, lsquic_stream_t *stream) {
        auto *self = static_cast<LsquicClientEngine *>(ctx);
        if (!stream || !self->_conn_ctx) {
            return nullptr;
        }

        while (!self->_conn_ctx->pending_requests.empty()) {
            auto request_id = self->_conn_ctx->pending_requests.front();
            self->_conn_ctx->pending_requests.pop_front();
            auto it = self->_requests.find(request_id);
            if (it == self->_requests.end()) {
                continue;
            }

            auto req = it->second;
            req->stream = stream;
            lsquic_stream_wantwrite(stream, 1);
            auto *stream_ctx = new ClientStreamCtx();
            stream_ctx->engine = self;
            stream_ctx->request = req;
            return reinterpret_cast<lsquic_stream_ctx_t *>(stream_ctx);
        }

        self->_host.log(QuicLogLevel::Warn, makeSlice("lsquic client received stream creation without a pending request"));
        lsquic_stream_close(stream);
        return nullptr;
    }

    static void onReadShim(lsquic_stream_t *stream, lsquic_stream_ctx_t *ctx) {
        auto *stream_ctx = reinterpret_cast<ClientStreamCtx *>(ctx);
        if (stream_ctx && stream_ctx->engine && stream_ctx->request) {
            stream_ctx->engine->onRead(stream, stream_ctx->request);
        }
    }

    static void onWriteShim(lsquic_stream_t *stream, lsquic_stream_ctx_t *ctx) {
        auto *stream_ctx = reinterpret_cast<ClientStreamCtx *>(ctx);
        if (stream_ctx && stream_ctx->engine && stream_ctx->request) {
            stream_ctx->engine->onWrite(stream, stream_ctx->request);
        }
    }

    static void onCloseShim(lsquic_stream_t *, lsquic_stream_ctx_t *ctx) {
        auto *stream_ctx = reinterpret_cast<ClientStreamCtx *>(ctx);
        if (!stream_ctx) {
            return;
        }

        auto req = stream_ctx->request;
        auto *engine = stream_ctx->engine;
        if (req) {
            destroyHeaderSet(req->header_set);
            req->header_set = nullptr;
            req->stream = nullptr;
            auto state = req->response_fin ? QuicClientState::Completed : req->state;
            if (state != QuicClientState::Completed && state != QuicClientState::Failed) {
                state = QuicClientState::Failed;
            }
            if (engine) {
                auto reason = state == QuicClientState::Completed ? std::string()
                                                                  : (!req->close_reason.empty() ? req->close_reason
                                                                                                : std::string("stream closed"));
                engine->reportClose(req, state, 0, reason);
                engine->_requests.erase(req->request_id);
            }
        }
        delete stream_ctx;
    }

    static void onResetShim(lsquic_stream_t *stream, lsquic_stream_ctx_t *ctx, int how) {
        auto *stream_ctx = reinterpret_cast<ClientStreamCtx *>(ctx);
        if (!stream_ctx || !stream_ctx->engine || !stream_ctx->request) {
            return;
        }

        auto &req = stream_ctx->request;
        req->reset_how = how;
        req->state = QuicClientState::Failed;
        std::ostringstream ss;
        ss << "stream reset by peer, how=" << how
           << ", stream_id=" << lsquic_stream_id(stream);
        req->close_reason = ss.str();
        stream_ctx->engine->_host.log(QuicLogLevel::Warn, makeSlice(req->close_reason));
    }

    static void onHskDoneShim(lsquic_conn_t *conn, enum lsquic_hsk_status status) {
        auto *conn_ctx = reinterpret_cast<ClientConnCtx *>(lsquic_conn_get_ctx(conn));
        if (!conn_ctx || !conn_ctx->engine) {
            return;
        }

        conn_ctx->handshake_done = status == LSQ_HSK_OK || status == LSQ_HSK_RESUMED_OK;
        auto *engine = conn_ctx->engine;
        switch (status) {
        case LSQ_HSK_OK:
        case LSQ_HSK_RESUMED_OK:
            engine->_host.log(QuicLogLevel::Info, makeSlice("lsquic client handshake completed"));
            break;
        case LSQ_HSK_FAIL:
        case LSQ_HSK_RESUMED_FAIL:
            engine->_host.log(QuicLogLevel::Warn, makeSlice("lsquic client handshake failed"));
            break;
        default:
            break;
        }
    }

    static void onConnCloseFrameReceivedShim(lsquic_conn_t *conn, int app_error, uint64_t error_code,
                                             const char *reason, int reason_len) {
        auto *conn_ctx = reinterpret_cast<ClientConnCtx *>(lsquic_conn_get_ctx(conn));
        if (!conn_ctx || !conn_ctx->engine) {
            return;
        }

        auto *engine = conn_ctx->engine;
        std::string reason_text;
        if (reason && reason_len > 0) {
            reason_text.assign(reason, reason_len);
        }
        std::ostringstream ss;
        ss << "peer CONNECTION_CLOSE"
           << ", app_error=" << app_error
           << ", error_code=" << error_code;
        if (!reason_text.empty()) {
            ss << ", reason=" << reason_text;
        }
        engine->_peer_close_reason = ss.str();
        engine->_host.log(QuicLogLevel::Warn, makeSlice(engine->_peer_close_reason));
    }

    static int packetsOutShim(void *ctx, const struct lsquic_out_spec *out_spec, unsigned n_packets_out) {
        return static_cast<LsquicClientEngine *>(ctx)->packetsOut(out_spec, n_packets_out);
    }

    static SSL_CTX *getSslCtxShim(void *peer_ctx, const struct sockaddr *) {
        auto *route = static_cast<PacketRoute *>(peer_ctx);
        return route && route->engine ? route->engine->_ssl_ctx.get() : nullptr;
    }

    static int verifyCertShim(void *ctx, STACK_OF(X509) *chain) {
        auto *self = static_cast<LsquicClientEngine *>(ctx);
        if (!self || !self->_ssl_ctx || !chain || sk_X509_num(chain) <= 0) {
            return -1;
        }

        auto *store_ctx = X509_STORE_CTX_new();
        if (!store_ctx) {
            return -1;
        }

        auto *cert = sk_X509_value(chain, 0);
        auto *store = SSL_CTX_get_cert_store(self->_ssl_ctx.get());
        auto ok = X509_STORE_CTX_init(store_ctx, store, cert, chain) == 1
               && X509_verify_cert(store_ctx) == 1;
        X509_STORE_CTX_free(store_ctx);
        return ok ? 0 : -1;
    }

    static const struct lsquic_stream_if &streamInterface() {
        static const struct lsquic_stream_if s_if = {
            &LsquicClientEngine::onNewConnShim,
            nullptr,
            &LsquicClientEngine::onConnClosedShim,
            &LsquicClientEngine::onNewStreamShim,
            &LsquicClientEngine::onReadShim,
            &LsquicClientEngine::onWriteShim,
            &LsquicClientEngine::onCloseShim,
            nullptr,
            nullptr,
            &LsquicClientEngine::onHskDoneShim,
            nullptr,
            nullptr,
            &LsquicClientEngine::onResetShim,
            &LsquicClientEngine::onConnCloseFrameReceivedShim,
        };
        return s_if;
    }

    static const struct lsquic_hset_if &headerSetInterface() {
        static const struct lsquic_hset_if s_if = {
            &LsquicClientEngine::headerSetCreateShim,
            &LsquicClientEngine::headerSetPrepareDecodeShim,
            &LsquicClientEngine::headerSetProcessHeaderShim,
            &LsquicClientEngine::headerSetDiscardShim,
            static_cast<enum lsquic_hsi_flag>(0),
        };
        return s_if;
    }

private:
    IQuicClientHost &_host;
    // All public entry points lock _mutex before driving lsquic, and lsquic then
    // invokes the stream/connection callbacks inline on that same call stack.
    // Keep callback-time mutations under this same invariant instead of adding a
    // second layer of locking inside the callbacks.
    mutable std::mutex _mutex;
    std::string _alpn;
    std::string _sni;
    std::string _ca_file;
    std::string _bind_ip;
    std::string _peer_host;
    uint16_t _local_port = 0;
    uint16_t _peer_port = 0;
    bool _verify_peer = true;
    bool _available = false;
    bool _shutdown_requested = false;
    std::string _shutdown_reason;
    std::string _peer_close_reason;
    std::string _authority;
    SslCtxPtr _ssl_ctx{nullptr, &SSL_CTX_free};
    struct lsquic_engine_settings _settings;
    struct lsquic_engine_api _api;
    lsquic_engine_t *_engine = nullptr;
    ClientConnCtx *_conn_ctx = nullptr;
    std::shared_ptr<PacketRoute> _route;
    std::unordered_map<uint64_t, std::shared_ptr<ClientRequestCtx>> _requests;
};

} // namespace

IQuicClientEngine *createLsquicClientEngine(IQuicClientHost &host, const QuicClientConfig &config) {
    auto *engine = new LsquicClientEngine(host, config);
    if (!engine->available()) {
        delete engine;
        return nullptr;
    }
    return engine;
}

} // namespace mediakit
