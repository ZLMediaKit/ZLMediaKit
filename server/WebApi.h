/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBAPI_H
#define ZLMEDIAKIT_WEBAPI_H

#include <string>
#include <functional>
#include "json/json.h"
#include "Common/Parser.h"
#include "Network/Socket.h"
#include "Http/HttpSession.h"
#include "Common/MultiMediaSourceMuxer.h"

#if defined(ENABLE_WEBRTC)
#include "webrtc/WebRtcTransport.h"
#endif

// 配置文件路径  [AUTO-TRANSLATED:8a373c2f]
// Configuration file path
extern std::string g_ini_file;

namespace mediakit {
// //////////RTSP服务器配置///////////  [AUTO-TRANSLATED:950e1981]
// //////////RTSP server configuration///////////
namespace Rtsp {
extern const std::string kPort;
} //namespace Rtsp

// //////////RTMP服务器配置///////////  [AUTO-TRANSLATED:8de6f41f]
// //////////RTMP server configuration///////////
namespace Rtmp {
extern const std::string kPort;
} //namespace RTMP
}  // namespace mediakit

namespace API {
typedef enum {
    NotFound = -500,//未找到
    Exception = -400,//代码抛异常
    InvalidArgs = -300,//参数不合法
    SqlFailed = -200,//sql执行失败
    AuthFailed = -100,//鉴权失败
    OtherFailed = -1,//业务代码执行失败，
    Success = 0//执行成功
} ApiErr;

extern const std::string kSecret;
}//namespace API

class ApiRetException: public std::runtime_error {
public:
    ApiRetException(const char *str = "success" ,int code = API::Success):runtime_error(str){
        _code = code;
    }
    int code(){ return _code; }
private:
    int _code;
};

class AuthException : public ApiRetException {
public:
    AuthException(const char *str):ApiRetException(str,API::AuthFailed){}
};

class InvalidArgsException: public ApiRetException {
public:
    InvalidArgsException(const char *str):ApiRetException(str,API::InvalidArgs){}
};

class SuccessException: public ApiRetException {
public:
    SuccessException():ApiRetException("success",API::Success){}
};

using ApiArgsType = std::map<std::string, std::string, mediakit::StrCaseCompare>;

template<typename Args, typename Key>
std::string getValue(Args &args, const Key &key) {
    auto it = args.find(key);
    if (it == args.end()) {
        return "";
    }
    return it->second;
}

template<typename Key>
std::string getValue(Json::Value &args, const Key &key) {
    auto value = args.find(key);
    if (value == nullptr) {
        return "";
    }
    return value->asString();
}

template<typename Key>
std::string getValue(std::string &args, const Key &key) {
    return "";
}

template <typename Key>
std::string getValue(const mediakit::Parser &parser, const Key &key) {
    auto ret = getValue(parser.getUrlArgs(), key);
    if (!ret.empty()) {
        return ret;
    }
    return getValue(parser.getHeader(), key);
}

template<typename Key>
std::string getValue(mediakit::Parser &parser, const Key &key) {
    return getValue((const mediakit::Parser &) parser, key);
}

template<typename Args, typename Key>
std::string getValue(const mediakit::Parser &parser, Args &args, const Key &key) {
    auto ret = getValue(args, key);
    if (!ret.empty()) {
        return ret;
    }
    return getValue(parser, key);
}

template<typename Args>
class HttpAllArgs {
    mediakit::Parser* _parser = nullptr;
    Args* _args = nullptr;
public:
    const mediakit::Parser& parser;
    Args& args;

    HttpAllArgs(const mediakit::Parser &p, Args &a): parser(p), args(a) {}

    HttpAllArgs(const HttpAllArgs &that): _parser(new mediakit::Parser(that.parser)),
                                          _args(new Args(that.args)),
                                          parser(*_parser), args(*_args) {}
    ~HttpAllArgs() {
        if (_parser) {
            delete _parser;
        }
        if (_args) {
            delete _args;
        }
    }

