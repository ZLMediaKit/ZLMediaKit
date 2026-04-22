#ifndef ZLMEDIAKIT_QUICCLIENTBACKEND_H
#define ZLMEDIAKIT_QUICCLIENTBACKEND_H

#include <functional>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Network/UdpClient.h"
#include "Poller/EventPoller.h"
#include "QuicPlugin.h"

namespace mediakit {

struct QuicClientOwnedHeader {
    std::string name;
    std::string value;
};

struct QuicClientRequest {
    uint64_t request_id = 0;
    std::string scheme = "https";
    std::string authority;
    std::string path = "/";
    std::string method = "GET";
    std::vector<QuicClientOwnedHeader> headers;
    bool end_stream = false;
};

struct QuicClientResponseHeaders {
    uint64_t request_id = 0;
    int32_t status_code = 0;
    std::vector<QuicClientOwnedHeader> headers;
    bool fin = false;
};

struct QuicClientCloseInfo {
    uint64_t request_id = 0;
    QuicClientState state = QuicClientState::Connecting;
    uint64_t app_error_code = 0;
    std::string reason;
};

struct QuicClientPeerEndpoint {
    std::string ip;
    uint16_t port = 0;
    int family = AF_UNSPEC;
};

class QuicClientBackend final : public IQuicClientHost, public std::enable_shared_from_this<QuicClientBackend> {
public:
    using Ptr = std::shared_ptr<QuicClientBackend>;

    struct Callbacks {
        std::function<void(QuicLogLevel level, const std::string &message)> on_log;
        std::function<void(const QuicClientResponseHeaders &headers)> on_headers;
        std::function<void(uint64_t request_id, const uint8_t *data, size_t len, bool fin)> on_body;
        std::function<void(const QuicClientCloseInfo &close_info)> on_close;
    };

    explicit QuicClientBackend(const toolkit::EventPoller::Ptr &poller = nullptr);
    ~QuicClientBackend() override;

    void setCallbacks(Callbacks callbacks);
    void setNetAdapter(std::string local_ip);
    const toolkit::EventPoller::Ptr &getPoller() const;

    bool available() const;
    bool alive() const;

    int start(const std::string &peer_host, uint16_t peer_port, const QuicClientConfig &config);
    int startRequest(const QuicClientRequest &request);
    int sendBody(uint64_t request_id, const uint8_t *data, size_t len, bool fin);
    int cancelRequest(uint64_t request_id, uint64_t app_error_code);
    int shutdown(uint64_t app_error_code = 0, const std::string &reason = std::string());

public:
    void log(QuicLogLevel level, QuicSlice message) override;
    uint64_t nowMS() override;
    int sendPacket(const QuicPacketView &packet) override;
    void wakeEngine(uint64_t delay_ms) override;
    void onHeaders(const QuicClientHeaders &headers) override;
    void onBody(uint64_t request_id, const uint8_t *data, size_t len, bool fin) override;
    void onRequestClosed(const QuicClientClose &close_info) override;

private:
    class UdpTransport final : public toolkit::UdpClient {
    public:
        UdpTransport(QuicClientBackend &owner, const toolkit::EventPoller::Ptr &poller);
        void detachOwner();
        void closeQuietly();

    protected:
        void onRecvFrom(const toolkit::Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) override;
        void onError(const toolkit::SockException &err) override;

    private:
        QuicClientBackend *_owner = nullptr;
    };

    struct RequestState {
        QuicClientRequest request;
        std::vector<QuicHeader> header_views;
    };

private:
    int startOnPoller(const std::string &peer_host, uint16_t peer_port, const QuicClientConfig &config);
    int startResolvedEndpoint(const QuicClientPeerEndpoint &endpoint);
    int startRequestOnPoller(const QuicClientRequest &request);
    int sendBodyOnPoller(uint64_t request_id, const uint8_t *data, size_t len, bool fin);
    int cancelRequestOnPoller(uint64_t request_id, uint64_t app_error_code);
    bool shouldRetryNextEndpoint() const;
    bool restartOnNextEndpoint();
    void onTransportPacket(const toolkit::Buffer::Ptr &buf, struct sockaddr *addr);
    void onTransportError(const toolkit::SockException &err);
    void closeTransport();
    void resetEngine();
    void flushTransport();
    void refreshTimer();
    void armTimer(uint64_t delay_ms);
    uint64_t runTimerTask(uint64_t timer_seq);

private:
    toolkit::EventPoller::Ptr _poller;
    QuicPluginRef _plugin;
    IQuicPlugin *_plugin_raw = nullptr;
    IQuicClientEngine *_engine = nullptr;
    std::shared_ptr<UdpTransport> _transport;
    std::string _net_adapter;
    std::string _peer_host;
    uint16_t _peer_port = 0;
    std::string _peer_resolve_host;
    uint16_t _peer_resolve_port = 0;
    std::vector<QuicClientPeerEndpoint> _peer_endpoints;
    size_t _peer_endpoint_index = 0;
    std::string _alpn;
    std::string _sni;
    std::string _ca_file;
    std::string _bind_ip;
    std::string _local_ip;
    uint16_t _local_port = 0;
    uint32_t _connect_timeout_ms = 0;
    uint32_t _idle_timeout_ms = 0;
    uint16_t _configured_local_port = 0;
    bool _verify_peer = true;
    QuicCongestionAlgo _congestion_algo = QuicCongestionAlgo::Default;
    bool _handshake_completed = false;
    Callbacks _callbacks;
    std::unordered_map<uint64_t, RequestState> _requests;
    std::mutex _timer_mutex;
    toolkit::EventPoller::DelayTask::Ptr _timer_task;
    uint64_t _timer_due_ms = 0;
    uint64_t _timer_seq = 0;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_QUICCLIENTBACKEND_H
