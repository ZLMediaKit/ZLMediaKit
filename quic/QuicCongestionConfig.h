#ifndef ZLMEDIAKIT_QUICCONGESTIONCONFIG_H
#define ZLMEDIAKIT_QUICCONGESTIONCONFIG_H

#include <string>

#include "Common/config.h"
#include "QuicPluginABI.h"
#include "Util/logger.h"

namespace mediakit {

inline QuicCongestionAlgo parseQuicCongestionAlgoConfig(const std::string &key, const std::string &value,
                                                        QuicCongestionAlgo fallback) {
    if (value.empty()) {
        return fallback;
    }

    auto algo = quicCongestionAlgoFromString(value);
    if (algo == QuicCongestionAlgo::Default && value != "default" && value != "0") {
        WarnL << "invalid " << key << "=" << value << ", fallback to " << quicCongestionAlgoName(fallback);
        return fallback;
    }
    return algo;
}

inline QuicCongestionAlgo loadDefaultQuicCongestionAlgoConfig() {
    GET_CONFIG_FUNC(QuicCongestionAlgo, s_quic_cc_algo_default, Http::kQuicCongestionControl, [](const std::string &value) {
        return parseQuicCongestionAlgoConfig(Http::kQuicCongestionControl, value, QuicCongestionAlgo::BBRv1);
    });
    return s_quic_cc_algo_default;
}

inline QuicCongestionAlgo loadServerQuicCongestionAlgoConfig() {
    GET_CONFIG_FUNC(QuicCongestionAlgo, s_quic_cc_algo_server, Http::kQuicServerCongestionControl, [](const std::string &value) {
        return parseQuicCongestionAlgoConfig(Http::kQuicServerCongestionControl, value, loadDefaultQuicCongestionAlgoConfig());
    });
    return s_quic_cc_algo_server;
}

inline QuicCongestionAlgo loadClientQuicCongestionAlgoConfig() {
    GET_CONFIG_FUNC(QuicCongestionAlgo, s_quic_cc_algo_client, Http::kQuicClientCongestionControl, [](const std::string &value) {
        return parseQuicCongestionAlgoConfig(Http::kQuicClientCongestionControl, value, loadDefaultQuicCongestionAlgoConfig());
    });
    return s_quic_cc_algo_client;
}

} // namespace mediakit

#endif // ZLMEDIAKIT_QUICCONGESTIONCONFIG_H
