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
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "Http/HttpBody.h"
#include "Common/config.h"
#include "Http/HttpProtocolHint.h"
#include "Http/HttpRequester.h"
#include "Thread/semaphore.h"
#include "Util/logger.h"
#include "quic/QuicPluginABI.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace {

class ChunkedStringBody : public HttpBody {
public:
    ChunkedStringBody(std::string body, size_t chunk_size)
        : _body(std::move(body)), _chunk_size(std::max<size_t>(1, chunk_size)) {}

    int64_t remainSize() override {
        return static_cast<int64_t>(_body.size() - _offset);
    }

    toolkit::Buffer::Ptr readData(size_t size) override {
        auto remain = static_cast<size_t>(remainSize());
        if (!remain) {
            return nullptr;
        }
        size = std::min(remain, std::min(size, _chunk_size));
        auto ret = std::make_shared<toolkit::BufferString>(_body, _offset, size);
        _offset += size;
        return ret;
    }

    bool snapshot(std::string &out, size_t max_size) const override {
        if (_body.size() > max_size || _offset > _body.size()) {
            return false;
        }
        out.assign(_body.data() + _offset, _body.size() - _offset);
        return true;
    }

private:
    size_t _offset = 0;
    std::string _body;
    size_t _chunk_size = 1;
};

static bool looksLikeOkJsonResponse(const Parser &parser) {
    auto body = parser.content();
    return parser.status() == "200" &&
           body.find("\"code\"") != string::npos &&
           body.find("\"data\"") != string::npos;
}

static const string &responseVersion(const Parser &parser) {
    return parser.method();
}

static pair<string, uint16_t> getOriginFromUrl(const string &url) {
    auto protocol = findSubString(url.data(), nullptr, "://");
    uint16_t default_port = 0;
    if (strcasecmp(protocol.data(), "http") == 0) {
        default_port = 80;
    } else if (strcasecmp(protocol.data(), "https") == 0) {
        default_port = 443;
    }

    auto host = findSubString(url.data(), "://", "/");
    if (host.empty()) {
        host = findSubString(url.data(), "://", nullptr);
    }

    splitUrl(host, host, default_port);
    return make_pair(host, default_port);
}

static bool parseUint64(const string &value, uint64_t &out) {
    if (value.empty()) {
        return false;
    }
    try {
        out = std::stoull(value);
        return true;
    } catch (...) {
        return false;
    }
}

static bool looksLikeText(const string &body) {
    if (body.empty()) {
        return true;
    }
    size_t control_count = 0;
    for (unsigned char ch : body) {
        if (ch == '\n' || ch == '\r' || ch == '\t') {
            continue;
        }
        if (ch < 0x20) {
            ++control_count;
        }
    }
    return control_count * 100 <= body.size();
}

static string formatDurationMs(std::chrono::steady_clock::duration duration) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    std::ostringstream ss;
    ss << ms << " ms";
    return ss.str();
}

struct DiagnosticOptions {
    string url;
    string method = "GET";
    string body;
    string output_file;
    string quic_cc;
    vector<pair<string, string> > headers;
    bool show_headers = false;
    bool verbose = false;
    bool verify_peer = true;
    bool suppress_body = false;
    float timeout_sec = 180.0f;
};

struct DiagnosticMetrics {
    std::chrono::steady_clock::time_point request_start;
    std::chrono::steady_clock::time_point quic_peer_resolved;
    std::chrono::steady_clock::time_point quic_udp_bound;
    std::chrono::steady_clock::time_point quic_engine_initialized;
    std::chrono::steady_clock::time_point quic_handshake_completed;
    std::chrono::steady_clock::time_point headers_received;
    std::chrono::steady_clock::time_point first_body_chunk;
    std::chrono::steady_clock::time_point request_end;
    size_t body_chunks = 0;
    size_t body_bytes = 0;
    bool quic_peer_resolved_seen = false;
    bool quic_udp_bound_seen = false;
    bool quic_engine_seen = false;
    bool quic_handshake_seen = false;
    bool headers_seen = false;
    bool first_body_seen = false;
};

