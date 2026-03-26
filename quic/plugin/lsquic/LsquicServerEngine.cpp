#include "LsquicServerEngine.h"

#include "LsquicCommon.h"

#include <lsxpack_header.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mediakit {

namespace {

using namespace lsquicplugin;

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
        _settings.es_cc_algo = static_cast<unsigned>(config.congestion_algo);

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

        {
            auto msg = std::string("lsquic server congestion control: ") + quicCongestionAlgoName(config.congestion_algo);
            _host.log(QuicLogLevel::Info, makeSlice(msg));
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
        PacketRoute *route = nullptr;
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

    struct StreamKey {
        uint64_t conn_id = 0;
        uint64_t stream_id = 0;

        bool operator==(const StreamKey &that) const {
            return conn_id == that.conn_id && stream_id == that.stream_id;
        }
    };

    struct StreamKeyHash {
        size_t operator()(const StreamKey &key) const {
            auto seed = std::hash<uint64_t>()(key.conn_id);
            seed ^= std::hash<uint64_t>()(key.stream_id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
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
        ctx->route = static_cast<PacketRoute *>(lsquic_conn_get_peer_ctx(conn, nullptr));
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
        if (ctx->conn->route) {
            request.local_ip = makeSlice(ctx->conn->route->local_ip);
            request.peer_ip = makeSlice(ctx->conn->route->peer_ip);
            request.local_port = ctx->conn->route->local_port;
            request.peer_port = ctx->conn->route->peer_port;
        }
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
            // lsquic exposes the decoded request header set on first readable
            // event; emit the host request once and switch the stream to body flow.
            ctx->header_set = static_cast<HeaderSet *>(lsquic_stream_get_hset(stream));
            if (!ctx->header_set || ctx->header_set->method.empty() || ctx->header_set->path.empty()) {
                _host.log(QuicLogLevel::Warn, makeSlice("invalid HTTP/3 request headers"));
                lsquic_stream_close(stream);
                return;
            }
            emitRequest(ctx);
            ctx->request_emitted = true;
        }

        char buf[16 * 1024];
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
            if (nwritten == 0) {
                // lsquic uses a zero write result to signal "retry later" on
                // the write side. Keep wantwrite enabled and return without
                // closing the stream.
                return;
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
            // HTTP/3 response completion is expressed as write-side shutdown on
            // the stream after queued body data has drained.
            lsquic_stream_shutdown(stream, 1);
            lsquic_stream_wantread(stream, 1);
            lsquic_stream_wantwrite(stream, 0);
        }
    }

    StreamKey makeStreamKey(uint64_t conn_id, uint64_t stream_id) const {
        StreamKey key;
        key.conn_id = conn_id;
        key.stream_id = stream_id;
        return key;
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
    // Like the client engine, host entry points hold _mutex while they drive the
    // lsquic engine. Stream/connection callbacks are expected to run inline under
    // that same lock and therefore mutate only engine-owned state.
    mutable std::mutex _mutex;
    std::string _alpn;
    std::string _alpn_wire;
    std::string _cert_file;
    std::string _key_file;
    std::string _key_password;
    std::string _key_password_copy;
    SslCtxPtr _ssl_ctx{nullptr, &SSL_CTX_free};
    struct lsquic_engine_settings _settings;
    struct lsquic_engine_api _api;
    lsquic_engine_t *_engine = nullptr;
    std::unordered_map<RouteKey, std::shared_ptr<PacketRoute>, RouteKeyHash> _route_map;
    std::unordered_map<uint64_t, lsquic_conn_t *> _conn_map;
    std::unordered_map<StreamKey, lsquic_stream_t *, StreamKeyHash> _stream_map;
    std::atomic<uint64_t> _next_conn_id{1};
    bool _available = false;
};

} // namespace

IQuicServerEngine *createLsquicServerEngine(IQuicServerHost &host, const QuicServerConfig &config) {
    auto *engine = new LsquicServerEngine(host, config);
    if (!engine->available()) {
        delete engine;
        return nullptr;
    }
    return engine;
}

} // namespace mediakit
