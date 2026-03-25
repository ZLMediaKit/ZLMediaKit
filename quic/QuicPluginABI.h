#ifndef ZLMEDIAKIT_QUICPLUGINABI_H
#define ZLMEDIAKIT_QUICPLUGINABI_H

#include <algorithm>
#include <cctype>
#include <stddef.h>
#include <stdint.h>
#include <string>

#if defined(_WIN32) && defined(_MSC_VER)
#if defined(ZLM_QUIC_PLUGIN_EXPORTS)
#define ZLM_QUIC_PLUGIN_API __declspec(dllexport)
#else
#define ZLM_QUIC_PLUGIN_API __declspec(dllimport)
#endif
#else
#define ZLM_QUIC_PLUGIN_API __attribute__((visibility("default")))
#endif

namespace mediakit {

static constexpr uint32_t kQuicPluginABIVersion = 0x00010000u;

struct QuicSlice {
    const char *data = nullptr;
    size_t len = 0;
};

struct QuicBytes {
    const uint8_t *data = nullptr;
    size_t len = 0;
};

struct QuicHeader {
    QuicSlice name;
    QuicSlice value;
};

enum class QuicLogLevel : uint32_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4
};

enum class QuicCloseSource : uint32_t {
    Transport = 0,
    Application = 1,
    IdleTimeout = 2,
    Local = 3
};

enum class QuicClientState : uint32_t {
    Connecting = 0,
    Connected = 1,
    ResponseHeaders = 2,
    ResponseBody = 3,
    Completed = 4,
    Failed = 5
};

enum class QuicCongestionAlgo : uint32_t {
    Default = 0,
    Cubic = 1,
    BBRv1 = 2,
    Adaptive = 3
};

inline QuicCongestionAlgo quicCongestionAlgoFromString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (value == "cubic" || value == "1") {
        return QuicCongestionAlgo::Cubic;
    }
    if (value == "bbr" || value == "bbrv1" || value == "2") {
        return QuicCongestionAlgo::BBRv1;
    }
    if (value == "adaptive" || value == "3") {
        return QuicCongestionAlgo::Adaptive;
    }
    return QuicCongestionAlgo::Default;
}

inline const char *quicCongestionAlgoName(QuicCongestionAlgo algo) {
    switch (algo) {
    case QuicCongestionAlgo::Cubic:
        return "cubic";
    case QuicCongestionAlgo::BBRv1:
        return "bbr";
    case QuicCongestionAlgo::Adaptive:
        return "adaptive";
    case QuicCongestionAlgo::Default:
    default:
        return "default";
    }
}

struct QuicPacketView {
    QuicBytes payload;
    QuicSlice local_ip;
    QuicSlice peer_ip;
    uint16_t local_port = 0;
    uint16_t peer_port = 0;
    uint8_t ecn = 0;
};

struct QuicServerConfig {
    uint32_t abi_version = kQuicPluginABIVersion;
    QuicSlice alpn;
    QuicSlice cert_file;
    QuicSlice key_file;
    QuicSlice key_password;
    uint32_t idle_timeout_ms = 0;
    uint16_t max_udp_payload_size = 0;
    bool enable_retry = false;
    bool enable_h3_datagram = false;
    QuicCongestionAlgo congestion_algo = QuicCongestionAlgo::Default;
};

struct QuicClientConfig {
    uint32_t abi_version = kQuicPluginABIVersion;
    QuicSlice alpn;
    QuicSlice sni;
    QuicSlice ca_file;
    QuicSlice bind_ip;
    QuicSlice peer_host;
    uint32_t connect_timeout_ms = 0;
    uint32_t idle_timeout_ms = 0;
    uint16_t local_port = 0;
    uint16_t peer_port = 0;
    bool verify_peer = true;
    QuicCongestionAlgo congestion_algo = QuicCongestionAlgo::Default;
};

struct QuicServerRequest {
    uint64_t conn_id = 0;
    uint64_t stream_id = 0;
    QuicSlice method;
    QuicSlice scheme;
    QuicSlice authority;
    QuicSlice path;
    const QuicHeader *headers = nullptr;
    size_t header_count = 0;
    bool end_stream = false;
};

struct QuicServerStreamClose {
    uint64_t conn_id = 0;
    uint64_t stream_id = 0;
    uint64_t app_error_code = 0;
    QuicCloseSource source = QuicCloseSource::Transport;
};

struct QuicServerConnectionClose {
    uint64_t conn_id = 0;
    uint64_t app_error_code = 0;
    QuicCloseSource source = QuicCloseSource::Transport;
    QuicSlice reason;
};

