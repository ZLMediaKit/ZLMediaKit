/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Common/Device.h"
#include "Common/config.h"
#include "Http/HttpClientImp.h"
#include "Http/HttpConst.h"
#include "Http/WebSocketSession.h"
#include "Network/TcpServer.h"
#include "Network/UdpServer.h"
#include "Rtmp/RtmpSession.h"
#include "Thread/semaphore.h"
#include "Util/File.h"
#include "Util/NoticeCenter.h"
#include "Util/SSLBox.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "quic/QuicSession.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace {

static const uint8_t kMuteAdts[] = {
    0xff, 0xf1, 0x6c, 0x40, 0x2d, 0x3f, 0xfc, 0x00, 0xe0, 0x34, 0x20, 0xad, 0xf2, 0x3f, 0xb5, 0xdd,
    0x73, 0xac, 0xbd, 0xca, 0xd7, 0x7d, 0x4a, 0x13, 0x2d, 0x2e, 0xa2, 0x62, 0x02, 0x70, 0x3c, 0x1c,
    0xc5, 0x63, 0x55, 0x69, 0x94, 0xb5, 0x8d, 0x70, 0xd7, 0x24, 0x6a, 0x9e, 0x2e, 0x86, 0x24, 0xea,
    0x4f, 0xd4, 0xf8, 0x10, 0x53, 0xa5, 0x4a, 0xb2, 0x9a, 0xf0, 0xa1, 0x4f, 0x2f, 0x66, 0xf9, 0xd3,
    0x8c, 0xa6, 0x97, 0xd5, 0x84, 0xac, 0x09, 0x25, 0x98, 0x0b, 0x1d, 0x77, 0x04, 0xb8, 0x55, 0x49,
    0x85, 0x27, 0x06, 0x23, 0x58, 0xcb, 0x22, 0xc3, 0x20, 0x3a, 0x12, 0x09, 0x48, 0x24, 0x86, 0x76,
    0x95, 0xe3, 0x45, 0x61, 0x43, 0x06, 0x6b, 0x4a, 0x61, 0x14, 0x24, 0xa9, 0x16, 0xe0, 0x97, 0x34,
    0xb6, 0x58, 0xa4, 0x38, 0x34, 0x90, 0x19, 0x5d, 0x00, 0x19, 0x4a, 0xc2, 0x80, 0x4b, 0xdc, 0xb7,
    0x00, 0x18, 0x12, 0x3d, 0xd9, 0x93, 0xee, 0x74, 0x13, 0x95, 0xad, 0x0b, 0x59, 0x51, 0x0e, 0x99,
    0xdf, 0x49, 0x98, 0xde, 0xa9, 0x48, 0x4b, 0xa5, 0xfb, 0xe8, 0x79, 0xc9, 0xe2, 0xd9, 0x60, 0xa5,
    0xbe, 0x74, 0xa6, 0x6b, 0x72, 0x0e, 0xe3, 0x7b, 0x28, 0xb3, 0x0e, 0x52, 0xcc, 0xf6, 0x3d, 0x39,
    0xb7, 0x7e, 0xbb, 0xf0, 0xc8, 0xce, 0x5c, 0x72, 0xb2, 0x89, 0x60, 0x33, 0x7b, 0xc5, 0xda, 0x49,
    0x1a, 0xda, 0x33, 0xba, 0x97, 0x9e, 0xa8, 0x1b, 0x6d, 0x5a, 0x77, 0xb6, 0xf1, 0x69, 0x5a, 0xd1,
    0xbd, 0x84, 0xd5, 0x4e, 0x58, 0xa8, 0x5e, 0x8a, 0xa0, 0xc2, 0xc9, 0x22, 0xd9, 0xa5, 0x53, 0x11,
    0x18, 0xc8, 0x3a, 0x39, 0xcf, 0x3f, 0x57, 0xb6, 0x45, 0x19, 0x1e, 0x8a, 0x71, 0xa4, 0x46, 0x27,
    0x9e, 0xe9, 0xa4, 0x86, 0xdd, 0x14, 0xd9, 0x4d, 0xe3, 0x71, 0xe3, 0x26, 0xda, 0xaa, 0x17, 0xb4,
    0xac, 0xe1, 0x09, 0xc1, 0x0d, 0x75, 0xba, 0x53, 0x0a, 0x37, 0x8b, 0xac, 0x37, 0x39, 0x41, 0x27,
    0x6a, 0xf0, 0xe9, 0xb4, 0xc2, 0xac, 0xb0, 0x39, 0x73, 0x17, 0x64, 0x95, 0xf4, 0xdc, 0x33, 0xbb,
    0x84, 0x94, 0x3e, 0xf8, 0x65, 0x71, 0x60, 0x7b, 0xd4, 0x5f, 0x27, 0x79, 0x95, 0x6a, 0xba, 0x76,
    0xa6, 0xa5, 0x9a, 0xec, 0xae, 0x55, 0x3a, 0x27, 0x48, 0x23, 0xcf, 0x5c, 0x4d, 0xbc, 0x0b, 0x35,
    0x5c, 0xa7, 0x17, 0xcf, 0x34, 0x57, 0xc9, 0x58, 0xc5, 0x20, 0x09, 0xee, 0xa5, 0xf2, 0x9c, 0x6c,
    0x39, 0x1a, 0x77, 0x92, 0x9b, 0xff, 0xc6, 0xae, 0xf8, 0x36, 0xba, 0xa8, 0xaa, 0x6b, 0x1e, 0x8c,
    0xc5, 0x97, 0x39, 0x6a, 0xb8, 0xa2, 0x55, 0xa8, 0xf8
};

