/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifdef ENABLE_WEBRTC
#ifndef MK_WEBRTC_API_H
#define MK_WEBRTC_API_H

#include "Common/Parser.h"
#include "Http/HttpSession.h"
#include "Network/Socket.h"
#include "jsoncpp/json.h"
#include <functional>
#include <string>

#include "../webrtc/WebRtcEchoTest.h"
#include "../webrtc/WebRtcPlayer.h"
#include "../webrtc/WebRtcPusher.h"
#include "../webrtc/WebRtcTransport.h"

using namespace mediakit;

namespace API {
typedef enum {
    NotFound = -500, //未找到
    Exception = -400, //代码抛异常
    InvalidArgs = -300, //参数不合法
    SqlFailed = -200, // sql执行失败
    AuthFailed = -100, //鉴权失败
    OtherFailed = -1, //业务代码执行失败，
    Success = 0 //执行成功
} ApiErr;
} // namespace API

class ApiRetException : public std::runtime_error {
public:
    ApiRetException(const char *str = "success", int code = API::Success)
        : runtime_error(str) {
        _code = code;
    }
    ~ApiRetException() = default;
    int code() { return _code; }

private:
    int _code;
};

class AuthException : public ApiRetException {
public:
    AuthException(const char *str)
        : ApiRetException(str, API::AuthFailed) {}
    ~AuthException() = default;
};

class InvalidArgsException : public ApiRetException {
public:
    InvalidArgsException(const char *str)
        : ApiRetException(str, API::InvalidArgs) {}
    ~InvalidArgsException() = default;
};

class SuccessException : public ApiRetException {
public:
    SuccessException()
        : ApiRetException("success", API::Success) {}
    ~SuccessException() = default;
};

using ApiArgsType = std::map<std::string, std::string, mediakit::StrCaseCompare>;
template <typename Args, typename First>
std::string getValue(Args &args, const First &first) {
    return args[first];
}

template <typename First>
std::string getValue(Json::Value &args, const First &first) {
    return args[first].asString();
}

template <typename First>
std::string getValue(std::string &args, const First &first) {
    return "";
}

template <typename First>
std::string getValue(const mediakit::Parser &parser, const First &first) {
    auto ret = parser.getUrlArgs()[first];
    if (!ret.empty()) {
        return ret;
    }
    return parser.getHeader()[first];
}

template <typename First>
std::string getValue(mediakit::Parser &parser, const First &first) {
    return getValue((const mediakit::Parser &)parser, first);
}

template <typename Args, typename First>
std::string getValue(const mediakit::Parser &parser, Args &args, const First &first) {
    auto ret = getValue(args, first);
    if (!ret.empty()) {
        return ret;
    }
    return getValue(parser, first);
}

template <typename Args>
class HttpAllArgs {
public:
    HttpAllArgs(const mediakit::Parser &parser, Args &args) {
        _get_args = [&args]() { return (void *)&args; };
        _get_parser = [&parser]() -> const mediakit::Parser & { return parser; };
        _get_value
            = [](HttpAllArgs &that, const std::string &key) { return getValue(that.getParser(), that.getArgs(), key); };
        _clone = [&](HttpAllArgs &that) {
            that._get_args = [args]() { return (void *)&args; };
            that._get_parser = [parser]() -> const mediakit::Parser & { return parser; };
            that._get_value = [](HttpAllArgs &that, const std::string &key) {
                return getValue(that.getParser(), that.getArgs(), key);
            };
            that._cache_able = true;
        };
    }

    HttpAllArgs(const HttpAllArgs &that) {
        if (that._cache_able) {
            _get_args = that._get_args;
            _get_parser = that._get_parser;
            _get_value = that._get_value;
            _cache_able = true;
        } else {
            that._clone(*this);
        }
    }

    ~HttpAllArgs() = default;

    template <typename Key>
    toolkit::variant operator[](const Key &key) const {
        return (toolkit::variant)_get_value(*(HttpAllArgs *)this, key);
    }

    const mediakit::Parser &getParser() const { return _get_parser(); }

    Args &getArgs() { return *((Args *)_get_args()); }

    const Args &getArgs() const { return *((Args *)_get_args()); }

private:
    bool _cache_able = false;
    std::function<void *()> _get_args;
    std::function<const mediakit::Parser &()> _get_parser;
    std::function<std::string(HttpAllArgs &that, const std::string &key)> _get_value;
    std::function<void(HttpAllArgs &that)> _clone;
};

#define API_ARGS_MAP                                                                                                   \
    toolkit::SockInfo &sender, mediakit::HttpSession::KeyValue &headerOut, const HttpAllArgs<ApiArgsType> &allArgs,    \
        Json::Value &val
#define API_ARGS_MAP_ASYNC API_ARGS_MAP, const mediakit::HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_JSON                                                                                                  \
    toolkit::SockInfo &sender, mediakit::HttpSession::KeyValue &headerOut, const HttpAllArgs<Json::Value> &allArgs,    \
        Json::Value &val
#define API_ARGS_JSON_ASYNC API_ARGS_JSON, const mediakit::HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_STRING                                                                                                \
    toolkit::SockInfo &sender, mediakit::HttpSession::KeyValue &headerOut, const HttpAllArgs<std::string> &allArgs,    \
        Json::Value &val
#define API_ARGS_STRING_ASYNC API_ARGS_STRING, const mediakit::HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_VALUE sender, headerOut, allArgs, val

//注册http请求参数是http原始请求信息的异步回复的http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_STRING_ASYNC)> &func);

template <typename Args, typename First>
bool checkArgs(Args &args, const First &first) {
    return !args[first].empty();
}

template <typename Args, typename First, typename... KeyTypes>
bool checkArgs(Args &args, const First &first, const KeyTypes &...keys) {
    return checkArgs(args, first) && checkArgs(args, keys...);
}

//检查http url中或body中或http header参数是否为空的宏
#define CHECK_ARGS(...)                                                                                                \
    if (!checkArgs(allArgs, ##__VA_ARGS__)) {                                                                          \
        throw InvalidArgsException("缺少必要参数:" #__VA_ARGS__);                                                      \
    }

#ifdef ENABLE_WEBRTC
class WebRtcArgsImp : public WebRtcArgs {
public:
    WebRtcArgsImp(const HttpAllArgs<std::string> &args, std::string session_id)
        : _args(args)
        , _session_id(std::move(session_id)) {}
    ~WebRtcArgsImp() override = default;

    variant operator[](const std::string &key) const override {
        if (key == "url") {
            return getUrl();
        }
        return _args[key];
    }

private:
    std::string getUrl() const {
        auto &allArgs = _args;
        CHECK_ARGS("app", "stream");

        return StrPrinter << RTC_SCHEMA << "://" << _args["Host"] << "/" << _args["app"] << "/" << _args["stream"]
                          << "?" << _args.getParser().Params() + "&session=" + _session_id;
    }

private:
    HttpAllArgs<std::string> _args;
    std::string _session_id;
};
#endif

#endif // MK_WEBRTC_API_H
#endif // ENABLE_WEBRTC