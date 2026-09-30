/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <algorithm>
#include <set>
#include "Common/config.h"
#include "Common/strCoding.h"
#include "HttpSession.h"
#include "HttpCookieManager.h"
#include "HttpConst.h"
#include "Common/Parser.h"
#include "Util/base64.h"
#include "Util/SHA1.h"
#include "Util/util.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

// 判断该跨域域名是否在白名单内
// Check whether the cross-origin domain is in the whitelist
static bool isOriginAllowed(const string &origin) {
    using OriginSet = std::set<std::string, StrCaseCompare>;
    GET_CONFIG_FUNC(OriginSet, allow_origins, Http::kAllowOrigins, [](const string &str) {
        OriginSet ret;
        for (auto &item : split(str, ",")) {
            trim(item);
            if (!item.empty()) {
                ret.emplace(item);
            }
        }
        return ret;
    });
    // 包含*则允许所有域名跨域
    // Allow all origins when * is configured
    if (allow_origins.find("*") != allow_origins.end()) {
        return true;
    }
    return allow_origins.find(origin) != allow_origins.end();
}

HttpSession::HttpSession(const Socket::Ptr &pSock) : Session(pSock) {
    // 设置默认参数  [AUTO-TRANSLATED:ae5b72e6]
    // Set default parameters
    setMaxReqSize(0);
    setTimeoutSec(0);
}

void HttpSession::onHttpRequest_HEAD() {
    // 暂时全部返回200 OK，因为HTTP GET存在按需生成流的操作，所以不能按照HTTP GET的流程返回  [AUTO-TRANSLATED:0ce05db5]
    // Temporarily return 200 OK for all, because HTTP GET has on-demand generation stream operations, so it cannot return according to the HTTP GET process
    // 如果直接返回404，那么又会导致按需生成流的逻辑失效，所以HTTP HEAD在静态文件或者已存在资源时才有效  [AUTO-TRANSLATED:ea2b6faa]
    // If you return 404 directly, it will also cause the on-demand generation stream logic to fail, so HTTP HEAD is only valid for static files or existing resources
    // 对于按需生成流的直播场景并不适用  [AUTO-TRANSLATED:5a47bf00]
    // Not applicable to live streaming scenarios that generate streams on demand
    sendResponse(200, false);
}

void HttpSession::onHttpRequest_OPTIONS() {
    KeyValue header;
    header.emplace("Allow", "GET, POST, PUT, HEAD, OPTIONS, DELETE");
    if (!_origin.empty() && isOriginAllowed(_origin)) {
        // 回显origin而不是返回*，避免与Access-Control-Allow-Credentials冲突
        // Echo the origin instead of returning *, to avoid conflicting with Access-Control-Allow-Credentials
        header.emplace("Access-Control-Allow-Origin", _origin);
        header.emplace("Access-Control-Allow-Credentials", "true");
        // 带凭证的请求不支持通配符，此处回显客户端声明的请求头
        // Credentialed requests do not support wildcards, so echo the headers declared by the client here
        auto &req_headers = _parser["Access-Control-Request-Headers"];
        header.emplace("Access-Control-Allow-Headers", req_headers.empty() ? "*" : req_headers);
        header.emplace("Access-Control-Allow-Methods", "GET, POST, PUT, HEAD, OPTIONS, DELETE");
        // 预检结果缓存1天，减少预检请求次数
        // Cache the preflight result for 1 day to reduce preflight requests
        header.emplace("Access-Control-Max-Age", "86400");
        // 跨域响应内容随origin变化，提示缓存服务器按Origin区分缓存
        // The cross-origin response varies with origin, tell caches to vary on Origin
        header.emplace("Vary", "Origin");
    }
    sendResponse(200, true, nullptr, header);
}

ssize_t HttpSession::onRecvHeader(const char *header, size_t len) {
    using func_type = void (HttpSession::*)();
    static unordered_map<string, func_type> s_func_map;
    static onceToken token([]() {
        s_func_map.emplace("GET", &HttpSession::onHttpRequest_GET);
        s_func_map.emplace("POST", &HttpSession::onHttpRequest_POST);
        s_func_map.emplace("PUT", &HttpSession::onHttpRequest_POST);
        // DELETE命令用于whip/whep用，只用于触发http api  [AUTO-TRANSLATED:f3b7aaea]
        // DELETE command is used for whip/whep, only used to trigger http api
        s_func_map.emplace("DELETE", &HttpSession::onHttpRequest_POST);
        s_func_map.emplace("HEAD", &HttpSession::onHttpRequest_HEAD);
        s_func_map.emplace("OPTIONS", &HttpSession::onHttpRequest_OPTIONS);
    });

    _parser.parse(header, len);
    CHECK(_parser.url()[0] == '/');
    _origin = _parser["Origin"];

    urlDecode(_parser);
    auto &cmd = _parser.method();
    auto it = s_func_map.find(cmd);
    if (it == s_func_map.end()) {
        WarnP(this) << "Http method not supported: " << cmd;
        sendResponse(405, true);
        return 0;
    }

    uint64_t content_len;
    auto &content_len_str = _parser["Content-Length"];
    if (content_len_str.empty()) {
        if (it->first == "POST") {
            // Http post未指定长度，我们认为是不定长的body  [AUTO-TRANSLATED:3578206b]
            // Http post does not specify length, we consider it to be an indefinite length body
            WarnL << "Received http post request without content-length, consider it to be unlimited length";
            content_len = SIZE_MAX;
        } else {
            content_len = 0;
        }
    } else {
        // 已经指定长度  [AUTO-TRANSLATED:a360c374]
        // Length has been specified
        content_len = atoll(content_len_str.data());
    }

    if (content_len == 0) {
        // // 没有body的情况，直接触发回调 ////  [AUTO-TRANSLATED:f2988336]
        // // No body case, trigger callback directly ////
        (this->*(it->second))();
        _parser.clear();
        // 如果设置了_on_recv_body, 那么说明后续要处理body  [AUTO-TRANSLATED:2dac5fc2]
        // If _on_recv_body is set, it means that the body will be processed later
        return _on_recv_body ? -1 : 0;
    }

    HttpBody::Ptr body;
    if (_parser.method() == "PUT" || _parser.method() == "POST") {
        NOTICE_EMIT(BroadcastBeforeHttpRequestArgs, Broadcast::kBroadcastBeforeHttpRequest, _parser, body, *this);
    }

    // 自定义HttpBody直接消费网络分片，不经过HttpRequestSplitter的内存缓存；因此maxReqSize只负责选择缓存策略，
    // 不能作为这里的上传上限，否则未携带Content-Length的正常流式上传会在默认40KB左右被错误拒绝。
    // A custom HttpBody consumes network fragments directly without HttpRequestSplitter buffering. Therefore maxReqSize
    // only selects the buffering strategy and must not be used as the upload limit, or normal unknown-length uploads
    // would be rejected at roughly the default 40KB threshold.
    if (body) {
        GET_CONFIG(uint64_t, max_upload_size_config, Http::kMaxUploadSize);
        // 已知长度可以在接收body前判断是否超限，避免先向HttpBody写入数据再拒绝请求;
        // 如果上传文件时不指定content-len，也直接拒绝，因为后续没法触发文件上传完毕事件
        if (content_len > max_upload_size_config) {
            WarnL << "Http upload size is too huge or no content-len provided: " << content_len << " > " << max_upload_size_config
                  << ", please set " << Http::kMaxUploadSize << " in config.ini file.";
            sendResponse(413, true);
            _parser.clear();
            // 仍返回不定长body模式并丢弃连接关闭前可能到达的数据，防止splitter把body误当成下一条请求头。
            // Keep the splitter in variable-body mode and discard data arriving before close, so body bytes are not
            // interpreted as another request header.
            _on_recv_body = [](const char *, size_t) { return true; };
            return -1;
        }

        size_t received = 0;
        _on_recv_body = [this, received, content_len, body, it](const char *data, size_t len) mutable {
            auto remain = content_len - received;
            if (len > remain) {
                // 告知HttpFileStorage，写入的数据长度超过content_len，触发文件删除操作
                body->writeData(data, len, content_len);
                // 上传的数据超过声明的content-len， 直接拒绝
                sendResponse(413, true);
                WarnL << "Upload file size larger than content_len: " << received + len << " > " << content_len;
                return false;
            }

            received += len;
            body->writeData(data, len, content_len);
            if (received < content_len) {
                // 还没收满  [AUTO-TRANSLATED:cecc867e]
                // Not yet received
                return true;
            }
            // 收满了  [AUTO-TRANSLATED:0c9cebd7]
            // Received full
            setContentLen(0);
            _parser.setBody(std::move(body));
            (this->*(it->second))();
            _parser.clear();
            return false;
        };
        // 声明后续都是body；Http body在本对象缓冲，不通过HttpRequestSplitter保存  [AUTO-TRANSLATED:0012b6c1]
        // Declare that the following is all body; Http body is buffered in this object, not saved through HttpRequestSplitter
        return -1;
    }

    // 未提供自定义HttpBody时保持原有语义：maxReqSize仅决定是否放弃内存整包缓存并改为分片回调，
    // 不在本分支中把它扩展为普通请求体的硬上限。
    // Without a custom HttpBody, preserve the original behavior: maxReqSize only switches from whole-body buffering
    // to fragment callbacks; it is not extended into a hard limit for ordinary request bodies in this branch.
    if (content_len > _max_req_size) {
        // // 不定长body或超大body ////  [AUTO-TRANSLATED:8d66ee77]
        // // Indefinite length body or oversized body ////
        if (content_len != SIZE_MAX) {
            WarnL << "Http body size is too huge: " << content_len << " > " << _max_req_size
                  << ", please set " << Http::kMaxReqSize << " in config.ini file.";
        }

        size_t received = 0;
        _on_recv_body = [this, received, content_len](const char *data, size_t len) mutable {
            received += len;
            onRecvUnlimitedContent(_parser, data, len, content_len, received);
            if (received < content_len) {
                // 还没收满  [AUTO-TRANSLATED:cecc867e]
                // Not yet received
                return true;
            }

            // 收满了  [AUTO-TRANSLATED:0c9cebd7]
            // Received full
            setContentLen(0);
            _parser.clear();
            return false;
        };
        // 声明后续都是body；Http body在本对象缓冲，不通过HttpRequestSplitter保存  [AUTO-TRANSLATED:0012b6c1]
        // Declare that the following is all body; Http body is buffered in this object, not saved through HttpRequestSplitter
        return -1;
    }

    // // body size明确指定且小于最大值的情况 ////  [AUTO-TRANSLATED:f1f1ee5d]
    // // Body size is explicitly specified and less than the maximum value ////
    _on_recv_body = [this, it](const char *data, size_t len) mutable {
        // 收集body完毕  [AUTO-TRANSLATED:981ad2c8]
        // Body collection complete
        _parser.setContent(std::string(data, len));
        (this->*(it->second))();
        _parser.clear();

        // _on_recv_body置空  [AUTO-TRANSLATED:437a201a]
        // _on_recv_body is cleared
        return false;
    };

    // 声明body长度，通过HttpRequestSplitter缓存然后一次性回调到_on_recv_body  [AUTO-TRANSLATED:3b11cfb7]
    // Declare the body length, cache it through HttpRequestSplitter and then callback to _on_recv_body at once
    return content_len;
}