class DiagnosticHttpClient : public HttpClientImp {
public:
    typedef function<void(const SockException &, const Parser &, const DiagnosticMetrics &, const string &)> ResultCB;

    void startRequestWithStats(const string &url, const ResultCB &cb, float timeout_sec) {
        _cb = cb;
        _body.clear();
        _metrics = DiagnosticMetrics();
        _metrics.request_start = std::chrono::steady_clock::now();
        setCompleteTimeout(timeout_sec * 1000);
        sendRequest(url);
    }

protected:
    void onQuicBackendLog(int, const string &message) override {
        auto now = std::chrono::steady_clock::now();
        if (message.find("quic client peer resolved: ") == 0) {
            _metrics.quic_peer_resolved_seen = true;
            _metrics.quic_peer_resolved = now;
            return;
        }
        if (message.find("quic client UDP bound: ") == 0) {
            _metrics.quic_udp_bound_seen = true;
            _metrics.quic_udp_bound = now;
            return;
        }
        if (message == "lsquic client engine initialized") {
            _metrics.quic_engine_seen = true;
            _metrics.quic_engine_initialized = now;
            return;
        }
        if (message == "lsquic client handshake completed") {
            _metrics.quic_handshake_seen = true;
            _metrics.quic_handshake_completed = now;
            return;
        }
    }

    void onResponseHeader(const string &, const HttpHeader &) override {
        _body.clear();
        _metrics.headers_seen = true;
        _metrics.headers_received = std::chrono::steady_clock::now();
    }

    void onResponseBody(const char *buf, size_t size) override {
        if (!_metrics.first_body_seen) {
            _metrics.first_body_seen = true;
            _metrics.first_body_chunk = std::chrono::steady_clock::now();
        }
        ++_metrics.body_chunks;
        _metrics.body_bytes += size;
        _body.append(buf, size);
    }

    void onResponseCompleted(const SockException &ex) override {
        _metrics.request_end = std::chrono::steady_clock::now();
        const_cast<Parser &>(response()).setContent(_body);
        if (_cb) {
            auto cb = std::move(_cb);
            cb(ex, response(), _metrics, _body);
        }
    }

private:
    ResultCB _cb;
    DiagnosticMetrics _metrics;
    string _body;
};

static void printUsage(const char *prog) {
    cerr << "Usage:" << endl;
    cerr << "  " << prog << " [options] <https-url>" << endl;
    cerr << "  " << prog << " [url] [post-form-url] [post-json-url] [auto-url]" << endl;
    cerr << endl;
    cerr << "Single URL options:" << endl;
    cerr << "  -o <file>       Write response body to file" << endl;
    cerr << "  -i              Print response headers" << endl;
    cerr << "  -v              Enable trace logs" << endl;
    cerr << "  -k              Disable peer verification" << endl;
    cerr << "  -X <method>     HTTP method, default GET" << endl;
    cerr << "  -d <body>       Request body; defaults method to POST when -X is not set" << endl;
    cerr << "  -H <k:v>        Extra request header, repeatable" << endl;
    cerr << "  --timeout <s>   Total request timeout in seconds, default 180" << endl;
    cerr << "  --quic-cc <v>   QUIC congestion control: default/cubic/bbr/adaptive" << endl;
    cerr << "  --no-body       Do not print response body to stdout" << endl;
    cerr << endl;
    cerr << "When multiple positional URLs are given, the built-in regression suite runs." << endl;
}

static bool parseHeaderArg(const string &arg, pair<string, string> &header) {
    auto pos = arg.find(':');
    if (pos == string::npos) {
        return false;
    }
    header.first = trim(arg.substr(0, pos));
    header.second = trim(arg.substr(pos + 1));
    return !header.first.empty();
}