    template<typename Key>
    toolkit::variant operator[](const Key &key) const {
        return (toolkit::variant)getValue(parser, args, key);
    }

    const Args& getArgs() const {
        return args;
    }

    const mediakit::Parser &getParser() const {
        return parser;
    }
};

using ArgsMap = HttpAllArgs<ApiArgsType>;
using ArgsJson = HttpAllArgs<Json::Value>;
using ArgsString = HttpAllArgs<std::string>;

#define API_ARGS_MAP toolkit::SockInfo &sender, mediakit::HttpSession::KeyValue &headerOut, const ArgsMap &allArgs, Json::Value &val
#define API_ARGS_MAP_ASYNC API_ARGS_MAP, const mediakit::HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_JSON toolkit::SockInfo &sender, mediakit::HttpSession::KeyValue &headerOut, const ArgsJson &allArgs, Json::Value &val
#define API_ARGS_JSON_ASYNC API_ARGS_JSON, const mediakit::HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_STRING toolkit::SockInfo &sender, mediakit::HttpSession::KeyValue &headerOut, const ArgsString &allArgs, Json::Value &val
#define API_ARGS_STRING_ASYNC API_ARGS_STRING, const mediakit::HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_VALUE sender, headerOut, allArgs, val

// 注册http请求参数是map<string, variant, StrCaseCompare>类型的http api  [AUTO-TRANSLATED:8a273897]
// Register http request parameters as map<string, variant, StrCaseCompare> type http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_MAP)> &func);
// 注册http请求参数是map<string, variant, StrCaseCompare>类型,但是可以异步回复的的http api  [AUTO-TRANSLATED:9da5d5f5]
// Register http request parameters as map<string, variant, StrCaseCompare> type, but can be replied asynchronously http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_MAP_ASYNC)> &func);

// 注册http请求参数是Json::Value类型的http api(可以支持多级嵌套的json参数对象)  [AUTO-TRANSLATED:c4794456]
// Register http request parameters as Json::Value type http api (can support multi-level nested json parameter objects)
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_JSON)> &func);
// 注册http请求参数是Json::Value类型，但是可以异步回复的的http api  [AUTO-TRANSLATED:742e57fd]
// Register http request parameters as Json::Value type, but can be replied asynchronously http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_JSON_ASYNC)> &func);

// 注册http请求参数是http原始请求信息的http api  [AUTO-TRANSLATED:72d3fe93]
// Register http request parameters as http original request information http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_STRING)> &func);
// 注册http请求参数是http原始请求信息的异步回复的http api  [AUTO-TRANSLATED:49feefa8]
// Register http request parameters as http original request information asynchronous reply http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_STRING_ASYNC)> &func);

template<typename Args, typename Key>
bool checkArgs(Args &args, const Key &key) {
    return !args[key].empty();
}

template<typename Args, typename Key, typename ...KeyTypes>
bool checkArgs(Args &args, const Key &key, const KeyTypes &...keys) {
    return checkArgs(args, key) && checkArgs(args, keys...);
}