void HttpSession::onRecvContent(const char *data, size_t len) {
    if (_on_recv_body && !_on_recv_body(data, len)) {
        _on_recv_body = nullptr;
    }
}

void HttpSession::onRecv(const Buffer::Ptr &pBuf) {
    _ticker.resetTime();
    input(pBuf->data(), pBuf->size());
}

void HttpSession::onError(const SockException &err) {
    // LL-CMAF由跨请求复用的LlCmafPlayer在会话过期时统一上报流量，不能按单个HTTP请求重复上报。
    // LL-CMAF flow is reported once by the cross-request LlCmafPlayer when its session expires;
    // individual HTTP requests must not report it again.
    if (_is_live_stream && _media_info.schema != LLCMAF_SCHEMA) {
        // flv/ts播放器  [AUTO-TRANSLATED:5b444fd9]
        // flv/ts player
        uint64_t duration = _ticker.createdTime() / 1000;
        WarnP(this) << "FLV/TS/FMP4播放器(" << _media_info.shortUrl() << ")断开:" << err << ",耗时(s):" << duration;

        GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);
        if (_total_bytes_usage >= iFlowThreshold * 1024) {
            NOTICE_EMIT(BroadcastFlowReportArgs, Broadcast::kBroadcastFlowReport, _media_info, _total_bytes_usage, duration, true, *this);
        }
        return;
    }
}

void HttpSession::setTimeoutSec(size_t keep_alive_sec) {
    if (!keep_alive_sec) {
        GET_CONFIG(size_t, s_keep_alive_sec, Http::kKeepAliveSecond);
        keep_alive_sec = s_keep_alive_sec;
    }
    _keep_alive_sec = keep_alive_sec;
    getSock()->setSendTimeOutSecond(keep_alive_sec);
}

void HttpSession::setMaxReqSize(size_t max_req_size) {
    if (!max_req_size) {
        GET_CONFIG(size_t, s_max_req_size, Http::kMaxReqSize);
        max_req_size = s_max_req_size;
    }
    _max_req_size = max_req_size;
    setMaxCacheSize(max_req_size);
}

void HttpSession::onManager() {
    if (_ticker.elapsedTime() > _keep_alive_sec * 1000) {
        // http超时  [AUTO-TRANSLATED:6f2fdd1f]
        // http timeout
        shutdown(SockException(Err_timeout, "session timeout"));
    }
}

