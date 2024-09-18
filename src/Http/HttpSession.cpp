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
#include "Common/config.h"
#include "Common/strCoding.h"
#include "HttpSession.h"
#include "HttpConst.h"
#include "Util/base64.h"
#include "Util/SHA1.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

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
    header.emplace("Allow", "GET, POST, HEAD, OPTIONS");
    GET_CONFIG(bool, allow_cross_domains, Http::kAllowCrossDomains);
    if (allow_cross_domains) {
        header.emplace("Access-Control-Allow-Origin", "*");
        header.emplace("Access-Control-Allow-Headers", "*");
        header.emplace("Access-Control-Allow-Methods", "GET, POST, HEAD, OPTIONS");
    }
    header.emplace("Access-Control-Allow-Credentials", "true");
    header.emplace("Access-Control-Request-Methods", "GET, POST, OPTIONS");
    header.emplace("Access-Control-Request-Headers", "Accept,Accept-Language,Content-Language,Content-Type");
    sendResponse(200, true, nullptr, header);
}

ssize_t HttpSession::onRecvHeader(const char *header, size_t len) {
    using func_type = void (HttpSession::*)();
    static unordered_map<string, func_type> s_func_map;
    static onceToken token([]() {
        s_func_map.emplace("GET", &HttpSession::onHttpRequest_GET);
        s_func_map.emplace("POST", &HttpSession::onHttpRequest_POST);
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

    size_t content_len;
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

    if (content_len > _max_req_size) {
        // // 不定长body或超大body ////  [AUTO-TRANSLATED:8d66ee77]
        // // Indefinite length body or oversized body ////
        if (content_len != SIZE_MAX) {
            WarnL << "Http body size is too huge: " << content_len << " > " << _max_req_size
                  << ", please set " << Http::kMaxReqSize << " in config.ini file.";
        }

        size_t received = 0;
        auto parser = std::move(_parser);
        _on_recv_body = [this, parser, received, content_len](const char *data, size_t len) mutable {
            received += len;
            onRecvUnlimitedContent(parser, data, len, content_len, received);
            if (received < content_len) {
                // 还没收满  [AUTO-TRANSLATED:cecc867e]
                // Not yet received
                return true;
            }

            // 收满了  [AUTO-TRANSLATED:0c9cebd7]
            // Received full
            setContentLen(0);
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
    if (_is_live_stream) {
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
    auto Sec_WebSocket_Accept = encodeBase64(SHA1::encode_bin(Sec_WebSocket_Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

    KeyValue headerOut;
    headerOut["Upgrade"] = "websocket";
    headerOut["Connection"] = "Upgrade";
    headerOut["Sec-WebSocket-Accept"] = Sec_WebSocket_Accept;
    if (!_parser["Sec-WebSocket-Protocol"].empty()) {
        headerOut["Sec-WebSocket-Protocol"] = _parser["Sec-WebSocket-Protocol"];
    }

    auto res_cb = [this, headerOut]() {
        _live_over_websocket = true;
        sendResponse(101, false, nullptr, headerOut, nullptr, true);
    };

    auto res_cb_flv = [this, headerOut]() mutable {
        _live_over_websocket = true;
        headerOut.emplace("Cache-Control", "no-store");
        sendResponse(101, false, nullptr, headerOut, nullptr, true);
    };

    // 判断是否为websocket-flv  [AUTO-TRANSLATED:31682d7a]
    // Determine whether it is websocket-flv
    if (checkLiveStreamFlv(res_cb_flv)) {
        // 这里是websocket-flv直播请求  [AUTO-TRANSLATED:4bea5956]
        // This is a websocket-flv live request
        return true;
    }

    // 判断是否为websocket-ts  [AUTO-TRANSLATED:9e8eb374]
    // Determine whether it is websocket-ts
    if (checkLiveStreamTS(res_cb)) {
        // 这里是websocket-ts直播请求  [AUTO-TRANSLATED:8ab08dd6]
        // This is a websocket-ts live request
        return true;
    }

    // 判断是否为websocket-fmp4  [AUTO-TRANSLATED:318f793f]
    // Determine whether it is websocket-fmp4
    if (checkLiveStreamFMP4(res_cb)) {
        // 这里是websocket-fmp4直播请求  [AUTO-TRANSLATED:ccf0c1e2]
        // This is a websocket-fmp4 live request
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
            strong_self->async([onRes, err]() { onRes(err); });
        }
    };

    auto flag = NOTICE_EMIT(BroadcastMediaPlayedArgs, Broadcast::kBroadcastMediaPlayed, _media_info, invoker, *this);
    if (!flag) {
        // 该事件无人监听,默认不鉴权  [AUTO-TRANSLATED:e1fbc6ae]
        // No one is listening to this event, no authentication by default
        onRes("");
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
            ret.set(static_pointer_cast<SockInfo>(weak_self.lock()));
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
            ret.set(static_pointer_cast<SockInfo>(weak_self.lock()));
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

    GET_CONFIG(bool, allow_cross_domains, Http::kAllowCrossDomains);
    if (allow_cross_domains && !_origin.empty()) {
        headerOut.emplace("Access-Control-Allow-Origin", _origin);
        headerOut.emplace("Access-Control-Allow-Credentials", "true");
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
        str += pr.second;
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