static const uint64_t kMuteAdtsMs = 128;
static const char *kStaticFileName = "http3_static.txt";
static const char *kStaticFileBody = "ZLMediaKit HTTP/3 server regression static body.\n";
static const uint16_t kHttpPort = 19080;
static const uint16_t kHttpsPort = 19444;
static const uint16_t kQuicPort = 19443;
static const size_t kLiveBodyLimit = 8192;

struct RequestOptions {
    string url;
    string method = "GET";
    string body;
    StrCaseMap headers;
    bool enable_http3 = false;
    bool verify_peer = false;
    bool keep_alive = true;
    float timeout_sec = 10.0f;
    size_t body_limit = 0;
};

static RequestOptions makeRequest(const string &url, bool enable_http3 = false) {
    RequestOptions options;
    options.url = url;
    options.enable_http3 = enable_http3;
    return options;
}

static const string &responseVersion(const Parser &parser) {
    return parser.method();
}

struct RequestResult {
    SockException ex;
    Parser parser;
    string body;
    bool aborted_by_limit = false;
};

class BlockingHttpClient : public HttpClientImp {
public:
    using Ptr = shared_ptr<BlockingHttpClient>;

    RequestResult request(const RequestOptions &options) {
        clear();
        _result = RequestResult();
        _options = options;
        setMethod(options.method);
        setEnableHttp3(options.enable_http3);
        setEnableHttp3Auto(false);
        setHttp3VerifyPeer(options.verify_peer);
        setCompleteTimeout(options.timeout_sec * 1000);
        setRequestKeepAlive(options.keep_alive);
        for (auto &pr : options.headers) {
            addHeader(pr.first, pr.second, true);
        }
        if (!options.body.empty()) {
            setBody(options.body);
        }
        sendRequest(options.url);
        _sem.wait();
        return _result;
    }

protected:
    void onResponseHeader(const string &, const HttpHeader &) override {}

    void onResponseBody(const char *buf, size_t size) override {
        _result.body.append(buf, size);
        if (_options.body_limit && !_result.aborted_by_limit && _result.body.size() >= _options.body_limit) {
            _result.aborted_by_limit = true;
            weak_ptr<BlockingHttpClient> weak_self = static_pointer_cast<BlockingHttpClient>(shared_from_this());
            getPoller()->async([weak_self]() {
                auto self = weak_self.lock();
                if (!self) {
                    return;
                }
                self->shutdown(SockException(Err_shutdown, "body limit reached"));
            }, false);
        }
    }

    void onResponseCompleted(const SockException &ex) override {
        _result.ex = ex;
        _result.parser = response();
        _sem.post();
    }

private:
    semaphore _sem;
    RequestOptions _options;
    RequestResult _result;
};