bool HttpSession::checkWebSocket() {
    auto Sec_WebSocket_Key = _parser["Sec-WebSocket-Key"];
    if (Sec_WebSocket_Key.empty()) {
        return false;
    }
    _is_websocket = true;
    auto Sec_WebSocket_Accept = encodeBase64(SHA1::encode_bin(Sec_WebSocket_Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

    KeyValue headerOut;
    headerOut["Upgrade"] = "websocket";
    headerOut["Connection"] = "Upgrade";
    headerOut["Sec-WebSocket-Accept"] = Sec_WebSocket_Accept;
    if (!_parser["Sec-WebSocket-Protocol"].empty()) {
        headerOut["Sec-WebSocket-Protocol"] = _parser["Sec-WebSocket-Protocol"];
    }

    auto res_cb = []() {
        // 改成先回复http头模式，以解决按需播放场景下websocket请求pending问题：#4553
    };

    auto res_immediately = [this, headerOut]() mutable {
        headerOut.emplace("Cache-Control", "no-store");
        sendResponse(101, false, nullptr, headerOut, nullptr, true);
        _live_over_websocket = true;
    };

    // 判断是否为websocket-flv  [AUTO-TRANSLATED:31682d7a]
    // Determine whether it is websocket-flv
    if (checkLiveStreamFlv(res_cb)) {
        // 这里是websocket-flv直播请求  [AUTO-TRANSLATED:4bea5956]
        // This is a websocket-flv live request
        res_immediately();
        return true;
    }

    // 判断是否为websocket-ts  [AUTO-TRANSLATED:9e8eb374]
    // Determine whether it is websocket-ts
    if (checkLiveStreamTS(res_cb)) {
        // 这里是websocket-ts直播请求  [AUTO-TRANSLATED:8ab08dd6]
        // This is a websocket-ts live request
        res_immediately();
        return true;
    }

    // 判断是否为websocket-fmp4  [AUTO-TRANSLATED:318f793f]
    // Determine whether it is websocket-fmp4
    if (checkLiveStreamFMP4(res_cb)) {
        // 这里是websocket-fmp4直播请求  [AUTO-TRANSLATED:ccf0c1e2]
        // This is a websocket-fmp4 live request
        res_immediately();
        return true;
    }

    // 这是普通的websocket连接  [AUTO-TRANSLATED:754721f8]
    // This is a normal websocket connection
    if (!onWebSocketConnect(_parser)) {
        sendResponse(501, true, nullptr, headerOut);
        return true;
    }
    sendResponse(101, false, nullptr, headerOut, nullptr, true);
    return true;
}

bool HttpSession::checkLiveStream(const string &schema, const string &url_suffix, const function<void(const MediaSource::Ptr &src)> &cb) {
    std::string url = _parser.url();
    auto it = _parser.getUrlArgs().find("schema");
    if (it != _parser.getUrlArgs().end()) {
        if (strcasecmp(it->second.c_str(), schema.c_str())) {
            // unsupported schema
            return false;
        }
    } else {
        auto prefix_size = url_suffix.size();
        if (url.size() < prefix_size || strcasecmp(url.data() + (url.size() - prefix_size), url_suffix.data())) {
            // 未找到后缀  [AUTO-TRANSLATED:6635499a]
            // Suffix not found
            return false;
        }
        // url去除特殊后缀  [AUTO-TRANSLATED:31c0c080]
        // Remove special suffix from url
        url.resize(url.size() - prefix_size);
    }

    // 带参数的url  [AUTO-TRANSLATED:074764b0]
    // Url with parameters
    if (!_parser.params().empty()) {
        url += "?";
        url += _parser.params();
    }

    // 解析带上协议+参数完整的url  [AUTO-TRANSLATED:5cdc7e68]
    // Parse the complete url with protocol + parameters
    _media_info.parse(schema + "://" + _parser["Host"] + url);

    if (_media_info.app.empty() || _media_info.stream.empty()) {
        // url不合法  [AUTO-TRANSLATED:9aad134e]
        // URL is invalid
        return false;
    }

    if (_is_websocket) {
        _media_info.protocol = overSsl() ? "wss" : "ws";
    } else {
        _media_info.protocol = overSsl() ? "https" : "http";
    }

    bool close_flag = !strcasecmp(_parser["Connection"].data(), "close");
    weak_ptr<HttpSession> weak_self = static_pointer_cast<HttpSession>(shared_from_this());

    // 鉴权结果回调  [AUTO-TRANSLATED:021df191]
    // Authentication result callback
    auto onRes = [cb, weak_self, close_flag](const string &err) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            // 本对象已经销毁  [AUTO-TRANSLATED:713e0f23]
            // This object has been destroyed
            return;
        }

        if (!err.empty()) {
            // 播放鉴权失败  [AUTO-TRANSLATED:64f99eeb]
            // Playback authentication failed
            strong_self->sendResponse(401, close_flag, nullptr, KeyValue(), std::make_shared<HttpStringBody>(err));
            return;
        }

        // 异步查找直播流  [AUTO-TRANSLATED:7cde5dac]
        // Asynchronously find live stream
        MediaSource::findAsync(strong_self->_media_info, strong_self, [weak_self, close_flag, cb](const MediaSource::Ptr &src) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                // 本对象已经销毁  [AUTO-TRANSLATED:713e0f23]
                // This object has been destroyed
                return;
            }
            if (!src) {
                // 未找到该流  [AUTO-TRANSLATED:2699ef82]
                // Stream not found
                strong_self->sendNotFound(close_flag);
            } else {
                strong_self->_is_live_stream = true;
                // 触发回调  [AUTO-TRANSLATED:ae2ff258]
                // Trigger callback
                cb(src);
            }
        });
    };

    Broadcast::AuthInvoker invoker = [weak_self, onRes](const string &err) {
        if (auto strong_self = weak_self.lock()) {
            strong_self->async([onRes, err]() { onRes(err); }, false);
        }
    };

    auto flag = NOTICE_EMIT(BroadcastMediaPlayedArgs, Broadcast::kBroadcastMediaPlayed, _media_info, invoker, *this);
    if (!flag) {
        // 该事件无人监听,默认不鉴权  [AUTO-TRANSLATED:e1fbc6ae]
        // No one is listening to this event, no authentication by default
        invoker("");
    }
    return true;
}

// http-fmp4 链接格式:http://vhost-url:port/app/streamid.live.mp4?key1=value1&key2=value2  [AUTO-TRANSLATED:c0174f8f]
// http-fmp4 link format: http://vhost-url:port/app/streamid.live.mp4?key1=value1&key2=value2
bool HttpSession::checkLiveStreamFMP4(const function<void()> &cb) {
    return checkLiveStream(FMP4_SCHEMA, ".live.mp4", [this, cb](const MediaSource::Ptr &src) {
        auto fmp4_src = dynamic_pointer_cast<FMP4MediaSource>(src);
        assert(fmp4_src);
        if (!cb) {
            // 找到源，发送http头，负载后续发送  [AUTO-TRANSLATED:ac272410]
            // Found the source, send the http header, and send the load later
            sendResponse(200, false, HttpFileManager::getContentType(".mp4").data(), KeyValue(), nullptr, true);
        } else {
            // 自定义发送http头  [AUTO-TRANSLATED:b8a8f683]
            // Custom send http header
            cb();
        }

        // 直播牺牲延时提升发送性能  [AUTO-TRANSLATED:7c6616c9]
        // Live streaming sacrifices delay to improve sending performance
        setSocketFlags();
        onWrite(std::make_shared<BufferString>(fmp4_src->getInitSegment()), true);
        weak_ptr<HttpSession> weak_self = static_pointer_cast<HttpSession>(shared_from_this());
        fmp4_src->pause(false);
        _fmp4_reader = fmp4_src->getRing()->attach(getPoller());
        _fmp4_reader->setGetInfoCB([weak_self]() {
            Any ret;
            ret.set(static_pointer_cast<Session>(weak_self.lock()));
            return ret;
        });
        _fmp4_reader->setDetachCB([weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                // 本对象已经销毁  [AUTO-TRANSLATED:713e0f23]
                // This object has been destroyed
                return;
            }
            strong_self->shutdown(SockException(Err_shutdown, "fmp4 ring buffer detached"));
        });
        _fmp4_reader->setReadCB([weak_self](const FMP4MediaSource::RingDataType &fmp4_list) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                // 本对象已经销毁  [AUTO-TRANSLATED:713e0f23]
                // This object has been destroyed
                return;
            }
            size_t i = 0;
            auto size = fmp4_list->size();
            fmp4_list->for_each([&](const FMP4Packet::Ptr &ts) { strong_self->onWrite(ts, ++i == size); });
        });
    });
}

// /////////////////////////// LL-HLS / LL-DASH ///////////////////////////

