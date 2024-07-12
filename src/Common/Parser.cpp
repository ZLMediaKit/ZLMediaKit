/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cinttypes>
#include "Parser.h"
#include "strCoding.h"
#include "Util/base64.h"
#include "Network/sockutil.h"
#include "Common/macros.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

string findSubString(const char *buf, const char *start, const char *end, size_t buf_size) {
    if (buf_size <= 0) {
        buf_size = strlen(buf);
    }
    auto msg_start = buf;
    auto msg_end = buf + buf_size;
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

void Parser::parse(const char *buf, size_t size) {
    clear();
    auto ptr = buf;
    while (true) {
        auto next_line = strchr(ptr, '\n');
        auto offset = 1;
        CHECK(next_line && next_line > ptr);
        if (*(next_line - 1) == '\r') {
            next_line -= 1;
            offset = 2;
        }
        if (ptr == buf) {
            auto blank = strchr(ptr, ' ');
            CHECK(blank > ptr && blank < next_line);
           _method = std::string(ptr, blank - ptr);
            auto next_blank = strchr(blank + 1, ' ');
            CHECK(next_blank && next_blank < next_line);
            _url.assign(blank + 1, next_blank);
            auto pos = _url.find('?');
            if (pos != string::npos) {
                _params = _url.substr(pos + 1);
                _url_args = parseArgs(_params);
                _url = _url.substr(0, pos);
            }
            _protocol = std::string(next_blank + 1, next_line);
        } else {
            auto pos = strchr(ptr, ':');
            CHECK(pos > ptr && pos < next_line);
            std::string key { ptr, static_cast<std::size_t>(pos - ptr) };
            std::string value;
            if (pos[1] == ' ') {
                value.assign(pos + 2, next_line);
            } else {
                value.assign(pos + 1, next_line);
            }
            _headers.emplace_force(trim(std::move(key)), trim(std::move(value)));
        }
        ptr = next_line + offset;
        if (strncmp(ptr, "\r\n", 2) == 0) { // 协议解析完毕
            _content.assign(ptr + 2, buf + size);
            break;
        }
    }
}

const string &Parser::method() const {
    return _method;
}

const string &Parser::url() const {
    return _url;
}

const std::string &Parser::status() const {
    return url();
}

string Parser::fullUrl() const {
    if (_params.empty()) {
        return _url;
    }
    return _url + "?" + _params;
}

const string &Parser::protocol() const {
    return _protocol;
}

const std::string &Parser::statusStr() const {
    return protocol();
}

static std::string kNull;

const string &Parser::operator[](const char *name) const {
    auto it = _headers.find(name);
    if (it == _headers.end()) {
        return kNull;
    }
    return it->second;
}

const string &Parser::content() const {
    return _content;
}

void Parser::clear() {
    _method.clear();
    _url.clear();
    _params.clear();
    _protocol.clear();
    _content.clear();
    _headers.clear();
    _url_args.clear();
}

const string &Parser::params() const {
    return _params;
}

void Parser::setUrl(string url) {
    _url = std::move(url);
}

void Parser::setContent(string content) {
    _content = std::move(content);
}

StrCaseMap &Parser::getHeader() const {
    return _headers;
}

StrCaseMap &Parser::getUrlArgs() const {
    return _url_args;
}

StrCaseMap Parser::parseArgs(const string &str, const char *pair_delim, const char *key_delim) {
    StrCaseMap ret;
    auto arg_vec = split(str, pair_delim);
    for (auto &key_val : arg_vec) {
        if (key_val.empty()) {
            // 忽略
            continue;
        }
        auto pos = key_val.find(key_delim);
        if (pos != string::npos) {
            auto key = trim(std::string(key_val, 0, pos));
            auto val = trim(key_val.substr(pos + strlen(key_delim)));
            ret.emplace_force(std::move(key), std::move(val));
        } else {
            trim(key_val);
            if (!key_val.empty()) {
                ret.emplace_force(std::move(key_val), "");
            }
        }
    }
    return ret;
}

std::string Parser::mergeUrl(const string &base_url, const string &path) {
    // 以base_url为基础, 合并path路径生成新的url, path支持相对路径和绝对路径
    if (base_url.empty()) {
        return path;
    }
    if (path.empty()) {
        return base_url;
    }
    // 如果包含协议，则直接返回
    if (path.find("://") != string::npos) {
        return path;
    }

    string protocol = "http://";
    size_t protocol_end = base_url.find("://");
    if (protocol_end != string::npos) {
        protocol = base_url.substr(0, protocol_end + 3);
    }
    // 如果path以"//"开头，则直接拼接协议
    if (path.find("//") == 0) {
        return protocol + path.substr(2);
    }
    string host;
    size_t pos = 0;
    if (protocol_end != string::npos) {
        pos = base_url.find('/', protocol_end + 3);
        host = base_url.substr(0, pos);
        if (pos == string::npos) {
            pos = base_url.size();
        } else {
            pos++;
        }
    }
    // 如果path以"/"开头，则直接拼接协议和主机
    if (path[0] == '/') {
        return host + path;
    }
    vector<string> path_parts;
    size_t next_pos = 0;
    if (!host.empty()) {
        path_parts.emplace_back(host);
    }
    while ((next_pos = base_url.find('/', pos)) != string::npos) {
        path_parts.emplace_back(base_url.substr(pos, next_pos - pos));
        pos = next_pos + 1;
    }
    pos = 0;
    while ((next_pos = path.find('/', pos)) != string::npos) {
        string part = path.substr(pos, next_pos - pos);
        if (part == "..") {
            if (!path_parts.empty() && !path_parts.back().empty()) {
                if (path_parts.size() > 1 || protocol_end == string::npos) {
                    path_parts.pop_back();
                }
            }
        } else if (part != "." && !part.empty()) {
            path_parts.emplace_back(part);
        }
        pos = next_pos + 1;
    }

    string part = path.substr(pos);
    if (part != ".." && part != "." && !part.empty()) {
        path_parts.emplace_back(part);
    }
    stringstream final_url;
    for (size_t i = 0; i < path_parts.size(); ++i) {
        if (i == 0) {
            final_url << path_parts[i];
        } else {
            final_url << '/' << path_parts[i];
        }
    }
    return final_url.str();
}

void RtspUrl::parse(const string &strUrl) {
    auto schema = findSubString(strUrl.data(), nullptr, "://");
    bool is_ssl = strcasecmp(schema.data(), "rtsps") == 0;
    // 查找"://"与"/"之间的字符串，用于提取用户名密码
    auto middle_url = findSubString(strUrl.data(), "://", "/");
    if (middle_url.empty()) {
        middle_url = findSubString(strUrl.data(), "://", nullptr);
    }
    auto pos = middle_url.rfind('@');
    if (pos == string::npos) {
        // 并没有用户名密码
        return setup(is_ssl, strUrl, "", "");
    }

    // 包含用户名密码
    auto user_pwd = middle_url.substr(0, pos);
    auto suffix = strUrl.substr(schema.size() + 3 + pos + 1);
    auto url = StrPrinter << "rtsp://" << suffix << endl;
    if (user_pwd.find(":") == string::npos) {
        return setup(is_ssl, url, user_pwd, "");
    }
    auto user = findSubString(user_pwd.data(), nullptr, ":");
    auto pwd = findSubString(user_pwd.data(), ":", nullptr);
    return setup(is_ssl, url, user, pwd);
}

void RtspUrl::setup(bool is_ssl, const string &url, const string &user, const string &passwd) {
    auto ip = findSubString(url.data(), "://", "/");
    if (ip.empty()) {
        ip = split(findSubString(url.data(), "://", NULL), "?")[0];
    }
    uint16_t port = is_ssl ? 322 : 554;
    splitUrl(ip, ip, port);

    _url = std::move(url);
    _user = strCoding::UrlDecodeUserOrPass(user);
    _passwd = strCoding::UrlDecodeUserOrPass(passwd);
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
        // 没有冒号，未指定端口;或者是纯粹的ipv6地址
        host = url;
        checkHost(host);
        return;
    }
    CHECK(pos > 0, "invalid url:", url);
    CHECK(sscanf(url.data() + pos + 1, "%" SCNu16, &port) == 1, "parse port from url failed:", url);
    host = url.substr(0, pos);
    checkHost(host);
}

void parseProxyUrl(const std::string &proxy_url, std::string &proxy_host, uint16_t &proxy_port, std::string &proxy_auth) {
    // 判断是否包含http://, 如果是则去掉
    std::string host;
    auto pos = proxy_url.find("://");
    if (pos != string::npos) {
        host = proxy_url.substr(pos + 3);
    } else {
        host = proxy_url;
    }
    // 判断是否包含用户名和密码
    pos = host.rfind('@');
    if (pos != string::npos) {
        proxy_auth = encodeBase64(host.substr(0, pos));
        host = host.substr(pos + 1, host.size());
    }
    splitUrl(host, proxy_host, proxy_port);
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

} // namespace mediakit
