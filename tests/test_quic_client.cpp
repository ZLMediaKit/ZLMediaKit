/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <iostream>
#include <mutex>
#include <unordered_map>

#include "Common/config.h"
#include "Common/Parser.h"
#include "Thread/semaphore.h"
#include "Util/logger.h"
#include "quic/QuicClientBackend.h"
#include "quic/QuicPluginABI.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace {

struct RequestResult {
    int32_t status_code = 0;
    bool headers_received = false;
    bool body_finished = false;
    bool closed = false;
    QuicClientState close_state = QuicClientState::Connecting;
    string close_reason;
    string body;
};

static void dumpResultSnapshot(const char *prefix, uint64_t request_id, const RequestResult &result) {
    cerr << prefix
         << " request_id=" << request_id
         << " status=" << result.status_code
         << " headers=" << result.headers_received
         << " body_finished=" << result.body_finished
         << " closed=" << result.closed
         << " close_state=" << static_cast<uint32_t>(result.close_state)
         << " body_bytes=" << result.body.size()
         << " reason=" << result.close_reason
         << endl;
}

static string ensurePath(string path) {
    if (path.empty()) {
        return "/";
    }
    if (path.front() != '/') {
        path.insert(path.begin(), '/');
    }
    return path;
}

static bool isIpv6Literal(const string &host) {
    return host.find(':') != string::npos;
}

static QuicCongestionAlgo loadClientCongestionAlgo() {
    auto value = mINI::Instance()[Http::kQuicClientCongestionControl].as<string>();
    if (value.empty()) {
        value = mINI::Instance()[Http::kQuicCongestionControl].as<string>();
    }
    return quicCongestionAlgoFromString(value);
}

class ClientHarness {
public:
    bool start(const string &authority) {
        splitUrl(authority, _host, _port);
        if (_host.empty() || !_port) {
            cerr << "invalid authority: " << authority << endl;
            return false;
        }

        _authority = authority;
        _backend = make_shared<QuicClientBackend>();
        if (!_backend->available()) {
            cerr << "quic client backend is unavailable" << endl;
            return false;
        }

        if (isIpv6Literal(_host)) {
            _backend->setNetAdapter("::");
        }

        QuicClientBackend::Callbacks callbacks;
        callbacks.on_log = [](QuicLogLevel, const string &) {};
        callbacks.on_headers = [this](const QuicClientResponseHeaders &headers) {
            lock_guard<mutex> lock(_mtx);
            auto &result = _results[headers.request_id];
            result.status_code = headers.status_code;
            result.headers_received = true;
            dumpResultSnapshot("on_headers", headers.request_id, result);
        };
        callbacks.on_body = [this](uint64_t request_id, const uint8_t *data, size_t len, bool fin) {
            lock_guard<mutex> lock(_mtx);
            auto &result = _results[request_id];
            if (data && len) {
                result.body.append(reinterpret_cast<const char *>(data), len);
            }
            result.body_finished = fin;
            if (fin || len) {
                dumpResultSnapshot("on_body", request_id, result);
            }
        };
        callbacks.on_close = [this](const QuicClientCloseInfo &close_info) {
            {
                lock_guard<mutex> lock(_mtx);
                auto &result = _results[close_info.request_id];
                result.closed = true;
                result.close_state = close_info.state;
                result.close_reason = close_info.reason;
                dumpResultSnapshot("on_close", close_info.request_id, result);
            }
            _sem.post();
        };
        _backend->setCallbacks(std::move(callbacks));

        QuicClientConfig config;
        config.verify_peer = false;
        config.idle_timeout_ms = 10000;
        config.connect_timeout_ms = 5000;
        config.congestion_algo = loadClientCongestionAlgo();
        return _backend->start(_host, _port, config) == 0;
    }

    bool runGet(uint64_t request_id, string path, RequestResult &out, unsigned timeout_ms) {
        return runRequest(request_id, "GET", std::move(path), {}, nullptr, timeout_ms, out);
    }

    bool runPostForm(uint64_t request_id, string path, string body, RequestResult &out, unsigned timeout_ms) {
        vector<QuicClientOwnedHeader> headers;
        headers.emplace_back(QuicClientOwnedHeader{"content-type", "application/x-www-form-urlencoded"});
        headers.emplace_back(QuicClientOwnedHeader{"content-length", to_string(body.size())});
        return runRequest(request_id, "POST", std::move(path), std::move(headers), &body, timeout_ms, out);
    }

