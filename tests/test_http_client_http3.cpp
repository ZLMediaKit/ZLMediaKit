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
#include <memory>

#include "Http/HttpBody.h"
#include "Http/HttpProtocolHint.h"
#include "Http/HttpRequester.h"
#include "Thread/semaphore.h"
#include "Util/logger.h"

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

} // namespace

int main(int argc, char *argv[]) {
    Logger::Instance().add(make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(make_shared<AsyncLogWriter>());
    signal(SIGPIPE, SIG_IGN);

#if !defined(ENABLE_QUIC)
    cerr << "ENABLE_QUIC is off; skipping HTTP/3 HttpClient test" << endl;
    return 0;
#else
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
#endif
}