struct QuicClientRequestDesc {
    uint32_t abi_version = kQuicPluginABIVersion;
    uint64_t request_id = 0;
    QuicSlice scheme;
    QuicSlice authority;
    QuicSlice path;
    QuicSlice method;
    const QuicHeader *headers = nullptr;
    size_t header_count = 0;
    bool end_stream = false;
};

struct QuicClientHeaders {
    uint64_t request_id = 0;
    int32_t status_code = 0;
    const QuicHeader *headers = nullptr;
    size_t header_count = 0;
    bool fin = false;
};

struct QuicClientClose {
    uint64_t request_id = 0;
    QuicClientState state = QuicClientState::Connecting;
    uint64_t app_error_code = 0;
    QuicSlice reason;
};

class IQuicServerHost {
public:
    virtual ~IQuicServerHost() = default;

    virtual void log(QuicLogLevel level, QuicSlice message) = 0;
    virtual uint64_t nowMS() = 0;
    virtual int sendPacket(const QuicPacketView &packet) = 0;
    virtual void wakeEngine(uint64_t delay_ms) = 0;
    virtual void onRequest(const QuicServerRequest &request) = 0;
    virtual void onBody(uint64_t conn_id, uint64_t stream_id, const uint8_t *data, size_t len, bool fin) = 0;
    virtual void onStreamClosed(const QuicServerStreamClose &close_info) = 0;
    virtual void onConnectionClosed(const QuicServerConnectionClose &close_info) = 0;
};

class IQuicClientHost {
public:
    virtual ~IQuicClientHost() = default;

    virtual void log(QuicLogLevel level, QuicSlice message) = 0;
    virtual uint64_t nowMS() = 0;
    virtual int sendPacket(const QuicPacketView &packet) = 0;
    virtual void wakeEngine(uint64_t delay_ms) = 0;
    virtual void onHeaders(const QuicClientHeaders &headers) = 0;
    virtual void onBody(uint64_t request_id, const uint8_t *data, size_t len, bool fin) = 0;
    virtual void onRequestClosed(const QuicClientClose &close_info) = 0;
};

class IQuicServerEngine {
public:
    virtual ~IQuicServerEngine() = default;

    virtual int handlePacket(const QuicPacketView &packet) = 0;
    virtual int onTimer(uint64_t now_ms) = 0;
    virtual uint64_t nextTimeoutMS(uint64_t now_ms) const = 0;
    virtual int sendHeaders(uint64_t conn_id, uint64_t stream_id, int32_t status_code, const QuicHeader *headers, size_t header_count, bool fin) = 0;
    virtual int sendBody(uint64_t conn_id, uint64_t stream_id, const uint8_t *data, size_t len, bool fin) = 0;
    virtual int resetStream(uint64_t conn_id, uint64_t stream_id, uint64_t app_error_code) = 0;
    virtual int closeConnection(uint64_t conn_id, uint64_t app_error_code, QuicSlice reason) = 0;
};

class IQuicClientEngine {
public:
    virtual ~IQuicClientEngine() = default;

    virtual int handlePacket(const QuicPacketView &packet) = 0;
    virtual int onTimer(uint64_t now_ms) = 0;
    virtual uint64_t nextTimeoutMS(uint64_t now_ms) const = 0;
    virtual int startRequest(const QuicClientRequestDesc &request) = 0;
    virtual int sendBody(uint64_t request_id, const uint8_t *data, size_t len, bool fin) = 0;
    virtual int cancelRequest(uint64_t request_id, uint64_t app_error_code) = 0;
    virtual int shutdown(uint64_t app_error_code, QuicSlice reason) = 0;
};

struct QuicPluginInfo {
    uint32_t abi_version = kQuicPluginABIVersion;
    QuicSlice plugin_name;
    bool has_server = false;
    bool has_client = false;
};

class IQuicPlugin {
public:
    virtual ~IQuicPlugin() = default;

    virtual QuicPluginInfo pluginInfo() const = 0;
    virtual IQuicServerEngine *createServerEngine(IQuicServerHost &host, const QuicServerConfig &config) = 0;
    virtual void destroyServerEngine(IQuicServerEngine *engine) = 0;
    virtual IQuicClientEngine *createClientEngine(IQuicClientHost &host, const QuicClientConfig &config) = 0;
    virtual void destroyClientEngine(IQuicClientEngine *engine) = 0;
};

} // namespace mediakit

extern "C" {

ZLM_QUIC_PLUGIN_API uint32_t zlm_quic_plugin_abi_version(void);
ZLM_QUIC_PLUGIN_API mediakit::IQuicPlugin *zlm_quic_plugin_create(void);
ZLM_QUIC_PLUGIN_API void zlm_quic_plugin_destroy(mediakit::IQuicPlugin *plugin);

}

#endif // ZLMEDIAKIT_QUICPLUGINABI_H