// 检查http url中或body中或http header参数是否为空的宏  [AUTO-TRANSLATED:9de001a4]
// Check whether the http url, body or http header parameters are empty
#define CHECK_ARGS(...)  \
    if(!checkArgs(allArgs,##__VA_ARGS__)){ \
        throw InvalidArgsException("Required parameter missed: " #__VA_ARGS__); \
    }

// 检查http参数中是否附带secret密钥的宏，127.0.0.1的ip不检查密钥  [AUTO-TRANSLATED:7546956c]
// Check whether the http parameters contain the secret key, the ip of 127.0.0.1 does not check the key
// 同时检测是否在ip白名单内  [AUTO-TRANSLATED:d12f963d]
// Check whether it is in the ip whitelist at the same time
#define CHECK_SECRET() \
    do { \
        auto ip = sender.get_peer_ip(); \
        if (!HttpFileManager::isIPAllowed(ip)) { \
            throw AuthException("Your ip is not allowed to access the service."); \
        } \
        CHECK_ARGS("secret"); \
        if (api_secret != allArgs["secret"]) { \
            throw AuthException("Incorrect secret"); \
        } \
    } while(false);

void installWebApi();
void unInstallWebApi();

#if defined(ENABLE_RTPPROXY)
uint16_t openRtpServer(uint16_t local_port, const mediakit::MediaTuple &tuple, int tcp_mode, const std::string &local_ip, bool re_use_port, uint32_t ssrc, int only_track, bool multiplex=false);
#endif

Json::Value makeMediaSourceJson(mediakit::MediaSource &media);
void getStatisticJson(const std::function<void(Json::Value &val)> &cb);
void addStreamProxy(const mediakit::MediaTuple &tuple, const std::string &url, int retry_count,
                    const mediakit::ProtocolOption &option, int rtp_type, float timeout_sec, const toolkit::mINI &args,
                    const std::function<void(const toolkit::SockException &ex, const std::string &key)> &cb);

template <typename Type>
class ServiceController {
public:
    using Pointer = std::shared_ptr<Type>;
    std::unordered_map<std::string, Pointer> _map;
    mutable std::recursive_mutex _mtx;

    void clear() {
        decltype(_map) copy;
        {
            std::lock_guard<std::recursive_mutex> lck(_mtx);
            copy.swap(_map);
        }
    }

    size_t erase(const std::string &key) {
        Pointer erase_ptr;
        {
            std::lock_guard<std::recursive_mutex> lck(_mtx);
            auto itr = _map.find(key);
            if (itr != _map.end()) {
                erase_ptr = std::move(itr->second);
                _map.erase(itr);
                return 1;
            }
        }
        return 0;
    }

    size_t size() { 
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        return _map.size();
    }

    Pointer find(const std::string &key) const {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto it = _map.find(key);
        if (it == _map.end()) {
            return nullptr;
        }
        return it->second;
    }

    void for_each(const std::function<void(const std::string&, const Pointer&)>& cb) {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto it = _map.begin();
        while (it != _map.end()) {
            cb(it->first, it->second);
            it++;
        }
    }

    template<class ..._Args>
    Pointer make(const std::string &key, _Args&& ...__args) {
        // assert(!find(key));

        auto server = std::make_shared<Type>(std::forward<_Args>(__args)...);
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto it = _map.emplace(key, server);
        assert(it.second);
        return server;
    }

    template<class ..._Args>
    Pointer makeWithAction(const std::string &key, std::function<void(Pointer)> action, _Args&& ...__args) {
        // assert(!find(key));

        auto server = std::make_shared<Type>(std::forward<_Args>(__args)...);
        action(server);
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto it = _map.emplace(key, server);
        assert(it.second);
        return server;
    }

    template<class ..._Args>
    Pointer emplace(const std::string &key, _Args&& ...__args) {
        // assert(!find(key));

        auto server = std::static_pointer_cast<Type>(std::forward<_Args>(__args)...);
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto it = _map.emplace(key, server);
        assert(it.second);
        return server;
    }
};

#if defined(ENABLE_WEBRTC)
template <typename Args>
class WebRtcArgsImp : public mediakit::WebRtcArgs {
public:
    WebRtcArgsImp(const HttpAllArgs<Args> &args, std::string session_id)
        : _args(args)
        , _session_id(std::move(session_id)) {}
    ~WebRtcArgsImp() override = default;

    toolkit::variant operator[](const std::string &key) const override {
        if (key == "url") {
            return getUrl();
        }
        return _args[key];
    }

private:
    std::string getUrl() const {
        auto &allArgs = _args;
        CHECK_ARGS("app", "stream");

        return StrPrinter << RTC_SCHEMA << "://" << (_args["Host"].empty() ? DEFAULT_VHOST : _args["Host"].data()) << "/" << _args["app"] << "/"
                          << _args["stream"] << "?" << _args.getParser().params() + "&session=" + _session_id;
    }

private:
    HttpAllArgs<Args> _args;
    std::string _session_id;
};
#endif

#endif //ZLMEDIAKIT_WEBAPI_H