// LL请求的url后缀
// URL suffixes of the LL requests
static const string kLlHlsSuffix = "/hls.ll.m3u8";
static const string kLlDashSuffix = "/dash.ll.mpd";
static const string kLlDirName = "/ll/";
static const string kLlPlayerCookieName = "ZL_LL_CMAF_PLAYER";
static const string kLlPlayerIdArg = "player_id";
// 30秒内未访问任何LL-CMAF资源时，视为播放已断开。
// Consider playback disconnected when no LL-CMAF resource is accessed for 30 seconds.
static constexpr uint32_t kLlPlayerCookieLife = 30;

static bool urlSuffixEqual(const string &url, const string &suffix) {
    return url.size() >= suffix.size() && strcasecmp(url.data() + (url.size() - suffix.size()), suffix.data()) == 0;
}

static void replaceAll(string &str, const string &from, const string &to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != string::npos) {
        str.replace(pos, from.size(), to);
        pos += to.size();
    }
}

/**
 * http chunked分块的长度头, 例如 "1000\r\n"
 * The size header of an HTTP chunked block, e.g. "1000\r\n"
 */
static Buffer::Ptr makeHttpChunkHead(size_t len) {
    char buf[32];
    auto head = snprintf(buf, sizeof(buf), "%zx\r\n", len);
    return std::make_shared<BufferString>(std::string(buf, (size_t)head));
}

/**
 * http chunked分块数据后的结束符 "\r\n"
 * 与长度头、数据分3次下发, socket层会合并成一次sendmsg的多个iovec, 从而避免拼接时的内存拷贝
 * The trailing CRLF of an HTTP chunked block
 * It is sent separately from the size header and the data; the socket layer merges them into a
 * single sendmsg with several iovecs, which avoids the copy needed when concatenating them
 */
static const BufferString::Ptr &getHttpChunkTail() {
    static const auto tail = std::make_shared<BufferString>("\r\n");
    return tail;
}

/**
 * http chunked编码的结束标记
 * The terminating block of the HTTP chunked encoding
 */
static const BufferString::Ptr& makeHttpChunkEnd() {
    static const auto end = std::make_shared<BufferString>("0\r\n\r\n");
    return end;
}

/**
 * 解析分片文件名
 *   "12.m4s"   -> 完整分片, part = -1
 *   "12.3.m4s" -> 部分分片, part = 3
 * Parse the segment file name
 *   "12.m4s"   -> full segment, part = -1
 *   "12.3.m4s" -> partial segment, part = 3
 */
static bool parseLlSegmentName(const string &name, uint64_t &msn, int &part) {
    if (!end_with(name, ".m4s")) {
        return false;
    }
    auto body = name.substr(0, name.size() - 4);
    auto dot = body.find('.');
    if (dot == string::npos) {
        msn = strtoull(body.data(), nullptr, 10);
        part = -1;
        return true;
    }
    msn = strtoull(body.substr(0, dot).data(), nullptr, 10);
    part = atoi(body.substr(dot + 1).data());
    return true;
}

bool HttpSession::checkLiveStreamLL() {
    auto url = _parser.url();
    bool is_dash = urlSuffixEqual(url, kLlDashSuffix);
    bool is_playlist = is_dash || urlSuffixEqual(url, kLlHlsSuffix);

    string suffix;
    if (is_playlist) {
        suffix = is_dash ? kLlDashSuffix : kLlHlsSuffix;
    } else {
        // 分片请求: /app/stream/ll/{msn}[.{part}].m4s,
        // 把"/ll/..."整体作为后缀, checkLiveStream会据此推导出app/stream
        // Segment request: /app/stream/ll/{msn}[.{part}].m4s
        // Use "/ll/..." as the suffix; checkLiveStream derives app/stream from it
        auto pos = url.rfind(kLlDirName);
        if (pos == string::npos) {
            return false;
        }
        suffix = url.substr(pos);
    }

    // 保存查询参数: checkLiveStream异步查找期间onRecvHeader会清空_parser
    // Save query parameters before the asynchronous checkLiveStream lookup; onRecvHeader clears _parser on return
    auto url_args = _parser.params();
    // 复用checkLiveStream完成鉴权与异步查找媒体源
    // Reuse checkLiveStream for the authentication and asynchronous media source lookup
    return checkLiveStream(LLCMAF_SCHEMA, suffix, [this, is_playlist, is_dash, suffix, url, url_args](const MediaSource::Ptr &src) {
        auto ll_src = dynamic_pointer_cast<LlMediaSource>(src);
        if (!ll_src) {
            sendNotFound(false);
            return;
        }
        auto args = Parser::parseArgs(url_args);
        auto it = args.find(kLlPlayerIdArg);
        if (is_playlist && it == args.end()) {
            auto player = std::make_shared<LlCmafPlayer>(ll_src);
            player->setSession(static_pointer_cast<Session>(shared_from_this()));
            player->setMediaInfo(_media_info);
            auto cookie = HttpCookieManager::Instance().addCookie(kLlPlayerCookieName, "", kLlPlayerCookieLife, Any(player));
            player->setId(cookie->getCookie());
            KeyValue header;
            header["Location"] = url + "?" + url_args + (url_args.empty() ? "" : "&") + kLlPlayerIdArg + "=" + cookie->getCookie();
            sendResponse(302, false, nullptr, header);
            return;
        }
        if (it == args.end()) {
            sendNotFound(false);
            return;
        }
        auto cookie = HttpCookieManager::Instance().getCookie(kLlPlayerCookieName, it->second);
        if (!cookie) {
            sendNotFound(false);
            return;
        }
        LlCmafPlayer::Ptr player;
        try {
            player = cookie->getAttach<LlCmafPlayer>().shared_from_this();
        } catch (std::exception &) {
            sendNotFound(false);
            return;
        }
        if (!player || player->getSource() != ll_src) {
            sendNotFound(false);
            return;
        }
        cookie->updateTime();
        player->attach(getPoller());
        if (is_playlist) {
            onLlPlaylist(player, is_dash, url_args);
            return;
        }
        // suffix形如 "/ll/init.mp4" 或 "/ll/12.3.m4s"
        // suffix looks like "/ll/init.mp4" or "/ll/12.3.m4s"
        auto name = suffix.substr(kLlDirName.size());
        if (name == "init.mp4") {
            onLlInitSegment(player);
            return;
        }
        uint64_t msn;
        int part;
        if (!parseLlSegmentName(name, msn, part)) {
            sendNotFound(false);
            return;
        }
        if (part < 0) {
            onLlSegment(player, msn);
        } else {
            onLlPart(player, msn, part);
        }
    });
}

