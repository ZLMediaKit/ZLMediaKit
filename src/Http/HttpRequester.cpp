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
#include <memory>
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
    if (ex && _retry++ < _max_retry) {
        std::weak_ptr<HttpRequester> weak_self = std::static_pointer_cast<HttpRequester>(shared_from_this());
        getPoller()->doDelayTask(_retry_delay, [weak_self](){
            if (auto self = weak_self.lock()) {
                InfoL << "resend request " << self->getUrl() << " with retry " << self->getRetry();
                self->sendRequest(self->getUrl());
            }
            return 0;
        });
        return ;
    }
    const_cast<Parser &>(response()).setContent(std::move(_res_body));
    if (_on_result) {
        _on_result(ex, response());
        _on_result = nullptr;
    }
}

void HttpRequester::setRetry(size_t count, size_t delay) {
    InfoL << "setRetry max=" << count << ", delay=" << delay;
    _max_retry = count;
    _retry_delay = delay;
}

void HttpRequester::startRequester(const string &url, const HttpRequesterResult &on_result, float timeout_sec) {
    _on_result = on_result;
    _retry = 0;
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

#if (defined(_WIN32) && !defined(WIN32))
#define WIN32 _WIN32
#elif (defined(WIN32) && !defined(_WIN32))
#define _WIN32 WIN32
#endif

#if (defined(_WIN32) && !defined(_MSC_VER) && !defined(_WIN64))
#ifndef __i386__
#define __i386__
#endif
#elif defined(_MSC_VER)
#if (defined(_M_IX86) && !defined(__i386__))
#define __i386__
#endif
#endif

#ifndef __i386__
#if (defined(__386__) || defined(__I386__) || _M_IX86)
#define __i386__
#endif
#endif

#if (defined(__i386__) && !defined(__I386__))
#define __I386__
#endif

#if (defined(__x86_64__) && !defined(__x86_64))
#define __x86_64
#endif

#if (defined(__x86_64) && !defined(__x86_64__))
#define __x86_64__
#endif

#if (defined(_M_AMD64)) && (!defined(__amd64__))
#define __amd64__
#endif

#if (defined(__amd64) && !defined(__amd64__))
#define __amd64__
#endif

#if (defined(__amd64__) && !defined(__amd64))
#define __amd64
#endif
    
#if (defined(_M_ARM64) && !defined(__arm64__))
#define __arm64__
#endif

#if (defined(_M_X64) && !defined(__x86_64__))
#define __x86_64__
#endif
    
#if (defined(_M_ARM) && !defined(__arm__))
#define __arm__
#endif

#if (defined(__i386__) || defined(__amd64__)) && (!defined(__x86__))
#if !(defined(_MSC_VER) && defined(__amd64__))
#define __x86__ // MSVC doesn't support inline assembly in x64
#endif
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
    args["start_time"] = getTimeStr("%Y-%m-%d %H:%M:%S");

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

#if ENABLE_FFMPEG
    args["ffmpeg"] = 1;
#else
    args["ffmpeg"] = 0;
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
    // http超时时间设置为30秒
    requester->startRequester(s_report_url, nullptr, 30);
}

static toolkit::onceToken s_token([]() {
    NoticeCenter::Instance().addListener(nullptr, "kBroadcastEventPollerPoolStarted", [](EventPollerPool &pool, size_t &size) {
        // 第一次汇报在程序启动后5分钟
        pool.getPoller()->doDelayTask(5 * 60 * 1000, []() {
            sendReport();
            // 后续每一个小时汇报一次
            return 60 * 60 * 1000;
        });
    });
});

#endif

} // namespace mediakit