static bool parseDiagnosticOptions(int argc, char *argv[], DiagnosticOptions &options, vector<string> &positionals) {
    bool explicit_method = false;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-o" || arg == "-X" || arg == "-d" || arg == "-H" || arg == "--timeout") {
            if (i + 1 >= argc) {
                cerr << "missing value for " << arg << endl;
                return false;
            }
            string value = argv[++i];
            if (arg == "-o") {
                options.output_file = value;
            } else if (arg == "-X") {
                options.method = value;
                explicit_method = true;
            } else if (arg == "-d") {
                options.body = value;
            } else if (arg == "--timeout") {
                try {
                    options.timeout_sec = std::stof(value);
                } catch (...) {
                    cerr << "invalid timeout value: " << value << endl;
                    return false;
                }
                if (options.timeout_sec <= 0) {
                    cerr << "timeout must be > 0" << endl;
                    return false;
                }
            } else {
                pair<string, string> header;
                if (!parseHeaderArg(value, header)) {
                    cerr << "invalid header argument: " << value << endl;
                    return false;
                }
                options.headers.emplace_back(std::move(header));
            }
            continue;
        }

        if (arg == "-i") {
            options.show_headers = true;
            continue;
        }
        if (arg == "-v") {
            options.verbose = true;
            continue;
        }
        if (arg == "-k") {
            options.verify_peer = false;
            continue;
        }
        if (arg == "--quic-cc") {
            if (i + 1 >= argc) {
                cerr << "missing value for " << arg << endl;
                return false;
            }
            options.quic_cc = argv[++i];
            continue;
        }
        if (arg == "--no-body") {
            options.suppress_body = true;
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            cerr << "unknown option: " << arg << endl;
            return false;
        }

        positionals.emplace_back(std::move(arg));
    }

    if (!explicit_method && !options.body.empty()) {
        options.method = "POST";
    }
    return true;
}

static int writeBodyToFile(const string &path, const string &body) {
    ofstream output(path.c_str(), ios::binary | ios::out | ios::trunc);
    if (!output.is_open()) {
        cerr << "failed to open output file: " << path << endl;
        return 2;
    }
    output.write(body.data(), static_cast<std::streamsize>(body.size()));
    output.close();
    if (!output) {
        cerr << "failed to write output file: " << path << endl;
        return 3;
    }
    return 0;
}

