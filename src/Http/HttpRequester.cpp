/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpRequester.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

void HttpRequester::onResponseHeader(const string &status, const HttpHeader &headers) {
    _res_body.clear();
}

void HttpRequester::onResponseBody(const char *buf, size_t size) {
    _res_body.append(buf, size);
}

void HttpRequester::onResponseCompleted(const SockException &ex) {
    const_cast<Parser &>(response()).setContent(std::move(_res_body));
    if (_on_result) {
        _on_result(ex, response());
        _on_result = nullptr;
    }
}

void HttpRequester::startRequester(const string &url, const HttpRequesterResult &on_result, float timeout_sec) {
    _on_result = on_result;
    setCompleteTimeout(timeout_sec * 1000);
    sendRequest(url);
}

void HttpRequester::clear() {
    HttpClientImp::clear();
    _res_body.clear();
    _on_result = nullptr;
}

void HttpRequester::setOnResult(const HttpRequesterResult &onResult) {
    _on_result = onResult;
}

////////////////////////////////////////////////////////////////////////

#if !defined(DISABLE_REPORT)
static constexpr auto s_interval_second = 60 * 5;
static constexpr auto s_report_url = "http://report.zlmediakit.com:8888/index/api/report";
extern const char kServerName[];

static std::string httpBody() {
    HttpArgs args;
    auto &os = args["os"];
#if defined(_WIN32)
    os = "windows";
#elif defined(__ANDROID__)
    os = "android";
#elif defined(__linux__)
    os = "linux";
#elif defined(OS_IPHONE)
    os = "ios";
#elif defined(__MACH__)
    os = "macos";
#else
    os = "unknow";
#endif

    auto &arch = args["arch"];
#if defined(__x86_64__) || defined(__amd64__)
    arch = "x86_64";
#elif defined(__x86__) || defined(__X86__) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
    arch = "x86";
#elif defined(__arm64__) || defined(__aarch64__)
    arch = "arm64";
#elif defined(__arm__)
    arch = "arm";
#elif defined(__loognarch__)
    arch = "loognarch";
#elif defined(__riscv) || defined(__riscv__)
    arch = "riscv";
#elif defined(__mipsl__) || defined(__mips__)
    arch = "mipsl";
#else
    arch = "unknow";
#endif

    auto &compiler = args["compiler"];
#if defined(__clang__)
    compiler = "clang";
#elif defined(_MSC_VER)
    compiler = "msvc";
#elif defined(__MINGW32__)
    compiler = "mingw";
#elif defined(__CYGWIN__)
    compiler = "cygwin";
#elif defined(__GNUC__)
    compiler = "gcc";
#elif defined(__ICC__)
    compiler = "icc";
#else
    compiler = "unknow";
#endif
    args["cplusplus"] = __cplusplus;
    args["build_date"] = __DATE__;
    args["version"] = kServerName;
    args["exe_name"] = exeName();

#if NDEBUG
    args["release"] = 1;
#else
    args["release"] = 0;
#endif

#if ENABLE_RTPPROXY
    args["rtp_proxy"] = 1;
#else
    args["rtp_proxy"] = 0;
#endif

#if ENABLE_HLS
    args["hls"] = 1;
#else
    args["hls"] = 0;
#endif

#if ENABLE_WEBRTC
    args["webrtc"] = 1;
#else
    args["webrtc"] = 0;
#endif

#if ENABLE_SCTP
    args["sctp"] = 1;
#else
    args["sctp"] = 0;
#endif

#if ENABLE_SRT
    args["srt"] = 1;
#else
    args["srt"] = 0;
#endif

#if ENABLE_MP4
    args["mp4"] = 1;
#else
    args["mp4"] = 0;
#endif

#if ENABLE_OPENSSL
    args["openssl"] = 1;
#else
    args["openssl"] = 0;
#endif

    args["rand_str"] = makeRandStr(32);
    for (auto &pr : mINI::Instance()) {
        // 只获取转协议相关配置
        if (pr.first.find("protocol.") == 0) {
            args[pr.first] = pr.second;
        }
    }
    return args.make();
}

static void sendReport() {
    static HttpRequester::Ptr requester = std::make_shared<HttpRequester>();
    // 获取一次静态信息，定时上报主要方便统计在线实例个数
    static auto body = httpBody();

    requester->setMethod("POST");
    requester->setBody(body);
    requester->startRequester(s_report_url, nullptr, s_interval_second);
}

static toolkit::onceToken s_token([]() {
    NoticeCenter::Instance().addListener(nullptr, EventPollerPool::kOnStarted, [](EventPollerPool &pool, size_t &size) {
        pool.getPoller()->doDelayTask(s_interval_second * 1000, []() {
            sendReport();
            return s_interval_second * 1000;
        });
    });
});

#endif

} // namespace mediakit