void HttpSession::onLlPlaylist(const LlCmafPlayer::Ptr &player, bool is_dash, const std::string &url_args) {
    auto source = player->getSource();
    auto store = source ? source->getStore() : nullptr;
    if (!store) {
        sendNotFound(false);
        return;
    }
    int64_t msn = -1;
    int part = -1;
    bool blocking = LlSegmentStore::parseBlockingParams(url_args, msn, part);

    auto weak_self = std::weak_ptr<HttpSession>(static_pointer_cast<HttpSession>(shared_from_this()));
    // 生成并下发播放列表(或mpd)
    // Generate and send the playlist (or mpd)
    auto send = [weak_self, player, store, is_dash]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        auto content = is_dash ? store->makeDashMpd() : store->makeHlsPlaylist();
        if (content.empty()) {
            strong_self->sendResponse(404, false, nullptr, KeyValue(), std::make_shared<HttpStringBody>("not ready"));
            return;
        }
        // 清单中的 init/segment/part 与 rendition-report 都必须携带同一个 player_id，
        // 才能汇聚一次播放所创建的全部 HTTP 链接。
        auto player_id = player->getId();
        if (is_dash) {
            replaceAll(content, "?session=", "?player_id=" + player_id + "&amp;session=");
        } else {
            replaceAll(content, "?session=", "?player_id=" + player_id + "&session=");
            replaceAll(content, "URI=\"hls.ll.m3u8\"", "URI=\"hls.ll.m3u8?player_id=" + player_id + "\"");
        }
        KeyValue header;
        // 播放列表必须禁止缓存, 否则播放器会一直拿到旧的live edge
        // The playlist must not be cached, otherwise the player keeps a stale live edge
        header["Cache-Control"] = "no-store";
        strong_self->sendResponse(200, false, is_dash ? "application/dash+xml" : "application/vnd.apple.mpegurl", header,
                                  std::make_shared<HttpStringBody>(std::move(content)));
    };

    // 先记录版本，再查询状态；等待注册期间若有新资源产生，会检测到版本变化并立即重新执行。
    auto event = part < 0 ? LlSegmentStore::EventType::Segment : LlSegmentStore::EventType::Part;
    auto event_version = store->getEventVersion(event);
    if (!blocking || store->checkBlockingCondition(msn, part)) {
        send();
        return;
    }

    // 阻塞重载只在清单可能变化时唤醒：请求Part等新Part，请求完整分片等新Segment。
    player->waitForEvent(event, event_version, getPoller(), store->getConfig().blocking_timeout_ms, send);
}

void HttpSession::onLlInitSegment(const LlCmafPlayer::Ptr &player) {
    auto source = player->getSource();
    auto store = source ? source->getStore() : nullptr;
    if (!store) {
        sendNotFound(false);
        return;
    }
    auto init = store->getInitSegment();
    if (init.empty()) {
        sendResponse(404, false, nullptr, KeyValue(), std::make_shared<HttpStringBody>("not ready"));
        return;
    }
    player->addByteUsage(init.size());
    KeyValue header;
    // 流重连后同一URL可能对应新的编码参数, 不可跨会话长期缓存
    // A stream restart can change codec parameters at the same URL; do not cache across sessions
    header["Cache-Control"] = "no-store";
    sendResponse(200, false, HttpFileManager::getContentType(".mp4").data(), header,
                 std::make_shared<HttpStringBody>(std::move(init)));
}

void HttpSession::onLlPart(const LlCmafPlayer::Ptr &player, uint64_t msn, int index) {
    auto source = player->getSource();
    auto store = source ? source->getStore() : nullptr;
    if (!store) {
        sendNotFound(false);
        return;
    }
    auto weak_self = std::weak_ptr<HttpSession>(static_pointer_cast<HttpSession>(shared_from_this()));
    auto ticker = std::make_shared<Ticker>();
    auto task = std::make_shared<function<void()>>();
    weak_ptr<function<void()>> weak_task = task;
    // 部分分片可能尚未产出(如PRELOAD-HINT指向的part), 挂起等待最多2个partDur
    // The partial segment may not exist yet (e.g. the one pointed to by PRELOAD-HINT),
    // hold the request for at most 2 partDur
    auto timeout = store->getConfig().part_dur_ms * 2;
    *task = [weak_self, player, store, msn, index, ticker, weak_task, timeout]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        auto event_version = store->getEventVersion(LlSegmentStore::EventType::Part);
        auto part = store->getPart(msn, index);
        if (part) {
            player->addByteUsage(part->getSize());
            KeyValue header;
            // MSN在流重连后会复用, 避免客户端缓存到上一会话的part
            // MSN values are reused after a stream restart; avoid serving a part from the previous session
            header["Cache-Control"] = "no-store";
            strong_self->sendResponse(200, false, HttpFileManager::getContentType(".m4s").data(), header,
                                      std::make_shared<HttpMultiBufferBody>(part->getBuffers()));
            return;
        }
        if (ticker->elapsedTime() >= (uint64_t)timeout) {
            strong_self->sendResponse(404, false, nullptr, KeyValue(), std::make_shared<HttpStringBody>("part not found"));
            return;
        }
        if (auto task = weak_task.lock()) {
            player->waitForEvent(LlSegmentStore::EventType::Part, event_version, strong_self->getPoller(),
                                 timeout - ticker->elapsedTime(), [task]() { (*task)(); });
        }
    };
    (*task)();
}

void HttpSession::onLlSegment(const LlCmafPlayer::Ptr &player, uint64_t msn) {
    auto source = player->getSource();
    auto store = source ? source->getStore() : nullptr;
    if (!store) {
        sendNotFound(false);
        return;
    }
    auto weak_self = std::weak_ptr<HttpSession>(static_pointer_cast<HttpSession>(shared_from_this()));
    auto ticker = std::make_shared<Ticker>();
    auto task = std::make_shared<function<void()>>();
    weak_ptr<function<void()>> weak_task = task;
    auto timeout = store->getConfig().blocking_timeout_ms;
    *task = [weak_self, player, store, msn, ticker, weak_task, timeout]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        auto event_version = store->getEventVersion(LlSegmentStore::EventType::Part);
        auto seg = store->getSegment(msn);
        if (seg) {
            std::list<Buffer::Ptr> buffers;
            if (store->getCompletedSegmentBuffers(msn, buffers)) {
                // 分片已经完成, 一次返回全部数据
                // The segment is complete, return all its data at once
                size_t size = 0;
                for (auto &buffer : buffers) {
                    size += buffer->size();
                }
                player->addByteUsage(size);
                KeyValue header;
                // MSN在流重连后会复用, 避免客户端缓存到上一会话的segment
                // MSN values are reused after a stream restart; avoid serving a segment from the previous session
                header["Cache-Control"] = "no-store";
                strong_self->sendResponse(200, false, HttpFileManager::getContentType(".m4s").data(), header,
                                          std::make_shared<HttpMultiBufferBody>(std::move(buffers)));
            } else {
                // 分片正在生成: 以chunked编码流式下发, 客户端可边收边播
                // The segment is being produced: stream it with the chunked encoding so the
                // client can start playing while it is still being downloaded
                KeyValue header;
                header["Cache-Control"] = "no-store";
                // 用chunked编码分块下发直至分片完成, 故不回Content-Length
                // No Content-Length; the chunked encoding delimits the response, which ends
                // once the segment is complete
                header["Transfer-Encoding"] = "chunked";
                strong_self->sendResponse(200, false, HttpFileManager::getContentType(".m4s").data(), header, nullptr, true);
                strong_self->streamLlSegment(player, seg);
            }
            return;
        }
        // 分片尚未到达: 挂起等待, 超时则404 (客户端外推的分片号可能超前)
        // The segment has not arrived yet: hold the request; on timeout return 404
        // (the segment number extrapolated by the client may run ahead)
        if (ticker->elapsedTime() >= timeout) {
            strong_self->sendResponse(404, false, nullptr, KeyValue(), std::make_shared<HttpStringBody>("segment not found"));
            return;
        }
        if (auto task = weak_task.lock()) {
            player->waitForEvent(LlSegmentStore::EventType::Part, event_version, strong_self->getPoller(),
                                 timeout - ticker->elapsedTime(), [task]() { (*task)(); });
        }
    };
    (*task)();
}

