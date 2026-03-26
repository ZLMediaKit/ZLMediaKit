#ifndef ZLMEDIAKIT_QUICPLUGINBACKEND_H
#define ZLMEDIAKIT_QUICPLUGINBACKEND_H

#include <mutex>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Common/MediaSource.h"
#include "Common/Parser.h"
#include "Http/HttpBody.h"
#include "QuicBackend.h"
#include "QuicPlugin.h"

namespace mediakit {

class QuicPluginBackend final : public QuicBackend, public IQuicServerHost {
public:
    QuicPluginBackend();
    ~QuicPluginBackend() override;

    toolkit::EventPoller::Ptr queryPoller(const toolkit::Buffer::Ptr &buffer) override;
    void inputPacket(const QuicPacket &packet) override;
    void onManager(const toolkit::EventPoller::Ptr &poller) override;

public:
    void log(QuicLogLevel level, QuicSlice message) override;
    uint64_t nowMS() override;
    int sendPacket(const QuicPacketView &packet) override;
    void wakeEngine(uint64_t delay_ms) override;
    void onRequest(const QuicServerRequest &request) override;
    void onBody(uint64_t conn_id, uint64_t stream_id, const uint8_t *data, size_t len, bool fin) override;
    void onStreamClosed(const QuicServerStreamClose &close_info) override;
    void onConnectionClosed(const QuicServerConnectionClose &close_info) override;

private:
    struct PendingSession;
    struct PendingFlvSession;
    struct PendingRequest;
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
    struct RequestKey {
        uint64_t conn_id = 0;
        uint64_t stream_id = 0;

        bool operator==(const RequestKey &that) const {
            return conn_id == that.conn_id && stream_id == that.stream_id;
        }
    };

    struct RequestKeyHash {
        size_t operator()(const RequestKey &key) const {
            auto seed = std::hash<uint64_t>()(key.conn_id);
            seed ^= std::hash<uint64_t>()(key.stream_id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    static RouteKey makeRouteKey(QuicSlice local_ip, uint16_t local_port, QuicSlice peer_ip, uint16_t peer_port);
    static RouteKey makeRouteKey(const QuicPacket &packet);
    static RequestKey makeRequestKey(uint64_t conn_id, uint64_t stream_id);
    static bool extractPacketConnectionIds(const uint8_t *data, size_t len,
                                           std::string *dcid, std::string *scid);
    void rememberPacketConnectionIds(const uint8_t *data, size_t len, const toolkit::EventPoller::Ptr &poller);
    void rememberRoute(const QuicPacket &packet);
    std::shared_ptr<PendingRequest> removeRequest(uint64_t conn_id, uint64_t stream_id);
    std::vector<std::shared_ptr<PendingRequest>> removeConnectionRequests(uint64_t conn_id);
    void dispatchRequest(const std::shared_ptr<PendingRequest> &request);
    bool checkLiveStream(const std::shared_ptr<PendingRequest> &request, Parser &parser,
                         const std::string &schema, const std::string &url_suffix,
                         const std::function<void(const MediaSource::Ptr &src, const std::shared_ptr<PendingRequest> &request)> &cb);
    bool checkLiveStreamFlv(const std::shared_ptr<PendingRequest> &request, Parser &parser);
    bool checkLiveStreamTS(const std::shared_ptr<PendingRequest> &request, Parser &parser);
    bool checkLiveStreamFMP4(const std::shared_ptr<PendingRequest> &request, Parser &parser);
    static toolkit::Any makeSenderInfo(const std::weak_ptr<PendingRequest> &weak_request);
    void sendHttpBody(const std::shared_ptr<PendingRequest> &request,
                      const HttpBody::Ptr &body, size_t chunk_size);
    void sendTextResponse(const std::shared_ptr<PendingRequest> &request, int code,
                          const std::string &content_type, const std::string &text);
    void sendResponseHeaders(const std::shared_ptr<PendingRequest> &request, int code,
                             const StrCaseMap &header_out, int64_t body_size,
                             bool no_content_length, bool fin);
    void sendResponseBody(const std::shared_ptr<PendingRequest> &request,
                          const uint8_t *data, size_t len, bool fin);
private:
    QuicPluginRef _plugin;
    IQuicPlugin *_plugin_raw = nullptr;
    IQuicServerEngine *_engine = nullptr;
    mutable std::mutex _cid_mutex;
    std::unordered_map<std::string, toolkit::EventPoller::Ptr> _cid_poller_map;
    mutable std::mutex _route_mutex;
    std::unordered_map<RouteKey, std::weak_ptr<toolkit::Socket>, RouteKeyHash> _route_map;
    mutable std::mutex _request_mutex;
    std::unordered_map<RequestKey, std::shared_ptr<PendingRequest>, RequestKeyHash> _request_map;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_QUICPLUGINBACKEND_H
