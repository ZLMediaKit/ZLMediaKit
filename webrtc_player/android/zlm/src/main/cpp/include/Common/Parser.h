/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_PARSER_H
#define ZLMEDIAKIT_PARSER_H

#include <map>
#include <string>
#include "Util/util.h"

namespace mediakit {

// 从字符串中提取子字符串
std::string findSubString(const char *buf, const char *start, const char *end, size_t buf_size = 0);
// 把url解析为主机地址和端口号,兼容ipv4/ipv6/dns
void splitUrl(const std::string &url, std::string &host, uint16_t &port);
// 解析proxy url,仅支持http
void parseProxyUrl(const std::string &proxy_url, std::string &proxy_host, uint16_t &proxy_port, std::string &proxy_auth);

struct StrCaseCompare {
    bool operator()(const std::string &__x, const std::string &__y) const { return strcasecmp(__x.data(), __y.data()) < 0; }
};

class StrCaseMap : public std::multimap<std::string, std::string, StrCaseCompare> {
public:
    using Super = std::multimap<std::string, std::string, StrCaseCompare>;

    std::string &operator[](const std::string &k) {
        auto it = find(k);
        if (it == end()) {
            it = Super::emplace(k, "");
        }
        return it->second;
    }

    template <typename K, typename V>
    void emplace(K &&k, V &&v) {
        auto it = find(k);
        if (it != end()) {
            return;
        }
        Super::emplace(std::forward<K>(k), std::forward<V>(v));
    }

    template <typename K, typename V>
    void emplace_force(K &&k, V &&v) {
        Super::emplace(std::forward<K>(k), std::forward<V>(v));
    }
};

// rtsp/http/sip解析类
class Parser {
public:
    // 解析http/rtsp/sip请求，需要确保buf以\0结尾
    void parse(const char *buf, size_t size);

    // 获取命令字，如GET/POST
    const std::string &method() const;

    // 请求时，获取中间url，不包含?后面的参数
    const std::string &url() const;
    // 回复时，获取状态码，如200/404
    const std::string &status() const;

    // 获取中间url，包含?后面的参数
    std::string fullUrl() const;

    // 请求时，获取协议名，如HTTP/1.1
    const std::string &protocol() const;
    // 回复时，获取状态字符串，如 OK/Not Found
    const std::string &statusStr() const;

    // 根据header key名，获取请求header value值
    const std::string &operator[](const char *name) const;

    // 获取http body或sdp
    const std::string &content() const;

    // 清空，为了重用
    void clear();

    // 获取?后面的参数
    const std::string &params() const;

    // 重新设置url
    void setUrl(std::string url);

    // 重新设置content
    void setContent(std::string content);

    // 获取header列表
    StrCaseMap &getHeader() const;

    // 获取url参数列表
    StrCaseMap &getUrlArgs() const;

    // 解析?后面的参数
    static StrCaseMap parseArgs(const std::string &str, const char *pair_delim = "&", const char *key_delim = "=");

    static std::string mergeUrl(const std::string &base_url, const std::string &path);

private:
    std::string _method;
    std::string _url;
    std::string _protocol;
    std::string _content;
    std::string _params;
    mutable StrCaseMap _headers;
    mutable StrCaseMap _url_args;
};

// 解析rtsp url的工具类
class RtspUrl {
public:
    bool _is_ssl;
    uint16_t _port;
    std::string _url;
    std::string _user;
    std::string _passwd;
    std::string _host;

public:
    void parse(const std::string &url);

private:
    void setup(bool, const std::string &, const std::string &, const std::string &);
};

} // namespace mediakit

#endif // ZLMEDIAKIT_PARSER_H