void HttpSession::streamLlSegment(const LlCmafPlayer::Ptr &player, const std::shared_ptr<LlSegment> &seg) {
    auto source = player->getSource();
    auto store = source ? source->getStore() : nullptr;
    if (!store) {
        shutdown(SockException(Err_shutdown, "ll media source expired"));
        return;
    }
    auto weak_self = std::weak_ptr<HttpSession>(static_pointer_cast<HttpSession>(shared_from_this()));
    auto offset = std::make_shared<size_t>(0);
    auto ticker = std::make_shared<Ticker>();
    auto task = std::make_shared<function<void()>>();
    weak_ptr<function<void()>> weak_task = task;
    // 即使长时间无新数据也不无限挂起, 避免socket长期空闲
    // Do not hold forever when no new data arrives, to avoid a long idle socket
    auto idle_timeout = store->getConfig().blocking_timeout_ms;
    *task = [weak_self, player, store, seg, offset, ticker, weak_task, idle_timeout]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        auto event_versions = store->getEventVersions();
        std::list<Buffer::Ptr> buffers;
        size_t total_size = 0;
        bool completed = false;
        if (!store->readSegmentBuffers(seg, *offset, 1024 * 1024, buffers, total_size, completed)) {
            strong_self->shutdown(SockException(Err_shutdown, "ll segment invalid"));
            return;
        }
        size_t size = 0;
        for (auto &buf : buffers) {
            size += buf->size();
        }
        if (size) {
            *offset += size;
            player->addByteUsage(size);
            ticker->resetTime();
            // chunked编码下发本段增量数据
            // Send this incremental block with the chunked encoding
            // 长度头/数据/结束符分3次下发, 期间关闭flush, 最后统一flush,
            // socket层会把它们合并成一次sendmsg的多个iovec, 既没有拼接拷贝也没有多余的系统调用
            // The size header, data and trailing CRLF are sent separately with flushing disabled,
            // then flushed together; the socket layer merges them into one sendmsg with several
            // iovecs, so there is neither a concatenation copy nor an extra syscall
            strong_self->onWrite(makeHttpChunkHead(size), false);
            for (auto &buf : buffers) {
                strong_self->onWrite(buf, false);
            }
            strong_self->onWrite(getHttpChunkTail(), true);
        }
        if (completed && *offset >= total_size) {
            // 分片发送完毕, 下发chunked结束标记并flush
            // The segment has been fully sent, send the terminating block and flush
            strong_self->onWrite(makeHttpChunkEnd(), true);
            return;
        }
        if (ticker->elapsedTime() >= idle_timeout) {
            // 长时间没有新数据, 主动结束chunked响应, 避免连接泄漏
            // No new data for a long time, end the chunked response to avoid leaking the connection
            strong_self->onWrite(makeHttpChunkEnd(), true);
            return;
        }
        if (auto task = weak_task.lock()) {
            player->waitForEvents(event_versions, { LlSegmentStore::EventType::Part, LlSegmentStore::EventType::Segment },
                                  strong_self->getPoller(), idle_timeout - ticker->elapsedTime(), [task]() { (*task)(); });
        }
    };
    (*task)();
}

// http-ts 链接格式:http://vhost-url:port/app/streamid.live.ts?key1=value1&key2=value2  [AUTO-TRANSLATED:aa1a9151]
// http-ts link format: http://vhost-url:port/app/streamid.live.ts?key1=value1&key2=value2
bool HttpSession::checkLiveStreamTS(const function<void()> &cb) {
    return checkLiveStream(TS_SCHEMA, ".live.ts", [this, cb](const MediaSource::Ptr &src) {
        auto ts_src = dynamic_pointer_cast<TSMediaSource>(src);
        assert(ts_src);
        if (!cb) {
            // 找到源，发送http头，负载后续发送  [AUTO-TRANSLATED:ac272410]
            // Found the source, send the http header, and send the load later
            sendResponse(200, false, HttpFileManager::getContentType(".ts").data(), KeyValue(), nullptr, true);
        } else {
            // 自定义发送http头  [AUTO-TRANSLATED:b8a8f683]
            // Custom send http header
            cb();
        }

        // 直播牺牲延时提升发送性能  [AUTO-TRANSLATED:7c6616c9]
        // Live streaming sacrifices delay to improve sending performance
        setSocketFlags();
        weak_ptr<HttpSession> weak_self = static_pointer_cast<HttpSession>(shared_from_this());
        ts_src->pause(false);
        _ts_reader = ts_src->getRing()->attach(getPoller());
        _ts_reader->setGetInfoCB([weak_self]() {
            Any ret;
            ret.set(static_pointer_cast<Session>(weak_self.lock()));
            return ret;
        });
        _ts_reader->setDetachCB([weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                // 本对象已经销毁  [AUTO-TRANSLATED:713e0f23]
                // This object has been destroyed
                return;
            }
            strong_self->shutdown(SockException(Err_shutdown, "ts ring buffer detached"));
        });
        _ts_reader->setReadCB([weak_self](const TSMediaSource::RingDataType &ts_list) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                // 本对象已经销毁  [AUTO-TRANSLATED:713e0f23]
                // This object has been destroyed
                return;
            }
            size_t i = 0;
            auto size = ts_list->size();
            ts_list->for_each([&](const TSPacket::Ptr &ts) { strong_self->onWrite(ts, ++i == size); });
        });
    });
}

// http-flv 链接格式:http://vhost-url:port/app/streamid.live.flv?key1=value1&key2=value2  [AUTO-TRANSLATED:7e78aa20]
// http-flv link format: http://vhost-url:port/app/streamid.live.flv?key1=value1&key2=value2
bool HttpSession::checkLiveStreamFlv(const function<void()> &cb) {
    auto start_pts = atoll(_parser.getUrlArgs()["starPts"].data());
    return checkLiveStream(RTMP_SCHEMA, ".live.flv", [this, cb, start_pts](const MediaSource::Ptr &src) {
        auto rtmp_src = dynamic_pointer_cast<RtmpMediaSource>(src);
        assert(rtmp_src);
        if (!cb) {
            // 找到源，发送http头，负载后续发送  [AUTO-TRANSLATED:ac272410]
            // Found the source, send the http header, and send the load later
            KeyValue headerOut;
            headerOut["Cache-Control"] = "no-store";
            sendResponse(200, false, HttpFileManager::getContentType(".flv").data(), headerOut, nullptr, true);
        } else {
            // 自定义发送http头  [AUTO-TRANSLATED:b8a8f683]
            // Custom send http header
            cb();
        }
        // 直播牺牲延时提升发送性能  [AUTO-TRANSLATED:7c6616c9]
        // Live streaming sacrifices delay to improve sending performance
        setSocketFlags();

        // 非H264/AAC时打印警告日志，防止用户提无效问题  [AUTO-TRANSLATED:59ee60df]
        // Print warning log when it is not H264/AAC, to prevent users from raising invalid issues
        auto tracks = src->getTracks(false);
        for (auto &track : tracks) {
            switch (track->getCodecId()) {
                case CodecH264:
                case CodecAAC: break;
                default: {
                    WarnP(this) << "flv播放器一般只支持H264和AAC编码,该编码格式可能不被播放器支持:" << track->getCodecName();
                    break;
                }
            }
        }

        start(getPoller(), rtmp_src, start_pts);
    });
}

