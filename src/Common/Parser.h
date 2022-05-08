/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_PARSER_H
#define ZLMEDIAKIT_PARSER_H

#include <map>
#include <string>
#include "Util/util.h"

namespace mediakit {

//从字符串中提取子字符串
std::string FindField(const char *buf, const char *start, const char *end, size_t bufSize = 0);
//把url解析为主机地址和端口号,兼容ipv4/ipv6/dns
void splitUrl(const std::string &url, std::string &host, uint16_t& port);

struct StrCaseCompare {
    bool operator()(const std::string &__x, const std::string &__y) const {
        return strcasecmp(__x.data(), __y.data()) < 0;
    }
};

class StrCaseMap : public std::multimap<std::string, std::string, StrCaseCompare> {
public:
    using Super = multimap<std::string, std::string, StrCaseCompare>;
    StrCaseMap() = default;
    ~StrCaseMap() = default;

    std::string &operator[](const std::string &k) {
        auto it = find(k);
        if (it == end()) {
            it = Super::emplace(k, "");
        }
        return it->second;
    }

    template<typename V>
    void emplace(const std::string &k, V &&v) {
        auto it = find(k);
        if (it != end()) {
            return;
        }
        Super::emplace(k, std::forward<V>(v));
    }

    template<typename V>
    void emplace_force(const std::string k, V &&v) {
        Super::emplace(k, std::forward<V>(v));
    }
};

//rtsp/http/sip解析类
class Parser {
public:
    Parser() = default;
    ~Parser() = default;

    //解析信令
    void Parse(const char *buf);

    //获取命令字
    const std::string &Method() const;

    //获取中间url，不包含?后面的参数
    const std::string &Url() const;

    //获取中间url，包含?后面的参数
    std::string FullUrl() const;

    //获取命令协议名
    const std::string &Tail() const;

    //根据header key名，获取请求header value值
    const std::string &operator[](const char *name) const;

    //获取http body或sdp
    const std::string &Content() const;

    //清空，为了重用
    void Clear();

    //获取?后面的参数
    const std::string &Params() const;

    //重新设置url
    void setUrl(std::string url);

    //重新设置content
    void setContent(std::string content);

    //获取header列表
    StrCaseMap &getHeader() const;

    //获取url参数列表
    StrCaseMap &getUrlArgs() const;

    //解析?后面的参数
    static StrCaseMap parseArgs(const std::string &str, const char *pair_delim = "&", const char *key_delim = "=");

private:
    std::string _strMethod;
    std::string _strUrl;
    std::string _strTail;
    std::string _strContent;
    std::string _strNull;
    std::string _params;
    mutable StrCaseMap _mapHeaders;
    mutable StrCaseMap _mapUrlArgs;
};

//解析rtsp url的工具类
class RtspUrl{
public:
    bool _is_ssl;
    uint16_t _port;
    std::string _url;
    std::string _user;
    std::string _passwd;
    std::string _host;

public:
    RtspUrl() = default;
    ~RtspUrl() = default;
    void parse(const std::string &url);

private:
    void setup(bool,const std::string &, const std::string &, const std::string &);
};

}//namespace mediakit

#endif //ZLMEDIAKIT_PARSER_H
