#include "QuicPluginBackend.h"

#include "Common/config.h"
#include "Common/macros.h"
#include "Network/Buffer.h"
#include "Network/Session.h"
#include "Http/HttpConst.h"
#include "Http/HttpFileManager.h"
#include "Http/HttpProtocolHint.h"
#include "Http/HttpRequestDispatcher.h"
#include "Http/HttpServerTypes.h"
#include "Rtmp/FlvMuxer.h"
#include "Rtmp/RtmpMediaSource.h"
#include "TS/TSMediaSource.h"
#include "FMP4/FMP4MediaSource.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/util.h"

#include <ctime>
#include <sstream>

namespace mediakit {

using namespace toolkit;

namespace {

thread_local toolkit::EventPoller::Ptr s_quic_request_poller;
thread_local std::string s_quic_request_local_ip;
thread_local std::string s_quic_request_peer_ip;
thread_local uint16_t s_quic_request_local_port = 0;
thread_local uint16_t s_quic_request_peer_port = 0;

static std::string quicSliceToString(QuicSlice slice) {
    if (!slice.data || !slice.len) {
        return std::string();
    }
    return std::string(slice.data, slice.len);
}

static QuicSlice makeSlice(const char *str) {
    QuicSlice ret;
    ret.data = str;
    ret.len = str ? strlen(str) : 0;
    return ret;
}

static std::string dateStr() {
    char buf[64];
    time_t tt = time(nullptr);
    strftime(buf, sizeof(buf), "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

static constexpr size_t kDefaultQuicCidLen = 8;

} // namespace

struct QuicPluginBackend::PendingSession : public toolkit::Session {
    PendingSession(const toolkit::EventPoller::Ptr &poller,
                   std::string local_ip,
                   uint16_t local_port,
                   std::string peer_ip,
                   uint16_t peer_port,
                   std::string identifier)
        : Session(toolkit::Socket::createSocket(poller, false)),
          _local_ip(std::move(local_ip)),
          _local_port(local_port),
          _peer_ip(std::move(peer_ip)),
          _peer_port(peer_port),
          _identifier(std::move(identifier)) {}

    std::string get_local_ip() override {
        return _local_ip;
    }

    uint16_t get_local_port() override {
        return _local_port;
    }

    std::string get_peer_ip() override {
        return _peer_ip;
    }

    uint16_t get_peer_port() override {
        return _peer_port;
    }

    std::string getIdentifier() const override {
        return _identifier;
    }

    void shutdown(const toolkit::SockException &) override {}

    void onRecv(const toolkit::Buffer::Ptr &) override {}
    void onError(const toolkit::SockException &) override {}
    void onManager() override {}

private:
    std::string _local_ip;
    uint16_t _local_port = 0;
    std::string _peer_ip;
    uint16_t _peer_port = 0;
    std::string _identifier;
};

struct QuicPluginBackend::PendingRequest {
    uint64_t conn_id = 0;
    uint64_t stream_id = 0;
    std::string method;
    std::string scheme;
    std::string authority;
    std::string path;
    StrCaseMap headers;
    std::string body;
    std::shared_ptr<PendingSession> sender;
    bool request_received = false;
    bool body_finished = false;
    bool dispatched = false;
    bool live_stream = false;
    std::shared_ptr<PendingFlvSession> flv_session;
    TSMediaSource::RingType::RingReader::Ptr ts_reader;
    FMP4MediaSource::RingType::RingReader::Ptr fmp4_reader;
};

struct QuicPluginBackend::PendingFlvSession : public PendingSession,
                                              public FlvMuxer {
    PendingFlvSession(QuicPluginBackend &owner, const std::shared_ptr<PendingRequest> &request)
        : PendingSession(request->sender->getPoller(),
                         request->sender->get_local_ip(),
                         request->sender->get_local_port(),
                         request->sender->get_peer_ip(),
                         request->sender->get_peer_port(),
                         request->sender->getIdentifier()),
          _owner(owner),
          _request(request) {}

    void setSelf(const std::shared_ptr<PendingFlvSession> &self) {
        _self = self;
    }

    void startStream(const RtmpMediaSource::Ptr &src, uint32_t start_pts) {
        start(getPoller(), src, start_pts);
    }

protected:
    void onWrite(const toolkit::Buffer::Ptr &data, bool) override {
        auto request = _request.lock();
        if (!request || !data) {
            return;
        }
        _owner.sendResponseBody(request, reinterpret_cast<const uint8_t *>(data->data()), data->size(), false);
    }

    void onDetach() override {
        auto request = _request.lock();
        if (!request) {
            return;
        }
        _owner.sendResponseBody(request, nullptr, 0, true);
    }

    std::shared_ptr<FlvMuxer> getSharedPtr() override {
        return std::static_pointer_cast<FlvMuxer>(_self.lock());
    }

private:
    QuicPluginBackend &_owner;
    std::weak_ptr<PendingFlvSession> _self;
    std::weak_ptr<PendingRequest> _request;
};

QuicPluginBackend::QuicPluginBackend() {
    _plugin_raw = zlm_quic_plugin_create();
    _plugin = QuicPluginRef(_plugin_raw);
    if (!_plugin.valid()) {
        WarnL << "failed to create QUIC plugin instance";
        return;
    }
    if (!_plugin.hasServer()) {
        WarnL << "QUIC plugin does not expose a server engine";
        return;
    }

    QuicServerConfig config;
    config.alpn = makeSlice("h3");
    auto cert_file = toolkit::exeDir() + "default.pem";
    config.cert_file.data = cert_file.data();
    config.cert_file.len = cert_file.size();
    config.key_file.data = cert_file.data();
    config.key_file.len = cert_file.size();
    config.idle_timeout_ms = 30000;
    config.max_udp_payload_size = 1350;
    config.enable_retry = false;
    config.enable_h3_datagram = false;
    _engine = _plugin.createServerEngine(*this, config);
    if (!_engine) {
        WarnL << "failed to create QUIC server engine from plugin: " << _plugin.pluginName();
    } else {
        InfoL << "QUIC backend initialized via shared plugin: " << _plugin.pluginName();
    }
}

QuicPluginBackend::~QuicPluginBackend() {
    if (_engine) {
        _plugin.destroyServerEngine(_engine);
        _engine = nullptr;
    }
    if (_plugin_raw) {
        zlm_quic_plugin_destroy(_plugin_raw);
        _plugin_raw = nullptr;
    }
}

toolkit::EventPoller::Ptr QuicPluginBackend::queryPoller(const toolkit::Buffer::Ptr &buffer) {
    if (!buffer) {
        return nullptr;
    }

    std::string dcid;
    if (!extractPacketConnectionIds(reinterpret_cast<const uint8_t *>(buffer->data()), buffer->size(), &dcid, nullptr) ||
        dcid.empty()) {
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(_cid_mutex);
        auto it = _cid_poller_map.find(dcid);
        if (it != _cid_poller_map.end()) {
            return it->second;
        }
    }

    auto poller = toolkit::EventPollerPool::Instance().getPoller(false);
    if (poller) {
        std::lock_guard<std::mutex> lock(_cid_mutex);
        _cid_poller_map.emplace(std::move(dcid), poller);
    }
    return poller;
}

void QuicPluginBackend::inputPacket(const QuicPacket &packet) {
    if (!_engine || !packet.buffer) {
        return;
    }

    rememberPacketConnectionIds(reinterpret_cast<const uint8_t *>(packet.buffer->data()), packet.buffer->size(), packet.poller);
    rememberRoute(packet);
    s_quic_request_poller = packet.poller;
    s_quic_request_local_ip = packet.local_ip;
    s_quic_request_peer_ip = packet.peer_ip;
    s_quic_request_local_port = packet.local_port;
    s_quic_request_peer_port = packet.peer_port;

    QuicPacketView view;
    view.payload.data = reinterpret_cast<const uint8_t *>(packet.buffer->data());
    view.payload.len = packet.buffer->size();
    view.local_ip.data = packet.local_ip.data();
    view.local_ip.len = packet.local_ip.size();
    view.peer_ip.data = packet.peer_ip.data();
    view.peer_ip.len = packet.peer_ip.size();
    view.local_port = packet.local_port;
    view.peer_port = packet.peer_port;
    auto rc = _engine->handlePacket(view);
    TraceL << "host->plugin QUIC packet handled, size=" << view.payload.len
           << ", local=" << packet.local_ip << ":" << packet.local_port
           << ", peer=" << packet.peer_ip << ":" << packet.peer_port
           << ", rc=" << rc;

    s_quic_request_poller = nullptr;
    s_quic_request_local_ip.clear();
    s_quic_request_peer_ip.clear();
    s_quic_request_local_port = 0;
    s_quic_request_peer_port = 0;
}

void QuicPluginBackend::onManager(const toolkit::EventPoller::Ptr &) {
    if (_engine) {
        _engine->onTimer(nowMS());
    }
}

void QuicPluginBackend::log(QuicLogLevel level, QuicSlice message) {
    auto text = quicSliceToString(message);
    switch (level) {
    case QuicLogLevel::Trace:
        TraceL << "[quic-plugin] " << text;
        break;
    case QuicLogLevel::Debug:
        DebugL << "[quic-plugin] " << text;
        break;
    case QuicLogLevel::Info:
        InfoL << "[quic-plugin] " << text;
        break;
    case QuicLogLevel::Warn:
        WarnL << "[quic-plugin] " << text;
        break;
    case QuicLogLevel::Error:
        ErrorL << "[quic-plugin] " << text;
        break;
    }
}

uint64_t QuicPluginBackend::nowMS() {
    return toolkit::getCurrentMillisecond();
}

int QuicPluginBackend::sendPacket(const QuicPacketView &packet) {
    rememberPacketConnectionIds(packet.payload.data, packet.payload.len, toolkit::EventPoller::getCurrentPoller());

    toolkit::Socket::Ptr sock;
    auto route_key = makeRouteKey(packet.local_ip, packet.local_port, packet.peer_ip, packet.peer_port);
    {
        std::lock_guard<std::mutex> lock(_route_mutex);
        auto it = _route_map.find(route_key);
        if (it != _route_map.end()) {
            sock = it->second.lock();
            if (!sock) {
                _route_map.erase(it);
            }
        }
    }

    if (!sock) {
        WarnL << "failed to route QUIC packet to " << quicSliceToString(packet.peer_ip) << ":" << packet.peer_port
              << ", local=" << quicSliceToString(packet.local_ip) << ":" << packet.local_port;
        return -1;
    }

    auto buffer = toolkit::BufferRaw::create();
    buffer->assign(reinterpret_cast<const char *>(packet.payload.data), packet.payload.len);
    auto ret = sock->send(buffer);
    if (ret < 0) {
        WarnL << "failed to send QUIC packet to " << quicSliceToString(packet.peer_ip) << ":" << packet.peer_port
              << ", size=" << packet.payload.len;
        return -1;
    }
    TraceL << "plugin->host QUIC packet sent, size=" << packet.payload.len
           << ", local=" << quicSliceToString(packet.local_ip) << ":" << packet.local_port
           << ", peer=" << quicSliceToString(packet.peer_ip) << ":" << packet.peer_port;
    return 0;
}

void QuicPluginBackend::wakeEngine(uint64_t) {}

void QuicPluginBackend::onRequest(const QuicServerRequest &request) {
    auto pending = std::make_shared<PendingRequest>();
    pending->conn_id = request.conn_id;
    pending->stream_id = request.stream_id;
    pending->method = quicSliceToString(request.method);
    pending->scheme = quicSliceToString(request.scheme);
    pending->authority = quicSliceToString(request.authority);
    pending->path = quicSliceToString(request.path);
    pending->request_received = true;
    pending->body_finished = request.end_stream;
    auto poller = s_quic_request_poller ? s_quic_request_poller : toolkit::EventPollerPool::Instance().getPoller();
    pending->sender = std::make_shared<PendingSession>(poller,
                                                       s_quic_request_local_ip,
                                                       s_quic_request_local_port,
                                                       s_quic_request_peer_ip,
                                                       s_quic_request_peer_port,
                                                       StrPrinter << request.conn_id << "-" << request.stream_id);

    for (size_t i = 0; i < request.header_count; ++i) {
        pending->headers.emplace(quicSliceToString(request.headers[i].name), quicSliceToString(request.headers[i].value));
    }
    if (!pending->authority.empty()) {
        pending->headers.emplace("Host", pending->authority);
    }

    {
        std::lock_guard<std::mutex> lock(_request_mutex);
        _request_map[makeRequestKey(request.conn_id, request.stream_id)] = pending;
    }

    if (pending->body_finished) {
        dispatchRequest(pending);
    }
}

void QuicPluginBackend::onBody(uint64_t conn_id, uint64_t stream_id, const uint8_t *data, size_t len, bool fin) {
    std::shared_ptr<PendingRequest> pending;
    {
        std::lock_guard<std::mutex> lock(_request_mutex);
        auto it = _request_map.find(makeRequestKey(conn_id, stream_id));
        if (it == _request_map.end()) {
            return;
        }
        pending = it->second;
    }

    if (!pending) {
        return;
    }

    if (len) {
        GET_CONFIG(size_t, max_req_size, Http::kMaxReqSize);
        if (pending->body.size() + len > max_req_size) {
            auto body = std::make_shared<HttpStringBody>("request body too large");
            StrCaseMap headers;
            headers.emplace("content-type", "text/plain; charset=utf-8");
            sendResponseHeaders(pending, 413, headers, body->remainSize(), false, false);
            while (auto chunk = body->readData(64 * 1024)) {
                sendResponseBody(pending,
                                 reinterpret_cast<const uint8_t *>(chunk->data()),
                                 chunk->size(),
                                 body->remainSize() == 0);
            }
            pending->dispatched = true;
            pending->body_finished = true;
            return;
        }
        pending->body.append(reinterpret_cast<const char *>(data), len);
    }
    if (fin) {
        pending->body_finished = true;
        dispatchRequest(pending);
    }
}

void QuicPluginBackend::onStreamClosed(const QuicServerStreamClose &close_info) {
    std::lock_guard<std::mutex> lock(_request_mutex);
    _request_map.erase(makeRequestKey(close_info.conn_id, close_info.stream_id));
}

void QuicPluginBackend::onConnectionClosed(const QuicServerConnectionClose &close_info) {
    TraceL << "QUIC connection closed scaffold: conn=" << close_info.conn_id
           << ", err=" << close_info.app_error_code;
}

QuicPluginBackend::RouteKey QuicPluginBackend::makeRouteKey(QuicSlice local_ip, uint16_t local_port, QuicSlice peer_ip, uint16_t peer_port) {
    RouteKey key;
    key.local_ip = quicSliceToString(local_ip);
    key.local_port = local_port;
    key.peer_ip = quicSliceToString(peer_ip);
    key.peer_port = peer_port;
    return key;
}

QuicPluginBackend::RouteKey QuicPluginBackend::makeRouteKey(const QuicPacket &packet) {
    RouteKey key;
    key.local_ip = packet.local_ip;
    key.local_port = packet.local_port;
    key.peer_ip = packet.peer_ip;
    key.peer_port = packet.peer_port;
    return key;
}

void QuicPluginBackend::rememberRoute(const QuicPacket &packet) {
    if (!packet.sock) {
        return;
    }
    std::lock_guard<std::mutex> lock(_route_mutex);
    _route_map[makeRouteKey(packet)] = packet.sock;
}

uint64_t QuicPluginBackend::makeRequestKey(uint64_t conn_id, uint64_t stream_id) {
    return (conn_id << 32) ^ stream_id;
}

bool QuicPluginBackend::extractPacketConnectionIds(const uint8_t *data, size_t len,
                                                   std::string *dcid, std::string *scid) {
    if (!data || len < 2) {
        return false;
    }

    const auto first = data[0];
    if (first & 0x80) {
        if (len < 7) {
            return false;
        }

        size_t offset = 1 + 4;
        auto dcid_len = static_cast<size_t>(data[offset++]);
        if (dcid_len == 0 || offset + dcid_len + 1 > len) {
            return false;
        }
        if (dcid) {
            dcid->assign(reinterpret_cast<const char *>(data + offset), dcid_len);
        }
        offset += dcid_len;

        auto scid_len = static_cast<size_t>(data[offset++]);
        if (offset + scid_len > len) {
            return false;
        }
        if (scid) {
            scid->assign(reinterpret_cast<const char *>(data + offset), scid_len);
        }
        return true;
    }

    if (len < 1 + kDefaultQuicCidLen) {
        return false;
    }
    if (dcid) {
        dcid->assign(reinterpret_cast<const char *>(data + 1), kDefaultQuicCidLen);
    }
    if (scid) {
        scid->clear();
    }
    return true;
}

void QuicPluginBackend::rememberPacketConnectionIds(const uint8_t *data, size_t len, const toolkit::EventPoller::Ptr &poller) {
    if (!poller) {
        return;
    }

    std::string dcid;
    std::string scid;
    if (!extractPacketConnectionIds(data, len, &dcid, &scid)) {
        return;
    }

    std::lock_guard<std::mutex> lock(_cid_mutex);
    if (!dcid.empty()) {
        _cid_poller_map[dcid] = poller;
    }
    if (!scid.empty()) {
        _cid_poller_map[scid] = poller;
    }
}

void QuicPluginBackend::sendResponseHeaders(const std::shared_ptr<PendingRequest> &request, int code,
                                            const StrCaseMap &header_out, int64_t body_size,
                                            bool no_content_length, bool fin) {
    if (!request || !_engine) {
        return;
    }

    StrCaseMap response_headers = header_out;
    response_headers.emplace("server", kServerName);
    response_headers.emplace("date", dateStr());
    appendAltSvcHeader(response_headers);

    if (!no_content_length && body_size >= 0) {
        response_headers.emplace("content-length", std::to_string(body_size));
    }

    std::vector<std::pair<std::string, std::string>> normalized_headers;
    normalized_headers.reserve(response_headers.size());
    for (auto &header : response_headers) {
        normalized_headers.emplace_back(toolkit::strToLower(std::string(header.first)), header.second);
    }

    std::vector<QuicHeader> quic_headers;
    quic_headers.reserve(normalized_headers.size());
    for (auto &header : normalized_headers) {
        QuicHeader item;
        item.name.data = header.first.data();
        item.name.len = header.first.size();
        item.value.data = header.second.data();
        item.value.len = header.second.size();
        quic_headers.emplace_back(item);
    }

    _engine->sendHeaders(request->conn_id, request->stream_id, code,
                         quic_headers.empty() ? nullptr : quic_headers.data(),
                         quic_headers.size(), fin);
}

void QuicPluginBackend::sendResponseBody(const std::shared_ptr<PendingRequest> &request,
                                         const uint8_t *data, size_t len, bool fin) {
    if (!request || !_engine) {
        return;
    }
    _engine->sendBody(request->conn_id, request->stream_id, data, len, fin);
}

bool QuicPluginBackend::checkLiveStream(const std::shared_ptr<PendingRequest> &request, Parser &parser,
                                        const std::string &schema, const std::string &url_suffix,
                                        const std::function<void(const MediaSource::Ptr &src, const std::shared_ptr<PendingRequest> &request)> &cb) {
    if (!request || !request->sender) {
        return false;
    }

    std::string url = parser.url();
    auto it = parser.getUrlArgs().find("schema");
    if (it != parser.getUrlArgs().end()) {
        if (strcasecmp(it->second.c_str(), schema.c_str())) {
            return false;
        }
    } else {
        auto suffix_size = url_suffix.size();
        if (url.size() < suffix_size || strcasecmp(url.data() + (url.size() - suffix_size), url_suffix.data())) {
            return false;
        }
        url.resize(url.size() - suffix_size);
    }

    if (!parser.params().empty()) {
        url += "?";
        url += parser.params();
    }

    MediaInfo media_info;
    media_info.parse(schema + "://" + parser["Host"] + url);
    if (media_info.app.empty() || media_info.stream.empty()) {
        return false;
    }
    media_info.protocol = "https";
    GET_CONFIG(std::string, not_found, Http::kNotFound);

    std::weak_ptr<PendingRequest> weak_request = request;
    auto sender = request->sender;
    auto on_res = [this, weak_request, media_info, cb](const std::string &err) {
        auto request = weak_request.lock();
        if (!request) {
            return;
        }

        if (!err.empty()) {
            auto body = std::make_shared<HttpStringBody>(err);
            StrCaseMap headers;
            headers.emplace("content-type", "text/plain; charset=utf-8");
            sendResponseHeaders(request, 401, headers, body->remainSize(), false, false);
            while (auto chunk = body->readData(64 * 1024)) {
                sendResponseBody(request,
                                 reinterpret_cast<const uint8_t *>(chunk->data()),
                                 chunk->size(),
                                 body->remainSize() == 0);
            }
            return;
        }

        MediaSource::findAsync(media_info, request->sender, [this, weak_request, cb](const MediaSource::Ptr &src) {
            auto request = weak_request.lock();
            if (!request) {
                return;
            }
            if (!src) {
                auto body = std::make_shared<HttpStringBody>(not_found);
                StrCaseMap headers;
                headers.emplace("content-type", "text/html; charset=utf-8");
                sendResponseHeaders(request, 404, headers, body->remainSize(), false, false);
                while (auto chunk = body->readData(64 * 1024)) {
                    sendResponseBody(request,
                                     reinterpret_cast<const uint8_t *>(chunk->data()),
                                     chunk->size(),
                                     body->remainSize() == 0);
                }
                return;
            }
            request->live_stream = true;
            cb(src, request);
        });
    };

    Broadcast::AuthInvoker invoker = [sender, on_res](const std::string &err) {
        if (sender) {
            sender->async([on_res, err]() { on_res(err); }, false);
        }
    };

    auto flag = NOTICE_EMIT(BroadcastMediaPlayedArgs, Broadcast::kBroadcastMediaPlayed, media_info, invoker, *sender);
    if (!flag) {
        invoker("");
    }
    return true;
}

bool QuicPluginBackend::checkLiveStreamTS(const std::shared_ptr<PendingRequest> &request, Parser &parser) {
    return checkLiveStream(request, parser, TS_SCHEMA, ".live.ts",
                           [this](const MediaSource::Ptr &src, const std::shared_ptr<PendingRequest> &request) {
        auto ts_src = std::dynamic_pointer_cast<TSMediaSource>(src);
        if (!ts_src || !request || !request->sender) {
            return;
        }

        StrCaseMap headers;
        headers.emplace("content-type", HttpFileManager::getContentType(".ts"));
        sendResponseHeaders(request, 200, headers, -1, true, false);

        auto weak_request = std::weak_ptr<PendingRequest>(request);
        ts_src->pause(false);
        request->ts_reader = ts_src->getRing()->attach(request->sender->getPoller());
        request->ts_reader->setGetInfoCB([weak_request]() {
            toolkit::Any ret;
            if (auto request = weak_request.lock()) {
                ret.set(std::static_pointer_cast<toolkit::Session>(request->sender));
            }
            return ret;
        });
        request->ts_reader->setDetachCB([this, weak_request]() {
            if (auto request = weak_request.lock()) {
                request->ts_reader = nullptr;
                sendResponseBody(request, nullptr, 0, true);
            }
        });
        request->ts_reader->setReadCB([this, weak_request](const TSMediaSource::RingDataType &ts_list) {
            auto request = weak_request.lock();
            if (!request) {
                return;
            }
            ts_list->for_each([this, request](const TSPacket::Ptr &ts) {
                sendResponseBody(request,
                                 reinterpret_cast<const uint8_t *>(ts->data()),
                                 ts->size(),
                                 false);
            });
        });
    });
}

bool QuicPluginBackend::checkLiveStreamFlv(const std::shared_ptr<PendingRequest> &request, Parser &parser) {
    auto start_pts = static_cast<uint32_t>(atoll(parser.getUrlArgs()["starPts"].data()));
    return checkLiveStream(request, parser, RTMP_SCHEMA, ".live.flv",
                           [this, start_pts](const MediaSource::Ptr &src, const std::shared_ptr<PendingRequest> &request) {
        auto rtmp_src = std::dynamic_pointer_cast<RtmpMediaSource>(src);
        if (!rtmp_src || !request || !request->sender) {
            return;
        }

        auto tracks = src->getTracks(false);
        for (auto &track : tracks) {
            switch (track->getCodecId()) {
            case CodecH264:
            case CodecAAC:
                break;
            default:
                WarnL << "HTTP/3 FLV usually expects H264/AAC, codec may be unsupported by clients: "
                      << track->getCodecName();
                break;
            }
        }

        StrCaseMap headers;
        headers.emplace("cache-control", "no-store");
        headers.emplace("content-type", HttpFileManager::getContentType(".flv"));
        sendResponseHeaders(request, 200, headers, -1, true, false);

        auto flv_session = std::make_shared<PendingFlvSession>(*this, request);
        flv_session->setSelf(flv_session);
        request->flv_session = flv_session;
        flv_session->startStream(rtmp_src, start_pts);
    });
}

bool QuicPluginBackend::checkLiveStreamFMP4(const std::shared_ptr<PendingRequest> &request, Parser &parser) {
    return checkLiveStream(request, parser, FMP4_SCHEMA, ".live.mp4",
                           [this](const MediaSource::Ptr &src, const std::shared_ptr<PendingRequest> &request) {
        auto fmp4_src = std::dynamic_pointer_cast<FMP4MediaSource>(src);
        if (!fmp4_src || !request || !request->sender) {
            return;
        }

        StrCaseMap headers;
        headers.emplace("content-type", HttpFileManager::getContentType(".mp4"));
        sendResponseHeaders(request, 200, headers, -1, true, false);
        auto &init_segment = fmp4_src->getInitSegment();
        if (!init_segment.empty()) {
            sendResponseBody(request,
                             reinterpret_cast<const uint8_t *>(init_segment.data()),
                             init_segment.size(),
                             false);
        }

        auto weak_request = std::weak_ptr<PendingRequest>(request);
        fmp4_src->pause(false);
        request->fmp4_reader = fmp4_src->getRing()->attach(request->sender->getPoller());
        request->fmp4_reader->setGetInfoCB([weak_request]() {
            toolkit::Any ret;
            if (auto request = weak_request.lock()) {
                ret.set(std::static_pointer_cast<toolkit::Session>(request->sender));
            }
            return ret;
        });
        request->fmp4_reader->setDetachCB([this, weak_request]() {
            if (auto request = weak_request.lock()) {
                request->fmp4_reader = nullptr;
                sendResponseBody(request, nullptr, 0, true);
            }
        });
        request->fmp4_reader->setReadCB([this, weak_request](const FMP4MediaSource::RingDataType &fmp4_list) {
            auto request = weak_request.lock();
            if (!request) {
                return;
            }
            fmp4_list->for_each([this, request](const FMP4Packet::Ptr &pkt) {
                sendResponseBody(request,
                                 reinterpret_cast<const uint8_t *>(pkt->data()),
                                 pkt->size(),
                                 false);
            });
        });
    });
}

void QuicPluginBackend::dispatchRequest(const std::shared_ptr<PendingRequest> &request) {
    if (!request || request->dispatched || !request->request_received || !request->body_finished || !request->sender) {
        return;
    }
    request->dispatched = true;

    std::ostringstream raw;
    raw << (request->method.empty() ? "GET" : request->method) << " "
        << (request->path.empty() ? "/" : request->path) << " HTTP/3\r\n";
    for (auto &header : request->headers) {
        raw << header.first << ": " << header.second << "\r\n";
    }
    raw << "\r\n";

    Parser parser;
    auto text = raw.str();
    parser.parse(text.c_str(), text.size());
    parser.setContent(request->body);

    auto sender = request->sender;
    std::weak_ptr<PendingRequest> weak_request = request;
    HttpResponseInvokerImp invoker = [this, weak_request](int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body) {
        auto request = weak_request.lock();
        if (!request || !_engine) {
            return;
        }

        StrCaseMap response_headers = headerOut;
        auto body_size = body ? body->remainSize() : 0;
        if (body_size > 0 && response_headers.find("content-type") == response_headers.end()) {
            response_headers.emplace("content-type", "text/plain; charset=utf-8");
        }
        bool fin = !body || body_size == 0;
        sendResponseHeaders(request, code, response_headers, body_size, false, fin);
        if (!body || body_size == 0) {
            return;
        }

        GET_CONFIG(uint32_t, send_buf_size, Http::kSendBufSize);
        while (auto chunk = body->readData(send_buf_size)) {
            sendResponseBody(request,
                             reinterpret_cast<const uint8_t *>(chunk->data()),
                             chunk->size(),
                             body->remainSize() == 0);
        }
    };

    sender->getPoller()->async([this, sender, parser, invoker, weak_request]() mutable {
        auto request = weak_request.lock();
        if (!request) {
            return;
        }

        if (strcasecmp(parser.method().data(), "OPTIONS") == 0) {
            StrCaseMap header;
            header.emplace("Allow", "GET, POST, HEAD, OPTIONS");
            GET_CONFIG(bool, allow_cross_domains, Http::kAllowCrossDomains);
            if (allow_cross_domains) {
                header.emplace("Access-Control-Allow-Origin", "*");
                header.emplace("Access-Control-Allow-Headers", "*");
                header.emplace("Access-Control-Allow-Methods", "GET, POST, HEAD, OPTIONS");
            }
            header.emplace("Access-Control-Allow-Credentials", "true");
            header.emplace("Access-Control-Request-Methods", "GET, POST, OPTIONS");
            header.emplace("Access-Control-Request-Headers", "Accept,Accept-Language,Content-Language,Content-Type");
            invoker(200, header, HttpBody::Ptr());
            return;
        }

        if (strcasecmp(parser.method().data(), "HEAD") == 0) {
            invoker(200, StrCaseMap(), HttpBody::Ptr());
            return;
        }

        if (strcasecmp(parser.method().data(), "GET") == 0) {
            if (HttpRequestDispatcher::emitHttpEvent(parser, *sender, invoker, false)) {
                return;
            }
            if (checkLiveStreamFlv(request, parser)) {
                return;
            }
            if (checkLiveStreamTS(request, parser)) {
                return;
            }
            if (checkLiveStreamFMP4(request, parser)) {
                return;
            }
            try {
                HttpRequestDispatcher::onAccessPath(*sender, parser, [invoker](int code, const std::string &content_type,
                                                                               const StrCaseMap &responseHeader, const HttpBody::Ptr &body) mutable {
                    StrCaseMap headers = responseHeader;
                    if (!content_type.empty() && headers.find("content-type") == headers.end()) {
                        headers.emplace("content-type", content_type);
                    }
                    invoker(code, headers, body);
                }, nullptr);
            } catch (std::exception &ex) {
                WarnL << "HTTP/3 access path failed: " << ex.what();
                invoker(403, StrCaseMap(), std::make_shared<HttpStringBody>("forbidden"));
            }
            return;
        }

        HttpRequestDispatcher::emitHttpEvent(parser, *sender, invoker, true);
    }, false);
}

} // namespace mediakit