void HttpSession::onHttpRequest_GET() {
    // 先看看是否为WebSocket请求  [AUTO-TRANSLATED:98cd3a86]
    // First check if it is a WebSocket request
    if (checkWebSocket()) {
        // 后续都是websocket body数据  [AUTO-TRANSLATED:c4fcbdcf]
        // The following are all websocket body data
        _on_recv_body = [this](const char *data, size_t len) {
            WebSocketSplitter::decode((uint8_t *)data, len);
            // _contentCallBack是可持续的，后面还要处理后续数据  [AUTO-TRANSLATED:920e8c23]
            // _contentCallBack is sustainable, and subsequent data needs to be processed later
            return true;
        };
        return;
    }

    if (emitHttpEvent(false)) {
        // 拦截http api事件  [AUTO-TRANSLATED:2f5e319d]
        // Intercept http api events
        return;
    }

    if (checkLiveStreamLL()) {
        // 拦截LL-HLS/LL-DASH请求
        // Intercept LL-HLS / LL-DASH requests
        return;
    }

    if (checkLiveStreamFlv()) {
        // 拦截http-flv播放器  [AUTO-TRANSLATED:299f6449]
        // Intercept http-flv player
        return;
    }

    if (checkLiveStreamTS()) {
        // 拦截http-ts播放器  [AUTO-TRANSLATED:d9e303e4]
        // Intercept http-ts player
        return;
    }

    if (checkLiveStreamFMP4()) {
        // 拦截http-fmp4播放器  [AUTO-TRANSLATED:78cdf3a1]
        // Intercept http-fmp4 player
        return;
    }

    bool bClose = !strcasecmp(_parser["Connection"].data(), "close");
    weak_ptr<HttpSession> weak_self = static_pointer_cast<HttpSession>(shared_from_this());
    HttpFileManager::onAccessPath(*this, _parser, [weak_self, bClose](int code, const string &content_type,
                                                                      const StrCaseMap &responseHeader, const HttpBody::Ptr &body) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->async([weak_self, bClose, code, content_type, responseHeader, body]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->sendResponse(code, bClose, content_type.data(), responseHeader, body);
        });
    });
}

static string dateStr() {
    char buf[64];
    time_t tt = time(NULL);
    strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

class AsyncSenderData {
public:
    friend class AsyncSender;
    using Ptr = std::shared_ptr<AsyncSenderData>;
    AsyncSenderData(HttpSession::Ptr session, const HttpBody::Ptr &body, bool close_when_complete) {
        _session = std::move(session);
        _body = body;
        _close_when_complete = close_when_complete;
    }

private:
    std::weak_ptr<HttpSession> _session;
    HttpBody::Ptr _body;
    bool _close_when_complete;
    bool _read_complete = false;
};

class AsyncSender {
public:
    using Ptr = std::shared_ptr<AsyncSender>;
    static bool onSocketFlushed(const AsyncSenderData::Ptr &data) {
        if (data->_read_complete) {
            if (data->_close_when_complete) {
                // 发送完毕需要关闭socket  [AUTO-TRANSLATED:fe660e55]
                // Close socket after sending is complete
                shutdown(data->_session.lock());
            }
            return false;
        }

        GET_CONFIG(uint32_t, sendBufSize, Http::kSendBufSize);
        data->_body->readDataAsync(sendBufSize, [data](const Buffer::Ptr &sendBuf) {
            auto session = data->_session.lock();
            if (!session) {
                // 本对象已经销毁  [AUTO-TRANSLATED:713e0f23]
                // This object has been destroyed
                return;
            }
            session->async([data, sendBuf]() {
                auto session = data->_session.lock();
                if (!session) {
                    // 本对象已经销毁  [AUTO-TRANSLATED:713e0f23]
                    // This object has been destroyed
                    return;
                }
                onRequestData(data, session, sendBuf);
            }, false);
        });
        return true;
    }

private:
    static void onRequestData(const AsyncSenderData::Ptr &data, const std::shared_ptr<HttpSession> &session, const Buffer::Ptr &sendBuf) {
        session->_ticker.resetTime();
        if (sendBuf && session->send(sendBuf) != -1) {
            // 文件还未读完，还需要继续发送  [AUTO-TRANSLATED:c454ca1a]
            // The file has not been read completely, and needs to be sent continuously
            if (!session->isSocketBusy()) {
                // socket还可写，继续请求数据  [AUTO-TRANSLATED:041df414]
                // Socket can still write, continue to request data
                onSocketFlushed(data);
            }
            return;
        }
        // 文件写完了  [AUTO-TRANSLATED:a9f8c117]
        // The file is written
        data->_read_complete = true;
        if (!session->isSocketBusy() && data->_close_when_complete) {
            shutdown(session);
        }
    }

    static void shutdown(const std::shared_ptr<HttpSession> &session) {
        if (session) {
            session->shutdown(SockException(Err_shutdown, StrPrinter << "close connection after send http body completed."));
        }
    }
};

void HttpSession::sendResponse(int code,
                               bool bClose,
                               const char *pcContentType,
                               const HttpSession::KeyValue &header,
                               const HttpBody::Ptr &body,
                               bool no_content_length) {
    if (_live_over_websocket) {
        WebSocketHeader ws_header;
        ws_header._fin = true;
        ws_header._reserved = 0;
        ws_header._opcode = WebSocketHeader::CLOSE;
        ws_header._mask_flag = false;
        uint16_t why = htons(0xFFFF & code);
        std::string buffer;
        buffer.append(reinterpret_cast<char *>(&why), 2);
        if (body && code != 404) {
            buffer.append(body->readData(body->remainSize())->toString());
        } else {
            buffer.append("unknown reason");
        }
        WebSocketSplitter::encode(ws_header, std::make_shared<BufferString>(std::move(buffer)));
        return;
    }
    GET_CONFIG(string, charSet, Http::kCharSet);
    GET_CONFIG(uint32_t, keepAliveSec, Http::kKeepAliveSecond);

    // body默认为空  [AUTO-TRANSLATED:527ccb6f]
    // Body defaults to empty
    int64_t size = 0;
    if (body && body->remainSize()) {
        // 有body，获取body大小  [AUTO-TRANSLATED:0d5f4b9a]
        // There is a body, get the body size
        size = body->remainSize();
    }

    if (no_content_length) {
        // http-flv直播是Keep-Alive类型  [AUTO-TRANSLATED:0ef3adfe]
        // Http-flv live broadcast is Keep-Alive type
        bClose = false;
    } else if ((size_t)size >= SIZE_MAX || size < 0) {
        // 不固定长度的body，那么发送完body后应该关闭socket，以便浏览器做下载完毕的判断  [AUTO-TRANSLATED:fc714997]
        // If the body is not fixed length, then the socket should be closed after sending the body, so that the browser can judge the download completion
        bClose = true;
    }

    HttpSession::KeyValue &headerOut = const_cast<HttpSession::KeyValue &>(header);
    headerOut.emplace("Date", dateStr());
    headerOut.emplace("Server", kServerName);
    headerOut.emplace("Connection", bClose ? "close" : "keep-alive");

    if (!_origin.empty() && isOriginAllowed(_origin)) {
        headerOut.emplace("Access-Control-Allow-Origin", _origin);
        headerOut.emplace("Access-Control-Allow-Credentials", "true");
        // 跨域响应内容随origin变化，提示缓存服务器按Origin区分缓存
        // The cross-origin response varies with origin, tell caches to vary on Origin
        headerOut.emplace("Vary", "Origin");
    }

    if (!bClose) {
        string keepAliveString = "timeout=";
        keepAliveString += to_string(keepAliveSec);
        keepAliveString += ", max=100";
        headerOut.emplace("Keep-Alive", std::move(keepAliveString));
    }

    if (!no_content_length && size >= 0 && (size_t)size < SIZE_MAX) {
        // 文件长度为固定值,且不是http-flv强制设置Content-Length  [AUTO-TRANSLATED:185c02a8]
        // The file length is a fixed value, and it is not http-flv that forcibly sets Content-Length
        headerOut["Content-Length"] = to_string(size);
    }

    if (size && !pcContentType) {
        // 有body时，设置缺省类型  [AUTO-TRANSLATED:21c9b233]
        // When there is a body, set the default type
        pcContentType = "text/plain";
    }

    if ((size || no_content_length) && pcContentType) {
        // 有body时，设置文件类型  [AUTO-TRANSLATED:0dcbeecc]
        // When there is a body, set the file type
        string strContentType = pcContentType;
        strContentType += "; charset=";
        strContentType += charSet;
        headerOut.emplace("Content-Type", std::move(strContentType));
    }

    // 发送http头  [AUTO-TRANSLATED:cca51598]
    // Send http header
    string str;
    str.reserve(256);
    str += "HTTP/1.1 ";
    str += to_string(code);
    str += ' ';
    str += HttpConst::getHttpStatusMessage(code);
    str += "\r\n";
    for (auto &pr : header) {
        str += pr.first;
        str += ": ";
        // Strip CR and LF from header values to prevent HTTP response splitting (CWE-113).
        // Any injected \r or \n is silently removed; other characters pass through unchanged.
        for (char c : pr.second) {
            if (c != '\r' && c != '\n') {
                str += c;
            }
        }
        str += "\r\n";
    }
    str += "\r\n";
    SockSender::send(std::move(str));
    _ticker.resetTime();

    if (!size) {
        // 没有body  [AUTO-TRANSLATED:bf891e3a]
        // No body
        if (bClose) {
            shutdown(SockException(Err_shutdown, StrPrinter << "close connection after send http header completed with status code:" << code));
        }
        return;
    }

#if 0
    // sendfile跟共享mmap相比并没有性能上的优势，相反，sendfile还有功能上的缺陷，先屏蔽  [AUTO-TRANSLATED:4de77827]
    // Sendfile has no performance advantage over shared mmap, on the contrary, sendfile also has functional defects, so it is blocked first
    if (typeid(*this) == typeid(HttpSession) && !body->sendFile(getSock()->rawFD())) {
        // http支持sendfile优化  [AUTO-TRANSLATED:04f691f1]
        // Http supports sendfile optimization
        return;
    }
#endif

    GET_CONFIG(uint32_t, sendBufSize, Http::kSendBufSize);
    if (body->remainSize() > sendBufSize) {
        // 文件下载提升发送性能  [AUTO-TRANSLATED:500922cc]
        // File download improves sending performance
        setSocketFlags();
    }

    setSendFlushFlag(true);
    // 发送http body  [AUTO-TRANSLATED:e9fc35d6]
    // Send http body
    AsyncSenderData::Ptr data = std::make_shared<AsyncSenderData>(static_pointer_cast<HttpSession>(shared_from_this()), body, bClose);
    getSock()->setOnFlush([data]() { return AsyncSender::onSocketFlushed(data); });
    AsyncSender::onSocketFlushed(data);
}

void HttpSession::urlDecode(Parser &parser) {
    parser.setUrl(strCoding::UrlDecodePath(parser.url()));
    for (auto &pr : _parser.getUrlArgs()) {
        const_cast<string &>(pr.second) = strCoding::UrlDecodeComponent(pr.second);
    }
}

bool HttpSession::emitHttpEvent(bool doInvoke) {
    bool bClose = !strcasecmp(_parser["Connection"].data(), "close");
    // ///////////////////异步回复Invoker///////////////////////////////  [AUTO-TRANSLATED:6d0c5fda]
    // ///////////////////Asynchronous reply Invoker///////////////////////////////
    weak_ptr<HttpSession> weak_self = static_pointer_cast<HttpSession>(shared_from_this());
    HttpResponseInvoker invoker = [weak_self, bClose](int code, const KeyValue &headerOut, const HttpBody::Ptr &body) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->async([weak_self, bClose, code, headerOut, body]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                // 本对象已经销毁  [AUTO-TRANSLATED:713e0f23]
                // This object has been destroyed
                return;
            }
            strong_self->sendResponse(code, bClose, nullptr, headerOut, body);
        });
    };
    // /////////////////广播HTTP事件///////////////////////////  [AUTO-TRANSLATED:fff9769c]
    // /////////////////Broadcast HTTP event///////////////////////////
    bool consumed = false; // 该事件是否被消费
    NOTICE_EMIT(BroadcastHttpRequestArgs, Broadcast::kBroadcastHttpRequest, _parser, invoker, consumed, *this);
    if (!consumed && doInvoke) {
        // 该事件无人消费，所以返回404  [AUTO-TRANSLATED:8a890dec]
        // This event is not consumed, so return 404
        invoker(404, KeyValue(), HttpBody::Ptr());
    }
    return consumed;
}