static string ensureSlash(string path) {
    if (path.empty() || path[0] != '/') {
        path.insert(path.begin(), '/');
    }
    return path;
}

static void expectTrue(bool expr, const string &message) {
    if (!expr) {
        throw runtime_error(message);
    }
}

static void expectStatus(const RequestResult &result, const string &status, const string &context) {
    if (result.parser.status() != status) {
        throw runtime_error(context + ": unexpected status " + result.parser.status());
    }
}

static void expectBodyContains(const RequestResult &result, const string &needle, const string &context) {
    if (result.body.find(needle) == string::npos) {
        throw runtime_error(context + ": response body missing expected text");
    }
}

static bool requestSucceeded(const RequestResult &result) {
    if (!result.ex) {
        return true;
    }
    return result.aborted_by_limit && result.ex.getErrCode() == Err_shutdown;
}

static void expectSucceeded(const RequestResult &result, const string &context) {
    if (!requestSucceeded(result)) {
        std::ostringstream ss;
        ss << context << ": request failed: " << result.ex.what()
           << ", status=" << result.parser.status()
           << ", version=" << responseVersion(result.parser)
           << ", body_bytes=" << result.body.size();
        throw runtime_error(ss.str());
    }
}

class ServerFixture {
public:
    ServerFixture() {
        _root = "/tmp/zlm-http3-server-regression-root";
        File::delete_file(_root, true);
        File::create_path(_root + "/placeholder", 0777);
        expectTrue(File::saveFile(kStaticFileBody, _root + "/" + kStaticFileName), "failed to create static test file");

        mINI::Instance()[General::kEnableVhost] = 0;
        mINI::Instance()[Http::kRootPath] = _root;
        mINI::Instance()[Http::kAllowCrossDomains] = 1;
        mINI::Instance()[Http::kQuicPort] = kQuicPort;
        mINI::Instance()[Protocol::kEnableAudio] = 1;
        mINI::Instance()[Protocol::kEnableRtmp] = 1;
        mINI::Instance()[Protocol::kEnableTS] = 1;
        mINI::Instance()[Protocol::kEnableFMP4] = 1;
        mINI::Instance()[Protocol::kRtmpDemand] = 0;
        mINI::Instance()[Protocol::kTSDemand] = 0;
        mINI::Instance()[Protocol::kFMP4Demand] = 0;

        SSL_Initor::Instance().loadCertificate((exeDir() + "default.pem").data());
        SSL_Initor::Instance().trustCertificate((exeDir() + "default.pem").data());
        SSL_Initor::Instance().ignoreInvalidCertificate(false);

        NoticeCenter::Instance().addListener(&_http_tag, Broadcast::kBroadcastHttpRequest, [](BroadcastHttpRequestArgs) {
            if (parser.url() == "/test/api/hello") {
                consumed = true;
                auto name = string("world");
                auto it = parser.getUrlArgs().find("name");
                if (it != parser.getUrlArgs().end()) {
                    name = it->second;
                }
                StrCaseMap headers;
                headers.emplace("content-type", "text/plain; charset=utf-8");
                invoker(200, headers, make_shared<HttpStringBody>("hello:" + name));
                return;
            }
            if (parser.url() == "/test/api/echo") {
                consumed = true;
                StrCaseMap headers;
                headers.emplace("content-type", "text/plain; charset=utf-8");
                invoker(200, headers, make_shared<HttpStringBody>(parser.content()));
                return;
            }
            if (parser.url() == "/test/api/forbidden") {
                consumed = true;
                StrCaseMap headers;
                headers.emplace("content-type", "text/plain; charset=utf-8");
                invoker(403, headers, make_shared<HttpStringBody>("forbidden"));
            }
        });

        NoticeCenter::Instance().addListener(&_play_tag, Broadcast::kBroadcastMediaPlayed, [](BroadcastMediaPlayedArgs) {
            if (args.params.find("deny=1") != string::npos) {
                invoker("denied");
                return;
            }
            invoker("");
        });

        _http_server = make_shared<TcpServer>();
        _https_server = make_shared<TcpServer>();
        _quic_server = make_shared<UdpServer>();
        _quic_server->setOnCreateSocket([](const EventPoller::Ptr &poller, const Buffer::Ptr &buf, struct sockaddr *, int) {
            if (!buf) {
                return Socket::createSocket(poller, false);
            }
            auto new_poller = QuicSession::queryPoller(buf);
            return Socket::createSocket(new_poller ? new_poller : poller, false);
        });

        _http_server->start<HttpSession>(kHttpPort, "127.0.0.1");
        _https_server->start<HttpsSession>(kHttpsPort, "127.0.0.1");
        _quic_server->start<QuicSession>(kQuicPort, "127.0.0.1");

        startLiveSource();
        try {
            waitForLiveSource();
        } catch (...) {
            _live_running = false;
            if (_live_thread.joinable()) {
                _live_thread.join();
            }
            throw;
        }
    }

