#include "../../QuicPluginABI.h"

#ifdef ZLM_QUIC_HAS_LSQUIC
#include <lsquic.h>
#include <lsxpack_header.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace mediakit {

namespace {

static QuicSlice makeSlice(const char *str) {
    QuicSlice ret;
    ret.data = str;
    ret.len = str ? std::char_traits<char>::length(str) : 0;
    return ret;
}

static QuicSlice makeSlice(const std::string &str) {
    QuicSlice ret;
    ret.data = str.data();
    ret.len = str.size();
    return ret;
}

static std::string toString(QuicSlice slice) {
    if (!slice.data || !slice.len) {
        return std::string();
    }
    return std::string(slice.data, slice.len);
}

static std::string toErrorString(int err) {
#ifdef ZLM_QUIC_HAS_LSQUIC
    auto *msg = ERR_error_string(err, nullptr);
    return msg ? std::string(msg) : std::string("unknown error");
#else
    (void) err;
    return std::string("unavailable");
#endif
}

#ifdef ZLM_QUIC_HAS_LSQUIC
static std::mutex &lsquicRuntimeMutex() {
    static std::mutex s_mutex;
    return s_mutex;
}

static size_t &lsquicRuntimeRefCount() {
    static size_t s_ref_count = 0;
    return s_ref_count;
}

static bool acquireLsquicRuntime() {
    std::lock_guard<std::mutex> lock(lsquicRuntimeMutex());
    auto &ref_count = lsquicRuntimeRefCount();
    if (ref_count == 0) {
        if (lsquic_global_init(LSQUIC_GLOBAL_CLIENT | LSQUIC_GLOBAL_SERVER) != 0) {
            return false;
        }
    }
    ++ref_count;
    return true;
}

static void releaseLsquicRuntime() {
    std::lock_guard<std::mutex> lock(lsquicRuntimeMutex());
    auto &ref_count = lsquicRuntimeRefCount();
    if (ref_count == 0) {
        return;
    }
    if (--ref_count == 0) {
        lsquic_global_cleanup();
    }
}

static bool sliceToSockaddr(QuicSlice ip, uint16_t port, sockaddr_storage &storage, socklen_t &len) {
    auto text = toString(ip);
    if (text.empty()) {
        return false;
    }

    std::memset(&storage, 0, sizeof(storage));
    sockaddr_in addr4;
    std::memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons(port);
    if (inet_pton(AF_INET, text.c_str(), &addr4.sin_addr) == 1) {
        std::memcpy(&storage, &addr4, sizeof(addr4));
        len = sizeof(addr4);
        return true;
    }

    sockaddr_in6 addr6;
    std::memset(&addr6, 0, sizeof(addr6));
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(port);
    if (inet_pton(AF_INET6, text.c_str(), &addr6.sin6_addr) == 1) {
        std::memcpy(&storage, &addr6, sizeof(addr6));
        len = sizeof(addr6);
        return true;
    }

    return false;
}

static bool sockaddrToEndpoint(const sockaddr *addr, std::string &ip, uint16_t &port) {
    if (!addr) {
        return false;
    }

    char buf[INET6_ADDRSTRLEN] = {0};
    switch (addr->sa_family) {
    case AF_INET: {
        auto *addr4 = reinterpret_cast<const sockaddr_in *>(addr);
        if (!inet_ntop(AF_INET, &addr4->sin_addr, buf, sizeof(buf))) {
            return false;
        }
        ip = buf;
        port = ntohs(addr4->sin_port);
        return true;
    }
    case AF_INET6: {
        auto *addr6 = reinterpret_cast<const sockaddr_in6 *>(addr);
        if (!inet_ntop(AF_INET6, &addr6->sin6_addr, buf, sizeof(buf))) {
            return false;
        }
        ip = buf;
        port = ntohs(addr6->sin6_port);
        return true;
    }
    default:
        return false;
    }
}

class LsquicServerEngine final : public IQuicServerEngine {
public:
    explicit LsquicServerEngine(IQuicServerHost &host, const QuicServerConfig &config)
        : _host(host),
          _alpn(toString(config.alpn).empty() ? std::string("h3") : toString(config.alpn)),
          _cert_file(toString(config.cert_file)),
          _key_file(toString(config.key_file).empty() ? _cert_file : toString(config.key_file)),
          _key_password(toString(config.key_password)) {
        if (_cert_file.empty() || _key_file.empty()) {
            _host.log(QuicLogLevel::Error, makeSlice("QUIC server requires cert_file/key_file"));
            return;
        }

        _alpn_wire.push_back(static_cast<char>(_alpn.size()));
        _alpn_wire.append(_alpn);
        if (!initTlsContext()) {
            return;
        }

        auto flags = LSENG_SERVER | LSENG_HTTP;
        lsquic_engine_init_settings(&_settings, flags);
        _settings.es_idle_timeout = std::max<unsigned>(1, config.idle_timeout_ms ? (config.idle_timeout_ms + 999) / 1000 : 30);
        if (config.max_udp_payload_size) {
            _settings.es_max_udp_payload_size_rx = config.max_udp_payload_size;
        }
        _settings.es_datagrams = config.enable_h3_datagram ? 1 : 0;

        char err_buf[256] = {0};
        if (lsquic_engine_check_settings(&_settings, flags, err_buf, sizeof(err_buf)) != 0) {
            std::string message = "invalid lsquic settings";
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
        _api.ea_packets_out = &LsquicServerEngine::packetsOutShim;
        _api.ea_packets_out_ctx = this;
        _api.ea_lookup_cert = &LsquicServerEngine::lookupCertShim;
        _api.ea_cert_lu_ctx = this;
        _api.ea_get_ssl_ctx = &LsquicServerEngine::getSslCtxShim;
        _api.ea_hsi_if = &headerSetInterface();

        _engine = lsquic_engine_new(flags, &_api);
        if (!_engine) {
            _host.log(QuicLogLevel::Error, makeSlice("lsquic_engine_new failed"));
            return;
        }

        _host.log(QuicLogLevel::Info, makeSlice("lsquic server engine initialized"));
        _available = true;
    }

    ~LsquicServerEngine() override {
        if (_engine) {
            lsquic_engine_destroy(_engine);
            _engine = nullptr;
        }
        _stream_map.clear();
        _conn_map.clear();
    }

    bool available() const {
        return _available;
    }

    int handlePacket(const QuicPacketView &packet) override {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_engine) {
            return -1;
        }

        auto route = getOrCreateRoute(packet);
        if (!route) {
            _host.log(QuicLogLevel::Warn, makeSlice("failed to build sockaddr for QUIC packet"));
            return -1;
        }

        auto rc = lsquic_engine_packet_in(_engine,
                                          packet.payload.data,
                                          packet.payload.len,
                                          reinterpret_cast<const sockaddr *>(&route->local_addr),
                                          reinterpret_cast<const sockaddr *>(&route->peer_addr),
                                          route.get(),
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

    int sendHeaders(uint64_t conn_id, uint64_t stream_id, int32_t status_code, const QuicHeader *headers, size_t header_count, bool fin) override {
        std::lock_guard<std::mutex> lock(_mutex);
        auto *ctx = findStreamCtx(conn_id, stream_id);
        auto *stream = findStream(conn_id, stream_id);
        if (!ctx || !stream) {
            return -1;
        }

        ctx->response_code = status_code;
        ctx->response_headers.clear();
        ctx->response_headers.reserve(header_count);
        for (size_t i = 0; i < header_count; ++i) {
            ctx->response_headers.emplace_back(toString(headers[i].name), toString(headers[i].value));
        }
        ctx->response_headers_ready = true;
        ctx->response_headers_sent = false;
        ctx->response_offset = 0;
        ctx->response_fin = fin && ctx->response_body.empty();

        lsquic_stream_wantwrite(stream, 1);
        driveEngine();
        return 0;
    }

    int sendBody(uint64_t conn_id, uint64_t stream_id, const uint8_t *data, size_t len, bool fin) override {
        std::lock_guard<std::mutex> lock(_mutex);
        auto *ctx = findStreamCtx(conn_id, stream_id);
        auto *stream = findStream(conn_id, stream_id);
        if (!ctx || !stream) {
            return -1;
        }

        if (len) {
            ctx->response_body.emplace_back(reinterpret_cast<const char *>(data), len);
        }
        if (fin) {
            ctx->response_fin = true;
        }

        lsquic_stream_wantwrite(stream, 1);
        driveEngine();
        return 0;
    }

    int resetStream(uint64_t, uint64_t, uint64_t) override {
        return -1;
    }

    int closeConnection(uint64_t conn_id, uint64_t, QuicSlice) override {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _conn_map.find(conn_id);
        if (it == _conn_map.end()) {
            return -1;
        }
        lsquic_conn_close(it->second);
        return 0;
    }

private:
    struct RouteKey {
        std::string local_ip;
        std::string peer_ip;
        uint16_t local_port = 0;
        uint16_t peer_port = 0;

        bool operator==(const RouteKey &that) const {
            return local_port == that.local_port && peer_port == that.peer_port &&
                   local_ip == that.local_ip && peer_ip == that.peer_ip;
        }
    };

    struct RouteKeyHash {
        size_t operator()(const RouteKey &key) const {
            auto seed = std::hash<std::string>()(key.local_ip);
            seed ^= std::hash<std::string>()(key.peer_ip) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<uint16_t>()(key.local_port) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<uint16_t>()(key.peer_port) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    struct PacketRoute {
        LsquicServerEngine *engine = nullptr;
        std::string local_ip;
        std::string peer_ip;
        uint16_t local_port = 0;
        uint16_t peer_port = 0;
        sockaddr_storage local_addr;
        sockaddr_storage peer_addr;
        socklen_t local_addr_len = 0;
        socklen_t peer_addr_len = 0;
    };

    struct ServerConnCtx {
        LsquicServerEngine *engine = nullptr;
        uint64_t conn_id = 0;
    };

    struct HeaderSlot {
        lsxpack_header xhdr;
        char *buffer = nullptr;
        size_t capacity = 0;
    };

    struct HeaderSet {
        std::vector<HeaderSlot *> slots;
        std::string method;
        std::string scheme;
        std::string authority;
        std::string path;
        std::vector<std::pair<std::string, std::string>> headers;
    };

    struct ServerStreamCtx {
        ServerConnCtx *conn = nullptr;
        uint64_t stream_id = 0;
        HeaderSet *header_set = nullptr;
        int32_t response_code = 200;
        std::vector<std::pair<std::string, std::string>> response_headers;
        std::deque<std::string> response_body;
        size_t response_offset = 0;
        bool request_emitted = false;
        bool response_headers_ready = false;
        bool response_headers_sent = false;
        bool response_fin = false;
        bool write_shutdown = false;
    };

    static RouteKey makeRouteKey(const QuicPacketView &packet) {
        RouteKey key;
        key.local_ip = toString(packet.local_ip);
        key.peer_ip = toString(packet.peer_ip);
        key.local_port = packet.local_port;
        key.peer_port = packet.peer_port;
        return key;
    }

    bool initTlsContext() {
        _ssl_ctx.reset(SSL_CTX_new(TLS_method()));
        if (!_ssl_ctx) {
            _host.log(QuicLogLevel::Error, makeSlice("SSL_CTX_new failed"));
            return false;
        }

        SSL_CTX_set_min_proto_version(_ssl_ctx.get(), TLS1_3_VERSION);
        SSL_CTX_set_max_proto_version(_ssl_ctx.get(), TLS1_3_VERSION);
        SSL_CTX_set_default_verify_paths(_ssl_ctx.get());
        SSL_CTX_set_alpn_select_cb(_ssl_ctx.get(), &LsquicServerEngine::selectAlpnShim, this);
#if defined(SSL_CTX_set_early_data_enabled)
        SSL_CTX_set_early_data_enabled(_ssl_ctx.get(), 1);
#endif

        if (SSL_CTX_use_certificate_chain_file(_ssl_ctx.get(), _cert_file.c_str()) != 1) {
            auto msg = std::string("SSL_CTX_use_certificate_chain_file failed: ") + _cert_file;
            _host.log(QuicLogLevel::Error, makeSlice(msg));
            return false;
        }

        if (!_key_password.empty()) {
            _key_password_copy = _key_password;
            SSL_CTX_set_default_passwd_cb_userdata(_ssl_ctx.get(), &_key_password_copy);
            SSL_CTX_set_default_passwd_cb(_ssl_ctx.get(), &LsquicServerEngine::passwordCallbackShim);
        }

        if (SSL_CTX_use_PrivateKey_file(_ssl_ctx.get(), _key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
            auto msg = std::string("SSL_CTX_use_PrivateKey_file failed: ") + _key_file;
            _host.log(QuicLogLevel::Error, makeSlice(msg));
            return false;
        }

        if (SSL_CTX_check_private_key(_ssl_ctx.get()) != 1) {
            auto err = ERR_get_error();
            auto msg = std::string("SSL_CTX_check_private_key failed: ") + toErrorString(static_cast<int>(err));
            _host.log(QuicLogLevel::Error, makeSlice(msg));
            return false;
        }

        return true;
    }

    void driveEngine() {
        lsquic_engine_process_conns(_engine);
        while (lsquic_engine_has_unsent_packets(_engine)) {
            lsquic_engine_send_unsent_packets(_engine);
        }
    }

    std::shared_ptr<PacketRoute> getOrCreateRoute(const QuicPacketView &packet) {
        auto key = makeRouteKey(packet);
        auto it = _route_map.find(key);
        if (it != _route_map.end()) {
            return it->second;
        }

        auto route = std::make_shared<PacketRoute>();
        route->engine = this;
        route->local_ip = key.local_ip;
        route->peer_ip = key.peer_ip;
        route->local_port = key.local_port;
        route->peer_port = key.peer_port;
        if (!sliceToSockaddr(packet.local_ip, packet.local_port, route->local_addr, route->local_addr_len) ||
            !sliceToSockaddr(packet.peer_ip, packet.peer_port, route->peer_addr, route->peer_addr_len)) {
            return nullptr;
        }
        _route_map.emplace(std::move(key), route);
        return route;
    }

    int packetsOut(const struct lsquic_out_spec *out_spec, unsigned n_packets_out) {
        unsigned sent = 0;
        for (; sent < n_packets_out; ++sent) {
            const auto &spec = out_spec[sent];
            auto *route = static_cast<PacketRoute *>(spec.peer_ctx);
            if (!route) {
                break;
            }

            std::string local_ip = route->local_ip;
            std::string peer_ip = route->peer_ip;
            uint16_t local_port = route->local_port;
            uint16_t peer_port = route->peer_port;
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

    ServerConnCtx *createConnCtx(lsquic_conn_t *conn) {
        auto *ctx = new ServerConnCtx();
        ctx->engine = this;
        ctx->conn_id = _next_conn_id.fetch_add(1);
        _conn_map.emplace(ctx->conn_id, conn);
        return ctx;
    }

    ServerStreamCtx *createStreamCtx(lsquic_stream_t *stream) {
        auto *conn_ctx = reinterpret_cast<ServerConnCtx *>(lsquic_conn_get_ctx(lsquic_stream_conn(stream)));
        if (!conn_ctx) {
            return nullptr;
        }

        auto *ctx = new ServerStreamCtx();
        ctx->conn = conn_ctx;
        ctx->stream_id = lsquic_stream_id(stream);
        _stream_map.emplace(makeStreamKey(conn_ctx->conn_id, ctx->stream_id), stream);
        return ctx;
    }

    void emitRequest(ServerStreamCtx *ctx) {
        std::vector<QuicHeader> headers;
        headers.reserve(ctx->header_set->headers.size());
        for (auto &header : ctx->header_set->headers) {
            QuicHeader item;
            item.name = makeSlice(header.first);
            item.value = makeSlice(header.second);
            headers.emplace_back(item);
        }

        QuicServerRequest request;
        request.conn_id = ctx->conn->conn_id;
        request.stream_id = ctx->stream_id;
        request.method = makeSlice(ctx->header_set->method);
        request.scheme = makeSlice(ctx->header_set->scheme);
        request.authority = makeSlice(ctx->header_set->authority);
        request.path = makeSlice(ctx->header_set->path);
        request.headers = headers.empty() ? nullptr : headers.data();
        request.header_count = headers.size();
        request.end_stream = false;
        _host.onRequest(request);
    }

    void onRead(lsquic_stream_t *stream, ServerStreamCtx *ctx) {
        if (!ctx->request_emitted) {
            ctx->header_set = static_cast<HeaderSet *>(lsquic_stream_get_hset(stream));
            if (!ctx->header_set || ctx->header_set->method.empty() || ctx->header_set->path.empty()) {
                _host.log(QuicLogLevel::Warn, makeSlice("invalid HTTP/3 request headers"));
                lsquic_stream_close(stream);
                return;
            }
            emitRequest(ctx);
            ctx->request_emitted = true;
        }

        char buf[4096];
        for (;;) {
            auto nread = lsquic_stream_read(stream, buf, sizeof(buf));
            if (nread > 0) {
                _host.onBody(ctx->conn->conn_id, ctx->stream_id, reinterpret_cast<const uint8_t *>(buf), nread, false);
                continue;
            }
            if (nread == 0) {
                _host.onBody(ctx->conn->conn_id, ctx->stream_id, nullptr, 0, true);
                lsquic_stream_wantread(stream, 0);
                return;
            }
            if (errno == EWOULDBLOCK) {
                return;
            }
            _host.log(QuicLogLevel::Warn, makeSlice("lsquic_stream_read failed"));
            lsquic_stream_close(stream);
            return;
        }
    }

    bool sendResponseHeaders(lsquic_stream_t *stream, ServerStreamCtx *ctx) {
        std::vector<std::pair<std::string, std::string>> headers;
        headers.reserve(ctx->response_headers.size() + 1);
        headers.emplace_back(":status", std::to_string(ctx->response_code));
        headers.insert(headers.end(), ctx->response_headers.begin(), ctx->response_headers.end());

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

    void onWrite(lsquic_stream_t *stream, ServerStreamCtx *ctx) {
        if (!ctx->response_headers_ready) {
            return;
        }

        if (!ctx->response_headers_sent) {
            if (!sendResponseHeaders(stream, ctx)) {
                lsquic_stream_close(stream);
                return;
            }
            ctx->response_headers_sent = true;
        }

        while (!ctx->response_body.empty()) {
            auto &chunk = ctx->response_body.front();
            auto nwritten = lsquic_stream_write(stream, chunk.data() + ctx->response_offset, chunk.size() - ctx->response_offset);
            if (nwritten > 0) {
                ctx->response_offset += static_cast<size_t>(nwritten);
                if (ctx->response_offset == chunk.size()) {
                    ctx->response_body.pop_front();
                    ctx->response_offset = 0;
                }
                continue;
            }
            if (errno == EWOULDBLOCK) {
                return;
            }
            _host.log(QuicLogLevel::Warn, makeSlice("lsquic_stream_write failed"));
            lsquic_stream_close(stream);
            return;
        }

        if (ctx->response_fin && !ctx->write_shutdown) {
            ctx->write_shutdown = true;
            lsquic_stream_shutdown(stream, 1);
            lsquic_stream_wantwrite(stream, 0);
        }
    }

    uint64_t makeStreamKey(uint64_t conn_id, uint64_t stream_id) const {
        return (conn_id << 32) ^ stream_id;
    }

    void eraseStream(uint64_t conn_id, uint64_t stream_id) {
        _stream_map.erase(makeStreamKey(conn_id, stream_id));
    }

    lsquic_stream_t *findStream(uint64_t conn_id, uint64_t stream_id) {
        auto it = _stream_map.find(makeStreamKey(conn_id, stream_id));
        return it == _stream_map.end() ? nullptr : it->second;
    }

    ServerStreamCtx *findStreamCtx(uint64_t conn_id, uint64_t stream_id) {
        auto *stream = findStream(conn_id, stream_id);
        return stream ? reinterpret_cast<ServerStreamCtx *>(lsquic_stream_get_ctx(stream)) : nullptr;
    }

    static void destroyHeaderSet(HeaderSet *header_set) {
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
        return new HeaderSet();
    }

    static lsxpack_header *headerSetPrepareDecodeShim(void *hset, lsxpack_header *hdr, size_t req_space) {
        auto *header_set = static_cast<HeaderSet *>(hset);
        HeaderSlot *slot = nullptr;
        if (!hdr) {
            slot = new HeaderSlot();
            slot->capacity = req_space;
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
        auto *header_set = static_cast<HeaderSet *>(hset);
        if (!hdr) {
            return (!header_set->method.empty() && !header_set->path.empty()) ? 0 : 1;
        }

        auto name = std::string(lsxpack_header_get_name(hdr), hdr->name_len);
        auto value = std::string(lsxpack_header_get_value(hdr), hdr->val_len);
        if (name == ":method") {
            header_set->method = std::move(value);
        } else if (name == ":scheme") {
            header_set->scheme = std::move(value);
        } else if (name == ":authority") {
            header_set->authority = std::move(value);
        } else if (name == ":path") {
            header_set->path = std::move(value);
        } else if (!name.empty() && name[0] != ':') {
            header_set->headers.emplace_back(std::move(name), std::move(value));
        }
        return 0;
    }

    static void headerSetDiscardShim(void *hset) {
        destroyHeaderSet(static_cast<HeaderSet *>(hset));
    }

    static int passwordCallbackShim(char *buf, int size, int, void *userdata) {
        auto *password = static_cast<std::string *>(userdata);
        if (!password || size <= 0) {
            return 0;
        }
        auto copy_len = std::min<int>(size - 1, static_cast<int>(password->size()));
        std::memcpy(buf, password->data(), copy_len);
        buf[copy_len] = '\0';
        return copy_len;
    }

    static int selectAlpnShim(SSL *, const unsigned char **out, unsigned char *outlen,
                              const unsigned char *in, unsigned int inlen, void *arg) {
        auto *self = static_cast<LsquicServerEngine *>(arg);
        int rc = SSL_select_next_proto(const_cast<unsigned char **>(out),
                                       outlen,
                                       in,
                                       inlen,
                                       reinterpret_cast<const unsigned char *>(self->_alpn_wire.data()),
                                       static_cast<unsigned>(self->_alpn_wire.size()));
        return rc == OPENSSL_NPN_NEGOTIATED ? SSL_TLSEXT_ERR_OK : SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    static lsquic_conn_ctx_t *onNewConnShim(void *ctx, lsquic_conn_t *conn) {
        return reinterpret_cast<lsquic_conn_ctx_t *>(static_cast<LsquicServerEngine *>(ctx)->createConnCtx(conn));
    }

    static void onConnClosedShim(lsquic_conn_t *conn) {
        auto *ctx = reinterpret_cast<ServerConnCtx *>(lsquic_conn_get_ctx(conn));
        if (!ctx) {
            return;
        }

        QuicServerConnectionClose close_info;
        close_info.conn_id = ctx->conn_id;
        close_info.app_error_code = 0;
        close_info.source = QuicCloseSource::Transport;
        ctx->engine->_host.onConnectionClosed(close_info);
        ctx->engine->_conn_map.erase(ctx->conn_id);
        delete ctx;
    }

    static lsquic_stream_ctx_t *onNewStreamShim(void *ctx, lsquic_stream_t *stream) {
        auto *self = static_cast<LsquicServerEngine *>(ctx);
        auto *stream_ctx = self->createStreamCtx(stream);
        if (!stream_ctx) {
            return nullptr;
        }
        lsquic_stream_wantread(stream, 1);
        return reinterpret_cast<lsquic_stream_ctx_t *>(stream_ctx);
    }

    static void onReadShim(lsquic_stream_t *stream, lsquic_stream_ctx_t *ctx) {
        auto *stream_ctx = reinterpret_cast<ServerStreamCtx *>(ctx);
        if (stream_ctx && stream_ctx->conn && stream_ctx->conn->engine) {
            stream_ctx->conn->engine->onRead(stream, stream_ctx);
        }
    }

    static void onWriteShim(lsquic_stream_t *stream, lsquic_stream_ctx_t *ctx) {
        auto *stream_ctx = reinterpret_cast<ServerStreamCtx *>(ctx);
        if (stream_ctx && stream_ctx->conn && stream_ctx->conn->engine) {
            stream_ctx->conn->engine->onWrite(stream, stream_ctx);
        }
    }

    static void onCloseShim(lsquic_stream_t *, lsquic_stream_ctx_t *ctx) {
        auto *stream_ctx = reinterpret_cast<ServerStreamCtx *>(ctx);
        if (!stream_ctx || !stream_ctx->conn || !stream_ctx->conn->engine) {
            if (stream_ctx) {
                destroyHeaderSet(stream_ctx->header_set);
            }
            delete stream_ctx;
            return;
        }

        auto *engine = stream_ctx->conn->engine;
        QuicServerStreamClose close_info;
        close_info.conn_id = stream_ctx->conn->conn_id;
        close_info.stream_id = stream_ctx->stream_id;
        close_info.app_error_code = 0;
        close_info.source = QuicCloseSource::Transport;
        engine->_host.onStreamClosed(close_info);
        engine->eraseStream(stream_ctx->conn->conn_id, stream_ctx->stream_id);
        destroyHeaderSet(stream_ctx->header_set);
        delete stream_ctx;
    }

    static int packetsOutShim(void *ctx, const struct lsquic_out_spec *out_spec, unsigned n_packets_out) {
        return static_cast<LsquicServerEngine *>(ctx)->packetsOut(out_spec, n_packets_out);
    }

    static SSL_CTX *lookupCertShim(void *ctx, const struct sockaddr *, const char *) {
        auto *self = static_cast<LsquicServerEngine *>(ctx);
        return self->_ssl_ctx.get();
    }

    static SSL_CTX *getSslCtxShim(void *peer_ctx, const struct sockaddr *) {
        auto *route = static_cast<PacketRoute *>(peer_ctx);
        return route && route->engine ? route->engine->_ssl_ctx.get() : nullptr;
    }

    static const struct lsquic_stream_if &streamInterface() {
        static const struct lsquic_stream_if s_if = {
            &LsquicServerEngine::onNewConnShim,
            nullptr,
            &LsquicServerEngine::onConnClosedShim,
            &LsquicServerEngine::onNewStreamShim,
            &LsquicServerEngine::onReadShim,
            &LsquicServerEngine::onWriteShim,
            &LsquicServerEngine::onCloseShim,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
        };
        return s_if;
    }

    static const struct lsquic_hset_if &headerSetInterface() {
        static const struct lsquic_hset_if s_if = {
            &LsquicServerEngine::headerSetCreateShim,
            &LsquicServerEngine::headerSetPrepareDecodeShim,
            &LsquicServerEngine::headerSetProcessHeaderShim,
            &LsquicServerEngine::headerSetDiscardShim,
            static_cast<enum lsquic_hsi_flag>(0),
        };
        return s_if;
    }

private:
    IQuicServerHost &_host;
    mutable std::mutex _mutex;
    std::string _alpn;
    std::string _alpn_wire;
    std::string _cert_file;
    std::string _key_file;
    std::string _key_password;
    std::string _key_password_copy;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> _ssl_ctx{nullptr, &SSL_CTX_free};
    struct lsquic_engine_settings _settings;
    struct lsquic_engine_api _api;
    lsquic_engine_t *_engine = nullptr;
    std::unordered_map<RouteKey, std::shared_ptr<PacketRoute>, RouteKeyHash> _route_map;
    std::unordered_map<uint64_t, lsquic_conn_t *> _conn_map;
    std::unordered_map<uint64_t, lsquic_stream_t *> _stream_map;
    std::atomic<uint64_t> _next_conn_id{1};
    bool _available = false;
};
#endif

#ifdef ZLM_QUIC_HAS_LSQUIC
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
        return lsquic_stream_send_headers(stream, &ls_headers, req->request_fin && req->request_body.empty()) == 0;
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

        char buf[4096];
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
            auto reason = engine->_shutdown_requested ? engine->_shutdown_reason : std::string("connection closed");
            engine->reportClose(req, state, 0, reason);
        }
        engine->_requests.clear();
        engine->_conn_ctx = nullptr;
        engine->_route.reset();
        engine->_authority.clear();
        engine->_shutdown_requested = false;
        engine->_shutdown_reason.clear();
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
                engine->reportClose(req, state, 0, state == QuicClientState::Completed ? std::string() : std::string("stream closed"));
                engine->_requests.erase(req->request_id);
            }
        }
        delete stream_ctx;
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
            nullptr,
            nullptr,
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
    std::string _authority;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> _ssl_ctx{nullptr, &SSL_CTX_free};
    struct lsquic_engine_settings _settings;
    struct lsquic_engine_api _api;
    lsquic_engine_t *_engine = nullptr;
    ClientConnCtx *_conn_ctx = nullptr;
    std::shared_ptr<PacketRoute> _route;
    std::unordered_map<uint64_t, std::shared_ptr<ClientRequestCtx>> _requests;
};
#else
class LsquicClientEngine final : public IQuicClientEngine {
public:
    explicit LsquicClientEngine(IQuicClientHost &host, const QuicClientConfig &) : _host(host) {}

    int handlePacket(const QuicPacketView &) override {
        if (!_warned) {
            _warned = true;
            _host.log(QuicLogLevel::Warn, makeSlice("lsquic client engine is unavailable"));
        }
        return -1;
    }

    int onTimer(uint64_t) override { return -1; }
    uint64_t nextTimeoutMS(uint64_t now_ms) const override { return now_ms + 1000; }
    int startRequest(const QuicClientRequestDesc &) override { return -1; }
    int sendBody(uint64_t, const uint8_t *, size_t, bool) override { return -1; }
    int cancelRequest(uint64_t, uint64_t) override { return -1; }
    int shutdown(uint64_t, QuicSlice) override { return 0; }

private:
    IQuicClientHost &_host;
    bool _warned = false;
};
#endif

class LsquicPlugin final : public IQuicPlugin {
public:
    LsquicPlugin() {
#ifdef ZLM_QUIC_HAS_LSQUIC
        _available = acquireLsquicRuntime();
#else
        _available = false;
#endif
    }

    ~LsquicPlugin() override {
#ifdef ZLM_QUIC_HAS_LSQUIC
        if (_available) {
            releaseLsquicRuntime();
        }
#endif
    }

    QuicPluginInfo pluginInfo() const override {
        QuicPluginInfo info;
        info.plugin_name = makeSlice("lsquic-shared-plugin");
#ifdef ZLM_QUIC_HAS_LSQUIC
        info.has_server = _available;
        info.has_client = _available;
#else
        info.has_server = false;
        info.has_client = false;
#endif
        return info;
    }

    IQuicServerEngine *createServerEngine(IQuicServerHost &host, const QuicServerConfig &config) override {
#ifdef ZLM_QUIC_HAS_LSQUIC
        if (!_available) {
            host.log(QuicLogLevel::Error, makeSlice("failed to initialize private lsquic runtime"));
            return nullptr;
        }

        auto *engine = new LsquicServerEngine(host, config);
        if (!engine->available()) {
            delete engine;
            return nullptr;
        }
        return engine;
#else
        (void) host;
        (void) config;
        return nullptr;
#endif
    }

    void destroyServerEngine(IQuicServerEngine *engine) override {
        delete engine;
    }

    IQuicClientEngine *createClientEngine(IQuicClientHost &host, const QuicClientConfig &config) override {
        if (!_available) {
            host.log(QuicLogLevel::Error, makeSlice("failed to initialize private lsquic runtime"));
            return nullptr;
        }
        auto *engine = new LsquicClientEngine(host, config);
#ifdef ZLM_QUIC_HAS_LSQUIC
        if (!engine->available()) {
            delete engine;
            return nullptr;
        }
#endif
        return engine;
    }

    void destroyClientEngine(IQuicClientEngine *engine) override {
        delete engine;
    }

private:
    bool _available = false;
};

static IQuicPlugin *createPluginInstance() {
    return new LsquicPlugin();
}

} // namespace

} // namespace mediakit

extern "C" {

ZLM_QUIC_PLUGIN_API uint32_t zlm_quic_plugin_abi_version(void) {
    return mediakit::kQuicPluginABIVersion;
}

ZLM_QUIC_PLUGIN_API mediakit::IQuicPlugin *zlm_quic_plugin_create(void) {
    return mediakit::createPluginInstance();
}

ZLM_QUIC_PLUGIN_API void zlm_quic_plugin_destroy(mediakit::IQuicPlugin *plugin) {
    delete plugin;
}

}