std::string HttpSession::get_peer_ip() {
    GET_CONFIG(string, forwarded_ip_header, Http::kForwardedIpHeader);
    if (!forwarded_ip_header.empty() && !_parser.getHeader()[forwarded_ip_header].empty()) {
        return _parser.getHeader()[forwarded_ip_header];
    }
    return Session::get_peer_ip();
}

void HttpSession::onHttpRequest_POST() {
    emitHttpEvent(true);
}

void HttpSession::sendNotFound(bool bClose) {
    GET_CONFIG(string, notFound, Http::kNotFound);
    sendResponse(404, bClose, "text/html", KeyValue(), std::make_shared<HttpStringBody>(notFound));
}

void HttpSession::setSocketFlags() {
    GET_CONFIG(int, mergeWriteMS, General::kMergeWriteMS);
    if (mergeWriteMS > 0) {
        // 推流模式下，关闭TCP_NODELAY会增加推流端的延时，但是服务器性能将提高  [AUTO-TRANSLATED:c8ec8fb8]
        // In push mode, closing TCP_NODELAY will increase the delay of the push end, but the server performance will be improved
        SockUtil::setNoDelay(getSock()->rawFD(), false);
        // 播放模式下，开启MSG_MORE会增加延时，但是能提高发送性能  [AUTO-TRANSLATED:7b558ab9]
        // In playback mode, enabling MSG_MORE will increase the delay, but it can improve sending performance
        setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
    }
}

void HttpSession::onWrite(const Buffer::Ptr &buffer, bool flush) {
    if (flush) {
        // 需要flush那么一次刷新缓存  [AUTO-TRANSLATED:8d1ec961]
        // Need to flush, then flush the cache once
        HttpSession::setSendFlushFlag(true);
    }

    _ticker.resetTime();
    if (!_live_over_websocket) {
        _total_bytes_usage += buffer->size();
        send(buffer);
    } else {
        WebSocketHeader header;
        header._fin = true;
        header._reserved = 0;
        header._opcode = WebSocketHeader::BINARY;
        header._mask_flag = false;
        WebSocketSplitter::encode(header, buffer);
    }

    if (flush) {
        // 本次刷新缓存后，下次不用刷新缓存  [AUTO-TRANSLATED:f56139f7]
        // After this cache flush, the next time you don't need to flush the cache
        HttpSession::setSendFlushFlag(false);
    }
}

void HttpSession::onWebSocketEncodeData(Buffer::Ptr buffer) {
    _total_bytes_usage += buffer->size();
    send(std::move(buffer));
}

void HttpSession::onWebSocketDecodeComplete(const WebSocketHeader &header_in) {
    WebSocketHeader &header = const_cast<WebSocketHeader &>(header_in);
    header._mask_flag = false;

    switch (header._opcode) {
        case WebSocketHeader::CLOSE: {
            encode(header, nullptr);
            shutdown(SockException(Err_shutdown, "recv close request from client"));
            break;
        }

        default: break;
    }
}

void HttpSession::onDetach() {
    shutdown(SockException(Err_shutdown, "rtmp ring buffer detached"));
}

std::shared_ptr<FlvMuxer> HttpSession::getSharedPtr() {
    return dynamic_pointer_cast<FlvMuxer>(shared_from_this());
}

} /* namespace mediakit */