    ~ServerFixture() {
        _live_running = false;
        if (_live_thread.joinable()) {
            _live_thread.join();
        }
        _live_channel.reset();
        _quic_server.reset();
        _https_server.reset();
        _http_server.reset();
        NoticeCenter::Instance().delListener(&_play_tag, Broadcast::kBroadcastMediaPlayed);
        NoticeCenter::Instance().delListener(&_http_tag, Broadcast::kBroadcastHttpRequest);
        File::delete_file(_root, true);
    }

    string httpUrl(const string &path) const {
        return "http://127.0.0.1:" + to_string(kHttpPort) + ensureSlash(path);
    }

    string httpsUrl(const string &path) const {
        return "https://127.0.0.1:" + to_string(kHttpsPort) + ensureSlash(path);
    }

    string http3Url(const string &path) const {
        return "https://127.0.0.1:" + to_string(kQuicPort) + ensureSlash(path);
    }

private:
    void startLiveSource() {
        ProtocolOption option;
        option.enable_audio = true;
        option.enable_hls = false;
        option.enable_hls_fmp4 = false;
        option.enable_mp4 = false;
        option.enable_rtsp = false;
        option.enable_rtmp = true;
        option.enable_ts = true;
        option.enable_fmp4 = true;
        option.enable_rtmp = true;
        option.rtmp_demand = false;
        option.ts_demand = false;
        option.fmp4_demand = false;
        option.add_mute_audio = false;

        _live_channel = make_shared<DevChannel>(MediaTuple{DEFAULT_VHOST, "live", "test", ""}, 0.0f, option);

        AudioInfo info;
        info.codecId = CodecAAC;
        info.iChannel = 2;
        info.iSampleBit = 16;
        info.iSampleRate = 44100;
        expectTrue(_live_channel->initAudio(info), "failed to initialize AAC live track");
        _live_channel->addTrackCompleted();

        _live_running = true;
        _live_thread = thread([this]() {
            uint64_t dts = 0;
            while (_live_running) {
                _live_channel->inputAAC(reinterpret_cast<const char *>(kMuteAdts) + 7,
                                        static_cast<int>(sizeof(kMuteAdts) - 7),
                                        dts,
                                        reinterpret_cast<const char *>(kMuteAdts));
                dts += kMuteAdtsMs;
                this_thread::sleep_for(chrono::milliseconds(20));
            }
        });
    }

    void waitForLiveSource() {
        auto deadline = chrono::steady_clock::now() + chrono::seconds(5);
        while (chrono::steady_clock::now() < deadline) {
            auto rtmp = MediaSource::find(RTMP_SCHEMA, DEFAULT_VHOST, "live", "test", false);
            auto ts = MediaSource::find(TS_SCHEMA, DEFAULT_VHOST, "live", "test", false);
            auto fmp4 = MediaSource::find(FMP4_SCHEMA, DEFAULT_VHOST, "live", "test", false);
            if (rtmp && ts && fmp4) {
                return;
            }
            this_thread::sleep_for(chrono::milliseconds(50));
        }
        throw runtime_error("timed out waiting for live media sources to register");
    }

private:
    string _root;
    TcpServer::Ptr _http_server;
    TcpServer::Ptr _https_server;
    UdpServer::Ptr _quic_server;
    DevChannel::Ptr _live_channel;
    thread _live_thread;
    atomic<bool> _live_running{false};
    int _http_tag = 0;
    int _play_tag = 0;
};

