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

//配置文件路径
extern std::string g_ini_file;

namespace mediakit {
////////////RTSP服务器配置///////////
namespace Rtsp {
extern const std::string kPort;
} //namespace Rtsp

////////////RTMP服务器配置///////////
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

template<typename Args, typename First>
std::string getValue(Args &args, const First &first) {
    return args[first];
}

template<typename First>
std::string getValue(Json::Value &args, const First &first) {
    return args[first].asString();
}

template<typename First>
std::string getValue(std::string &args, const First &first) {
    return "";
}

template<typename First>
std::string getValue(const mediakit::Parser &parser, const First &first) {
    auto ret = parser.getUrlArgs()[first];
    if (!ret.empty()) {
        return ret;
    }
    return parser.getHeader()[first];
}

template<typename First>
std::string getValue(mediakit::Parser &parser, const First &first) {
    return getValue((const mediakit::Parser &) parser, first);
}

template<typename Args, typename First>
std::string getValue(const mediakit::Parser &parser, Args &args, const First &first) {
    auto ret = getValue(args, first);
    if (!ret.empty()) {
        return ret;
    }
    return getValue(parser, first);
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

//注册http请求参数是map<string, variant, StrCaseCompare>类型的http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_MAP)> &func);
//注册http请求参数是map<string, variant, StrCaseCompare>类型,但是可以异步回复的的http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_MAP_ASYNC)> &func);

//注册http请求参数是Json::Value类型的http api(可以支持多级嵌套的json参数对象)
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_JSON)> &func);
//注册http请求参数是Json::Value类型，但是可以异步回复的的http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_JSON_ASYNC)> &func);

//注册http请求参数是http原始请求信息的http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_STRING)> &func);
//注册http请求参数是http原始请求信息的异步回复的http api
void api_regist(const std::string &api_path, const std::function<void(API_ARGS_STRING_ASYNC)> &func);

template<typename Args, typename First>
bool checkArgs(Args &args, const First &first) {
    return !args[first].empty();
}

template<typename Args, typename First, typename ...KeyTypes>
bool checkArgs(Args &args, const First &first, const KeyTypes &...keys) {
    return checkArgs(args, first) && checkArgs(args, keys...);
}

//检查http url中或body中或http header参数是否为空的宏
#define CHECK_ARGS(...)  \
    if(!checkArgs(allArgs,##__VA_ARGS__)){ \
        throw InvalidArgsException("Required parameter missed: " #__VA_ARGS__); \
    }

// 检查http参数中是否附带secret密钥的宏，127.0.0.1的ip不检查密钥
// 同时检测是否在ip白名单内
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
uint16_t openRtpServer(uint16_t local_port, const std::string &stream_id, int tcp_mode, const std::string &local_ip, bool re_use_port, uint32_t ssrc, int only_track, bool multiplex=false);
#endif

Json::Value makeMediaSourceJson(mediakit::MediaSource &media);
void getStatisticJson(const std::function<void(Json::Value &val)> &cb);
void addStreamProxy(const std::string &vhost, const std::string &app, const std::string &stream, const std::string &url, int retry_count,
                    const mediakit::ProtocolOption &option, int rtp_type, float timeout_sec, const toolkit::mINI &args,
                    const std::function<void(const toolkit::SockException &ex, const std::string &key)> &cb);
#endif //ZLMEDIAKIT_WEBAPI_H