static int runDiagnosticClient(const DiagnosticOptions &options) {
    semaphore sem;
    int exit_code = 0;
    SockException result_ex;
    Parser result_parser;
    DiagnosticMetrics result_metrics;
    string result_body;

    if (!options.quic_cc.empty()) {
        auto cc = quicCongestionAlgoFromString(options.quic_cc);
        if (cc == QuicCongestionAlgo::Default && options.quic_cc != "default" && options.quic_cc != "0") {
            cerr << "invalid --quic-cc value: " << options.quic_cc << endl;
            return 1;
        }
        mINI::Instance()[Http::kQuicClientCongestionControl] = quicCongestionAlgoName(cc);
    }

    auto requester = std::make_shared<DiagnosticHttpClient>();
    requester->setEnableHttp3(true);
    requester->setEnableHttp3Auto(false);
    requester->setHttp3VerifyPeer(options.verify_peer);
    requester->setRequestKeepAlive(false);
    requester->setMethod(options.method);
    for (auto &header : options.headers) {
        requester->addHeader(header.first, header.second, true);
    }
    if (!options.body.empty()) {
        requester->setBody(options.body);
    }

    requester->startRequestWithStats(options.url, [&](const SockException &ex, const Parser &parser, const DiagnosticMetrics &metrics, const string &body) {
        result_ex = ex;
        result_parser = parser;
        result_metrics = metrics;
        result_body = body;
        sem.post();
    }, options.timeout_sec);

    sem.wait();

    auto total_time = result_metrics.request_end - result_metrics.request_start;
    string peer_resolved_time = result_metrics.quic_peer_resolved_seen ? formatDurationMs(result_metrics.quic_peer_resolved - result_metrics.request_start) : string("n/a");
    string udp_bound_time = result_metrics.quic_udp_bound_seen ? formatDurationMs(result_metrics.quic_udp_bound - result_metrics.request_start) : string("n/a");
    string engine_ready_time = result_metrics.quic_engine_seen ? formatDurationMs(result_metrics.quic_engine_initialized - result_metrics.request_start) : string("n/a");
    string handshake_time = result_metrics.quic_handshake_seen ? formatDurationMs(result_metrics.quic_handshake_completed - result_metrics.request_start) : string("n/a");
    string header_time = result_metrics.headers_seen ? formatDurationMs(result_metrics.headers_received - result_metrics.request_start) : string("n/a");
    string first_body_time = result_metrics.first_body_seen ? formatDurationMs(result_metrics.first_body_chunk - result_metrics.request_start) : string("n/a");
    string headers_after_handshake = (result_metrics.quic_handshake_seen && result_metrics.headers_seen)
                                       ? formatDurationMs(result_metrics.headers_received - result_metrics.quic_handshake_completed)
                                       : string("n/a");
    string total_time_str = formatDurationMs(total_time);

    uint64_t content_length = 0;
    bool has_content_length = parseUint64(result_parser["Content-Length"], content_length);
    bool body_expected = strcasecmp(options.method.c_str(), "HEAD") != 0;
    bool content_length_match = !body_expected || !has_content_length || content_length == result_body.size();
    bool status_success = !result_parser.status().empty() && result_parser.status()[0] == '2';

    cout << "HTTP version: " << responseVersion(result_parser) << endl;
    cout << "Status: " << result_parser.status() << endl;
    cout << "URL: " << options.url << endl;
    cout << "Method: " << options.method << endl;
    auto quic_cc = mINI::Instance()[Http::kQuicClientCongestionControl].as<string>();
    if (quic_cc.empty()) {
        quic_cc = mINI::Instance()[Http::kQuicCongestionControl].as<string>();
    }
    cout << "QUIC cc: " << quic_cc << endl;
    cout << "QUIC peer resolved: " << peer_resolved_time << endl;
    cout << "QUIC UDP bound: " << udp_bound_time << endl;
    cout << "QUIC engine ready: " << engine_ready_time << endl;
    cout << "QUIC handshake completed: " << handshake_time << endl;
    cout << "Total time: " << total_time_str << endl;
    cout << "Headers received: " << header_time << endl;
    cout << "Headers after handshake: " << headers_after_handshake << endl;
    cout << "First body chunk: " << first_body_time << endl;
    cout << "Body chunks: " << result_metrics.body_chunks << endl;
    cout << "Body bytes: " << result_body.size() << endl;
    if (has_content_length) {
        cout << "Content-Length: " << content_length << endl;
        cout << "Body complete: " << (content_length_match ? "yes" : (body_expected ? "no" : "n/a")) << endl;
    } else {
        cout << "Content-Length: <absent>" << endl;
        cout << "Body complete: unknown" << endl;
    }
    if (!result_parser["Content-Type"].empty()) {
        cout << "Content-Type: " << result_parser["Content-Type"] << endl;
    }
    if (!result_parser["Alt-Svc"].empty()) {
        cout << "Alt-Svc: " << result_parser["Alt-Svc"] << endl;
    }

    if (options.show_headers) {
        cout << endl << "Response headers:" << endl;
        for (auto &pr : result_parser.getHeader()) {
            cout << pr.first << ": " << pr.second << endl;
        }
    }

    if (!options.output_file.empty()) {
        auto write_rc = writeBodyToFile(options.output_file, result_body);
        if (write_rc != 0) {
            exit_code = write_rc;
        } else {
            cout << "Body written to: " << options.output_file << endl;
        }
    }

    if (!options.suppress_body && options.output_file.empty()) {
        cout << endl << "Response body:" << endl;
        if (looksLikeText(result_body)) {
            cout.write(result_body.data(), static_cast<std::streamsize>(result_body.size()));
            if (result_body.empty() || result_body.back() != '\n') {
                cout << endl;
            }
        } else {
            cout << "<binary body omitted; use -o to save it>" << endl;
        }
    }

    if (result_ex) {
        cerr << "request failed: code=" << result_ex.getErrCode() << " msg=" << result_ex.what() << endl;
        exit_code = 4;
    } else if (responseVersion(result_parser) != "HTTP/3") {
        cerr << "unexpected protocol version: " << responseVersion(result_parser) << endl;
        exit_code = 5;
    } else if (!status_success) {
        cerr << "unexpected HTTP status: " << result_parser.status() << endl;
        exit_code = 6;
    } else if (!content_length_match) {
        cerr << "response body does not match Content-Length" << endl;
        exit_code = 7;
    }

    cout.flush();
    cerr.flush();
    fflush(nullptr);
    _Exit(exit_code);
    return exit_code;
}