static void runServerRegression() {
    ServerFixture fixture;
    auto http_client = make_shared<BlockingHttpClient>();
    auto https_client = make_shared<BlockingHttpClient>();
    auto http3_client = make_shared<BlockingHttpClient>();

    auto http_static = http_client->request(makeRequest(fixture.httpUrl(kStaticFileName)));
    expectSucceeded(http_static, "http static GET");
    expectStatus(http_static, "200", "http static GET");
    expectTrue(http_static.body == kStaticFileBody, "http static GET: unexpected body");

    auto https_static = https_client->request(makeRequest(fixture.httpsUrl(kStaticFileName)));
    expectSucceeded(https_static, "https static GET");
    expectStatus(https_static, "200", "https static GET");
    expectTrue(https_static.body == kStaticFileBody, "https static GET: unexpected body");
    expectTrue(https_static.parser["Alt-Svc"].find("h3=\":19443\"") != string::npos, "https static GET: missing Alt-Svc");

    auto http3_static = http3_client->request(makeRequest(fixture.http3Url(kStaticFileName), true));
    expectSucceeded(http3_static, "http3 static GET");
    expectStatus(http3_static, "200", "http3 static GET");
    expectTrue(http3_static.body == kStaticFileBody, "http3 static GET: unexpected body");
    expectTrue(responseVersion(http3_static.parser) == "HTTP/3", "http3 static GET: protocol mismatch");

    RequestOptions head_options;
    head_options.url = fixture.http3Url(kStaticFileName);
    head_options.method = "HEAD";
    head_options.enable_http3 = true;
    auto http3_head = http3_client->request(head_options);
    expectSucceeded(http3_head, "http3 HEAD");
    expectStatus(http3_head, "200", "http3 HEAD");
    expectTrue(http3_head.body.empty(), "http3 HEAD: body must be empty");

    RequestOptions options_request;
    options_request.url = fixture.http3Url("/test/api/hello");
    options_request.method = "OPTIONS";
    options_request.enable_http3 = true;
    auto http3_options = http3_client->request(options_request);
    expectSucceeded(http3_options, "http3 OPTIONS");
    expectStatus(http3_options, "200", "http3 OPTIONS");
    expectTrue(http3_options.parser["Allow"].find("OPTIONS") != string::npos, "http3 OPTIONS: missing Allow");

    auto https_hello = https_client->request(makeRequest(fixture.httpsUrl("/test/api/hello?name=quic")));
    expectSucceeded(https_hello, "https api GET");
    expectStatus(https_hello, "200", "https api GET");
    expectTrue(https_hello.body == "hello:quic", "https api GET: body mismatch");

    auto http3_hello = http3_client->request(makeRequest(fixture.http3Url("/test/api/hello?name=quic"), true));
    expectSucceeded(http3_hello, "http3 api GET");
    expectStatus(http3_hello, "200", "http3 api GET");
    expectTrue(http3_hello.body == "hello:quic", "http3 api GET: body mismatch");

    RequestOptions echo_options;
    echo_options.url = fixture.http3Url("/test/api/echo");
    echo_options.method = "POST";
    echo_options.body = "message=http3-post-body";
    echo_options.enable_http3 = true;
    echo_options.headers.emplace("content-type", "application/x-www-form-urlencoded");
    auto http3_echo = http3_client->request(echo_options);
    expectSucceeded(http3_echo, "http3 api POST");
    expectStatus(http3_echo, "200", "http3 api POST");
    expectTrue(http3_echo.body == echo_options.body, "http3 api POST: body mismatch");

    auto http3_forbidden = http3_client->request(makeRequest(fixture.http3Url("/test/api/forbidden"), true));
    expectSucceeded(http3_forbidden, "http3 forbidden api");
    expectStatus(http3_forbidden, "403", "http3 forbidden api");
    expectBodyContains(http3_forbidden, "forbidden", "http3 forbidden api");

    auto http3_missing = http3_client->request(makeRequest(fixture.http3Url("/missing-not-found.txt"), true));
    expectSucceeded(http3_missing, "http3 missing static");
    expectStatus(http3_missing, "404", "http3 missing static");

    RequestOptions flv_h1;
    flv_h1.url = fixture.httpsUrl("/live/test.live.flv");
    flv_h1.body_limit = kLiveBodyLimit;
    auto https_flv = https_client->request(flv_h1);
    expectSucceeded(https_flv, "https live flv");
    expectStatus(https_flv, "200", "https live flv");
    expectTrue(https_flv.parser["Content-Type"].find("video/x-flv") != string::npos, "https live flv: content-type mismatch");
    expectTrue(https_flv.body.size() >= kLiveBodyLimit, "https live flv: not enough body bytes");

    RequestOptions flv_h3;
    flv_h3.url = fixture.http3Url("/live/test.live.flv");
    flv_h3.enable_http3 = true;
    flv_h3.body_limit = kLiveBodyLimit;
    auto http3_flv = http3_client->request(flv_h3);
    expectSucceeded(http3_flv, "http3 live flv");
    expectStatus(http3_flv, "200", "http3 live flv");
    expectTrue(http3_flv.parser["Content-Type"].find("video/x-flv") != string::npos, "http3 live flv: content-type mismatch");
    expectTrue(http3_flv.body.size() >= kLiveBodyLimit, "http3 live flv: not enough body bytes");

    RequestOptions ts_h3;
    ts_h3.url = fixture.http3Url("/live/test.live.ts");
    ts_h3.enable_http3 = true;
    ts_h3.body_limit = kLiveBodyLimit;
    auto http3_ts = http3_client->request(ts_h3);
    expectSucceeded(http3_ts, "http3 live ts");
    expectStatus(http3_ts, "200", "http3 live ts");
    expectTrue(http3_ts.parser["Content-Type"].find("video/mp2t") != string::npos, "http3 live ts: content-type mismatch");
    expectTrue(http3_ts.body.size() >= kLiveBodyLimit, "http3 live ts: not enough body bytes");

    RequestOptions mp4_h3;
    mp4_h3.url = fixture.http3Url("/live/test.live.mp4");
    mp4_h3.enable_http3 = true;
    mp4_h3.body_limit = kLiveBodyLimit;
    auto http3_mp4 = http3_client->request(mp4_h3);
    expectSucceeded(http3_mp4, "http3 live fmp4");
    expectStatus(http3_mp4, "200", "http3 live fmp4");
    expectTrue(http3_mp4.parser["Content-Type"].find("video/mp4") != string::npos, "http3 live fmp4: content-type mismatch");
    expectTrue(http3_mp4.body.size() >= kLiveBodyLimit, "http3 live fmp4: not enough body bytes");

    auto http3_live_auth_fail = http3_client->request(makeRequest(fixture.http3Url("/live/test.live.flv?deny=1"), true));
    expectSucceeded(http3_live_auth_fail, "http3 live auth fail");
    expectStatus(http3_live_auth_fail, "401", "http3 live auth fail");
    expectBodyContains(http3_live_auth_fail, "denied", "http3 live auth fail");

    auto http3_after_abort = http3_client->request(makeRequest(fixture.http3Url(kStaticFileName), true));
    expectSucceeded(http3_after_abort, "http3 static GET after live abort");
    expectStatus(http3_after_abort, "200", "http3 static GET after live abort");
    expectTrue(http3_after_abort.body == kStaticFileBody, "http3 static GET after live abort: body mismatch");
}

} // namespace

int main() {
    signal(SIGPIPE, SIG_IGN);
    Logger::Instance().add(make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(make_shared<AsyncLogWriter>());
    Logger::Instance().setLevel(LInfo);

#if !defined(ENABLE_QUIC)
    cerr << "ENABLE_QUIC is off; skipping HTTP/3 server regression" << endl;
    return 0;
#else
    try {
        runServerRegression();
        cout << "HTTP/3 server regression passed" << endl;
        return 0;
    } catch (const exception &ex) {
        cerr << "HTTP/3 server regression failed: " << ex.what() << endl;
        return 1;
    }
#endif
}
