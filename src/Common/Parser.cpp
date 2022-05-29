/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cinttypes>
#include "Parser.h"
#include "macros.h"
#include "Network/sockutil.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

string FindField(const char* buf, const char* start, const char *end ,size_t bufSize) {
    if(bufSize <=0 ){
        bufSize = strlen(buf);
    }
    const char *msg_start = buf, *msg_end = buf + bufSize;
    size_t len = 0;
    if (start != NULL) {
        len = strlen(start);
        msg_start = strstr(buf, start);
    }
    if (msg_start == NULL) {
        return "";
    }
    msg_start += len;
    if (end != NULL) {
        msg_end = strstr(msg_start, end);
        if (msg_end == NULL) {
            return "";
        }
    }
    return string(msg_start, msg_end);
}

void Parser::Parse(const char *buf) {
    //解析
    const char *start = buf;
    Clear();
    while (true) {
        auto line = FindField(start, NULL, "\r\n");
        if (line.size() == 0) {
            break;
        }
        if (start == buf) {
            _strMethod = FindField(line.data(), NULL, " ");
            auto strFullUrl = FindField(line.data(), " ", " ");
            auto args_pos = strFullUrl.find('?');
            if (args_pos != string::npos) {
                _strUrl = strFullUrl.substr(0, args_pos);
                _params = strFullUrl.substr(args_pos + 1);
                _mapUrlArgs = parseArgs(_params);
            } else {
                _strUrl = strFullUrl;
            }
            _strTail = FindField(line.data(), (strFullUrl + " ").data(), NULL);
        } else {
            auto field = FindField(line.data(), NULL, ": ");
            auto value = FindField(line.data(), ": ", NULL);
            if (field.size() != 0) {
                _mapHeaders.emplace_force(field, value);
            }
        }
        start = start + line.size() + 2;
        if (strncmp(start, "\r\n", 2) == 0) { //协议解析完毕
            _strContent = FindField(start, "\r\n", NULL);
            break;
        }
    }
}

const string &Parser::Method() const {
    return _strMethod;
}

const string &Parser::Url() const {
    return _strUrl;
}

string Parser::FullUrl() const {
    if (_params.empty()) {
        return _strUrl;
    }
    return _strUrl + "?" + _params;
}

const string &Parser::Tail() const {
    return _strTail;
}

const string &Parser::operator[](const char *name) const {
    auto it = _mapHeaders.find(name);
    if (it == _mapHeaders.end()) {
        return _strNull;
    }
    return it->second;
}

const string &Parser::Content() const {
    return _strContent;
}

void Parser::Clear() {
    _strMethod.clear();
    _strUrl.clear();
    _params.clear();
    _strTail.clear();
    _strContent.clear();
    _mapHeaders.clear();
    _mapUrlArgs.clear();
}

const string &Parser::Params() const {
    return _params;
}

void Parser::setUrl(string url) {
    this->_strUrl = std::move(url);
}

void Parser::setContent(string content) {
    this->_strContent = std::move(content);
}

StrCaseMap &Parser::getHeader() const {
    return _mapHeaders;
}

StrCaseMap &Parser::getUrlArgs() const {
    return _mapUrlArgs;
}

StrCaseMap Parser::parseArgs(const string &str, const char *pair_delim, const char *key_delim) {
    StrCaseMap ret;
    auto arg_vec = split(str, pair_delim);
    for (string &key_val : arg_vec) {
        if (key_val.empty()) {
            //忽略
            continue;
        }
        auto key = trim(FindField(key_val.data(), NULL, key_delim));
        if (!key.empty()) {
            auto val = trim(FindField(key_val.data(), key_delim, NULL));
            ret.emplace_force(key, val);
        } else {
            trim(key_val);
            if (!key_val.empty()) {
                ret.emplace_force(key_val, "");
            }
        }
    }
    return ret;
}

void RtspUrl::parse(const string &strUrl) {
    auto schema = FindField(strUrl.data(), nullptr, "://");
    bool is_ssl = strcasecmp(schema.data(), "rtsps") == 0;
    //查找"://"与"/"之间的字符串，用于提取用户名密码
    auto middle_url = FindField(strUrl.data(), "://", "/");
    if (middle_url.empty()) {
        middle_url = FindField(strUrl.data(), "://", nullptr);
    }
    auto pos = middle_url.rfind('@');
    if (pos == string::npos) {
        //并没有用户名密码
        return setup(is_ssl, strUrl, "", "");
    }

    //包含用户名密码
    auto user_pwd = middle_url.substr(0, pos);
    auto suffix = strUrl.substr(schema.size() + 3 + pos + 1);
    auto url = StrPrinter << "rtsp://" << suffix << endl;
    if (user_pwd.find(":") == string::npos) {
        return setup(is_ssl, url, user_pwd, "");
    }
    auto user = FindField(user_pwd.data(), nullptr, ":");
    auto pwd = FindField(user_pwd.data(), ":", nullptr);
    return setup(is_ssl, url, user, pwd);
}

void RtspUrl::setup(bool is_ssl, const string &url, const string &user, const string &passwd) {
    auto ip = FindField(url.data(), "://", "/");
    if (ip.empty()) {
        ip = split(FindField(url.data(), "://", NULL), "?")[0];
    }
    uint16_t port = is_ssl ? 322 : 554;
    splitUrl(ip, ip, port);

    _url = std::move(url);
    _user = std::move(user);
    _passwd = std::move(passwd);
    _host = std::move(ip);
    _port = port;
    _is_ssl = is_ssl;
}

static void inline checkHost(std::string &host) {
    if (host.back() == ']' && host.front() == '[') {
        // ipv6去除方括号
        host.pop_back();
        host.erase(0, 1);
        CHECK(SockUtil::is_ipv6(host.data()), "not a ipv6 address:", host);
    }
}

void splitUrl(const std::string &url, std::string &host, uint16_t &port) {
    CHECK(!url.empty(), "empty url");
    auto pos = url.rfind(':');
    if (pos == string::npos || url.back() == ']') {
        //没有冒号，未指定端口;或者是纯粹的ipv6地址
        host = url;
        checkHost(host);
        return;
    }
    CHECK(pos > 0, "invalid url:", port);
    CHECK(sscanf(url.data() + pos + 1, "%" SCNu16, &port) == 1, "parse port from url failed:", url);
    host = url.substr(0, pos);
    checkHost(host);
}

#if 0
//测试代码
static onceToken token([](){
    string host;
    uint16_t port;
    splitUrl("www.baidu.com:8880", host, port);
    splitUrl("192.168.1.1:8880", host, port);
    splitUrl("[::]:8880", host, port);
    splitUrl("[fe80::604d:4173:76e9:1009]:8880", host, port);
});
#endif

}//namespace mediakit