static int runRegressionSuite(int argc, char *argv[]) {
    auto url = argc > 1 ? string(argv[1]) : string("https://127.0.0.1:18443/index/api/version");
    auto post_form_url = argc > 2 ? string(argv[2]) : string("https://127.0.0.1:18443/index/api/getServerConfig");
    auto post_json_url = argc > 3 ? string(argv[3]) : string("https://127.0.0.1:18443/index/api/version");
    auto auto_url = argc > 4 ? string(argv[4]) : string("https://127.0.0.1:18444/index/api/version");
    auto auto_post_url = Parser::mergeUrl(auto_url, "/index/api/getServerConfig");
    auto auto_origin = getOriginFromUrl(auto_url);

    semaphore sem;
    int exit_code = 0;

    auto run_request = [&](const shared_ptr<HttpRequester> &requester,
                           const string &request_url,
                           const function<void(HttpRequester &)> &configure_request,
                           int err_code,
                           int bad_response_code,
                           const function<bool(const Parser &)> &validator,
                           const string &label,
                           bool expect_transport_alive_after_response) {
        requester->setEnableHttp3(false);
        requester->setEnableHttp3Auto(true);
        requester->setHttp3VerifyPeer(false);
        configure_request(*requester);
        requester->startRequester(request_url, [&](const SockException &ex, const Parser &parser) {
            if (ex) {
                cerr << label << " failed: code=" << ex.getErrCode()
                     << " msg=" << ex.what() << endl;
                exit_code = err_code;
            } else if (!validator(parser)) {
                cerr << label << " unexpected response: status=" << parser.status()
                     << " version=" << responseVersion(parser)
                     << " alt-svc=" << parser["Alt-Svc"]
                     << " body=" << parser.content() << endl;
                exit_code = bad_response_code;
            } else {
                cout << label << " ok, url=" << request_url
                     << ", bytes=" << parser.content().size() << endl;
            }
            sem.post();
        }, 10);

        sem.wait();
        if (exit_code == 0 && expect_transport_alive_after_response && !requester->alive()) {
            cerr << label << " transport was not kept alive for reuse" << endl;
            exit_code = bad_response_code;
        }
        requester->clear();
        return exit_code == 0;
    };

    auto requester = std::make_shared<HttpRequester>();
    requester->setRequestKeepAlive(true);
    if (!run_request(requester,
                     url,
                     [](HttpRequester &requester) {
                         requester.setEnableHttp3(true);
                         requester.setMethod("GET");
                     },
                     2,
                     3,
                     [](const Parser &parser) {
                         return parser.status() == "200" && parser.content().find('{') != string::npos;
                     },
                     "HTTP/3 requester GET",
                     true)) {
        return exit_code;
    }

    if (!run_request(requester,
                     post_form_url,
                     [](HttpRequester &requester) {
                         requester.setEnableHttp3(true);
                         requester.setMethod("POST");
                         requester.setBody(std::make_shared<ChunkedStringBody>("secret=quic-smoke-secret", 4));
                     },
                     4,
                     5,
                     looksLikeOkJsonResponse,
                     "HTTP/3 requester POST form",
                     true)) {
        return exit_code;
    }

    if (!run_request(requester,
                     post_json_url,
                     [](HttpRequester &requester) {
                         requester.setEnableHttp3(true);
                         requester.setMethod("POST");
                         requester.setBody(std::make_shared<ChunkedStringBody>("{\"secret\":\"quic-smoke-secret\"}", 5));
                         requester.addHeader("Content-Type", "application/json", true);
                     },
                     6,
                     7,
                     [](const Parser &parser) {
                         return parser.status() == "200" && parser.content().find("\"buildTime\"") != string::npos;
                     },
                     "HTTP/3 requester POST json",
                     false)) {
        return exit_code;
    }

    auto auto_requester = std::make_shared<HttpRequester>();
    auto auto_validator = [](const Parser &parser) {
        return parser.status() == "200" && parser.content().find('{') != string::npos;
    };
    eraseAltSvc(auto_origin.first, auto_origin.second);
    if (!run_request(auto_requester,
                     auto_url,
                     [](HttpRequester &requester) {
                         requester.setMethod("GET");
                     },
                     8,
                     9,
                     [auto_validator](const Parser &parser) {
                         return auto_validator(parser) &&
                                responseVersion(parser) == "HTTP/1.1" &&
                                parser["Alt-Svc"].find("h3=") != string::npos;
                     },
                     "HTTP/3 auto bootstrap GET",
                     false)) {
        return exit_code;
    }

    if (!run_request(auto_requester,
                     auto_url,
                     [](HttpRequester &requester) {
                         requester.setMethod("GET");
                     },
                     10,
                     11,
                     [auto_validator](const Parser &parser) {
                         return auto_validator(parser) && responseVersion(parser) == "HTTP/3";
                     },
                     "HTTP/3 auto Alt-Svc GET",
                     false)) {
        return exit_code;
    }

    auto auto_post_requester = std::make_shared<HttpRequester>();
    eraseAltSvc(auto_origin.first, auto_origin.second);
    if (!run_request(auto_post_requester,
                     auto_url,
                     [](HttpRequester &requester) {
                         requester.setMethod("GET");
                     },
                     12,
                     13,
                     [auto_validator](const Parser &parser) {
                         return auto_validator(parser) &&
                                responseVersion(parser) == "HTTP/1.1" &&
                                parser["Alt-Svc"].find("h3=") != string::npos;
                     },
                     "HTTP/3 auto POST bootstrap GET",
                     false)) {
        return exit_code;
    }

    if (!run_request(auto_post_requester,
                     auto_post_url,
                     [](HttpRequester &requester) {
                         requester.setMethod("POST");
                         requester.setBody(std::make_shared<ChunkedStringBody>("secret=quic-smoke-secret", 3));
                     },
                     14,
                     15,
                     [](const Parser &parser) {
                         return looksLikeOkJsonResponse(parser) && responseVersion(parser) == "HTTP/3";
                     },
                     "HTTP/3 auto Alt-Svc POST form",
                     false)) {
        return exit_code;
    }

    if (!run_request(auto_requester,
                     auto_url,
                     [](HttpRequester &requester) {
                         requester.setMethod("GET");
                         requester.setEnableHttp3Auto(false);
                     },
                     16,
                     17,
                     [auto_validator](const Parser &parser) {
                         return auto_validator(parser) && responseVersion(parser) == "HTTP/1.1";
                     },
                     "HTTP/3 back to direct HTTPS GET",
                     false)) {
        return exit_code;
    }

    return exit_code;
}

} // namespace

int main(int argc, char *argv[]) {
    Logger::Instance().add(make_shared<ConsoleChannel>());
    signal(SIGPIPE, SIG_IGN);

#if !defined(ENABLE_QUIC)
    cerr << "ENABLE_QUIC is off; skipping HTTP/3 HttpClient test" << endl;
    return 0;
#else
    DiagnosticOptions options;
    vector<string> positionals;
    if (!parseDiagnosticOptions(argc, argv, options, positionals)) {
        printUsage(argv[0]);
        return 1;
    }

    auto generic_mode = positionals.size() == 1;
    auto regression_mode = positionals.empty() || (positionals.size() > 1 && argc == static_cast<int>(positionals.size() + 1));
    if (!generic_mode && !regression_mode) {
        printUsage(argv[0]);
        return 1;
    }

    if (generic_mode) {
        Logger::Instance().setWriter(nullptr);
    } else {
        Logger::Instance().setWriter(make_shared<AsyncLogWriter>());
    }

    Logger::Instance().setLevel(generic_mode ? (options.verbose ? LTrace : LWarn) : LTrace);

    if (generic_mode) {
        options.url = positionals.front();
        return runDiagnosticClient(options);
    }

    return runRegressionSuite(argc, argv);
#endif
}