    void shutdown() {
        if (_backend) {
            _backend->shutdown();
        }
    }

private:
    bool runRequest(uint64_t request_id,
                    string method,
                    string path,
                    vector<QuicClientOwnedHeader> headers,
                    const string *body,
                    unsigned timeout_ms,
                    RequestResult &out) {
        QuicClientRequest request;
        request.request_id = request_id;
        request.authority = _authority;
        request.path = ensurePath(std::move(path));
        request.method = std::move(method);
        request.headers = std::move(headers);
        request.end_stream = !body || body->empty();

        {
            lock_guard<mutex> lock(_mtx);
            _results.erase(request_id);
        }

        if (_backend->startRequest(request) != 0) {
            cerr << "startRequest failed for " << request.path << endl;
            return false;
        }

        if (body && !body->empty()) {
            auto split = body->size() / 2;
            if (split == 0) {
                split = body->size();
            }
            if (_backend->sendBody(request_id,
                                   reinterpret_cast<const uint8_t *>(body->data()),
                                   split,
                                   split == body->size()) != 0) {
                cerr << "sendBody(first chunk) failed for " << request.path << endl;
                return false;
            }
            if (split < body->size() &&
                _backend->sendBody(request_id,
                                   reinterpret_cast<const uint8_t *>(body->data() + split),
                                   body->size() - split,
                                   true) != 0) {
                cerr << "sendBody(second chunk) failed for " << request.path << endl;
                return false;
            }
        }

        if (!waitClosed(request_id, timeout_ms, out)) {
            {
                lock_guard<mutex> lock(_mtx);
                auto it = _results.find(request_id);
                if (it != _results.end()) {
                    dumpResultSnapshot("timeout", request_id, it->second);
                }
            }
            cerr << "request timed out: " << request.path << endl;
            return false;
        }
        return true;
    }
    bool waitClosed(uint64_t request_id, unsigned timeout_ms, RequestResult &out) {
        auto deadline = getCurrentMillisecond() + timeout_ms;
        for (;;) {
            {
                lock_guard<mutex> lock(_mtx);
                auto it = _results.find(request_id);
                if (it != _results.end() && it->second.closed) {
                    out = it->second;
                    return true;
                }
            }

            auto now = getCurrentMillisecond();
            if (now >= deadline) {
                return false;
            }
            auto wait_ms = static_cast<unsigned>(std::min<uint64_t>(deadline - now, 200));
            _sem.wait(wait_ms);
        }
    }

private:
    semaphore _sem;
    mutex _mtx;
    string _authority;
    string _host;
    uint16_t _port = 0;
    QuicClientBackend::Ptr _backend;
    unordered_map<uint64_t, RequestResult> _results;
};

} // namespace

int main(int argc, char *argv[]) {
    Logger::Instance().add(make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(make_shared<AsyncLogWriter>());

    auto authority = argc > 1 ? string(argv[1]) : string("127.0.0.1:18443");
    auto root_path = argc > 2 ? string(argv[2]) : string("/");
    auto api_path = argc > 3 ? string(argv[3]) : string("/index/api/version");
    auto post_path = argc > 4 ? string(argv[4]) : string("/index/api/getServerConfig");
    auto post_body = argc > 5 ? string(argv[5]) : string("secret=quic-smoke-secret");

    signal(SIGPIPE, SIG_IGN);

    RequestResult root_result;
    {
        ClientHarness harness;
        if (!harness.start(authority)) {
            return 1;
        }
        if (!harness.runGet(1, root_path, root_result, 10000)) {
            harness.shutdown();
            return 2;
        }
        harness.shutdown();
    }

    RequestResult api_result;
    {
        ClientHarness harness;
        if (!harness.start(authority)) {
            return 1;
        }
        if (!harness.runGet(2, api_path, api_result, 10000)) {
            harness.shutdown();
            return 3;
        }
        harness.shutdown();
    }

    RequestResult post_result;
    {
        ClientHarness harness;
        if (!harness.start(authority)) {
            return 1;
        }
        if (!harness.runPostForm(3, post_path, post_body, post_result, 10000)) {
            harness.shutdown();
            return 6;
        }
        harness.shutdown();
    }

    if (root_result.close_state != QuicClientState::Completed || root_result.status_code != 200 || root_result.body.empty()) {
        cerr << "root request failed: status=" << root_result.status_code
             << " state=" << static_cast<uint32_t>(root_result.close_state)
             << " reason=" << root_result.close_reason << endl;
        return 4;
    }

    if (api_result.close_state != QuicClientState::Completed || api_result.status_code != 200 || api_result.body.find('{') == string::npos) {
        cerr << "api request failed: status=" << api_result.status_code
             << " state=" << static_cast<uint32_t>(api_result.close_state)
             << " reason=" << api_result.close_reason << endl;
        return 5;
    }

    if (post_result.close_state != QuicClientState::Completed ||
        post_result.status_code != 200 ||
        post_result.body.find("\"code\"") == string::npos ||
        post_result.body.find("\"data\"") == string::npos) {
        cerr << "post request failed: status=" << post_result.status_code
             << " state=" << static_cast<uint32_t>(post_result.close_state)
             << " reason=" << post_result.close_reason
             << " body=" << post_result.body << endl;
        return 7;
    }

    cout << "HTTP/3 GET " << ensurePath(root_path) << " ok, bytes=" << root_result.body.size() << endl;
    cout << "HTTP/3 GET " << ensurePath(api_path) << " ok, bytes=" << api_result.body.size() << endl;
    cout << "HTTP/3 POST " << ensurePath(post_path) << " ok, bytes=" << post_result.body.size() << endl;
    return 0;
}
