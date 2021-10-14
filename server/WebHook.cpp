/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Http/HttpRequester.h"
#include "Network/TcpSession.h"
#include "Rtsp/RtspSession.h"
#include "Http/HttpSession.h"
#include "WebHook.h"
#include "WebApi.h"

using namespace toolkit;
using namespace mediakit;

namespace Hook {
#define HOOK_FIELD "hook."

const string kEnable = HOOK_FIELD"enable";
const string kTimeoutSec = HOOK_FIELD"timeoutSec";
const string kOnPublish = HOOK_FIELD"on_publish";
const string kOnPlay = HOOK_FIELD"on_play";
const string kOnFlowReport = HOOK_FIELD"on_flow_report";
const string kOnRtspRealm = HOOK_FIELD"on_rtsp_realm";
const string kOnRtspAuth = HOOK_FIELD"on_rtsp_auth";
const string kOnStreamChanged = HOOK_FIELD"on_stream_changed";
const string kOnStreamNotFound = HOOK_FIELD"on_stream_not_found";
const string kOnRecordMp4 = HOOK_FIELD"on_record_mp4";
const string kOnRecordTs = HOOK_FIELD"on_record_ts";
const string kOnShellLogin = HOOK_FIELD"on_shell_login";
const string kOnStreamNoneReader = HOOK_FIELD"on_stream_none_reader";
const string kOnHttpAccess = HOOK_FIELD"on_http_access";
const string kOnServerStarted = HOOK_FIELD"on_server_started";
const string kOnServerKeepalive = HOOK_FIELD"on_server_keepalive";
const string kAdminParams = HOOK_FIELD"admin_params";
const string kAliveInterval = HOOK_FIELD"alive_interval";

onceToken token([](){
    mINI::Instance()[kEnable] = false;
    mINI::Instance()[kTimeoutSec] = 10;
    //默认hook地址设置为空，采用默认行为(例如不鉴权)
    mINI::Instance()[kOnPublish] = "";
    mINI::Instance()[kOnPlay] = "";
    mINI::Instance()[kOnFlowReport] = "";
    mINI::Instance()[kOnRtspRealm] = "";
    mINI::Instance()[kOnRtspAuth] = "";
    mINI::Instance()[kOnStreamChanged] = "";
    mINI::Instance()[kOnStreamNotFound] = "";
    mINI::Instance()[kOnRecordMp4] = "";
    mINI::Instance()[kOnRecordTs] = "";
    mINI::Instance()[kOnShellLogin] = "";
    mINI::Instance()[kOnStreamNoneReader] = "";
    mINI::Instance()[kOnHttpAccess] = "";
    mINI::Instance()[kOnServerStarted] = "";
    mINI::Instance()[kOnServerKeepalive] = "";
    mINI::Instance()[kAdminParams] = "secret=035c73f7-bb6b-4889-a715-d9eb2d1925cc";
    mINI::Instance()[kAliveInterval] = 30.0;
},nullptr);
}//namespace Hook


static void parse_http_response(const SockException &ex,
                                const Parser &res,
                                const function<void(const Value &,const string &)> &fun){
    if (ex) {
        auto errStr = StrPrinter << "[network err]:" << ex.what() << endl;
        fun(Json::nullValue, errStr);
        return;
    }
    if (res.Url() != "200") {
        auto errStr = StrPrinter << "[bad http status code]:" << res.Url() << endl;
        fun(Json::nullValue, errStr);
        return;
    }
    Value result;
    try {
        stringstream ss(res.Content());
        ss >> result;
    } catch (std::exception &ex) {
        auto errStr = StrPrinter << "[parse json failed]:" << ex.what() << endl;
        fun(Json::nullValue, errStr);
        return;
    }
    if (result["code"].asInt() != 0) {
        auto errStr = StrPrinter << "[json code]:" << "code=" << result["code"] << ",msg=" << result["msg"] << endl;
        fun(Json::nullValue, errStr);
        return;
    }
    try {
        fun(result, "");
    } catch (std::exception &ex) {
        auto errStr = StrPrinter << "[do hook invoker failed]:" << ex.what() << endl;
        //如果还是抛异常，那么再上抛异常
        fun(Json::nullValue, errStr);
    }
}

string to_string(const Value &value){
    return value.toStyledString();
}

string to_string(const HttpArgs &value){
    return value.make();
}

const char *getContentType(const Value &value){
    return "application/json";
}

const char *getContentType(const HttpArgs &value){
    return "application/x-www-form-urlencoded";
}

string getVhost(const Value &value) {
    const char *key = VHOST_KEY;
    auto val = value.find(key, key + sizeof(VHOST_KEY) - 1);
    return val ? val->asString() : "";
}

string getVhost(const HttpArgs &value) {
    auto val = value.find(VHOST_KEY);
    return val != value.end() ? val->second : "";
}

void do_http_hook(const string &url,const ArgsType &body,const function<void(const Value &,const string &)> &func){
    GET_CONFIG(string, mediaServerId, General::kMediaServerId);
    GET_CONFIG(float, hook_timeoutSec, Hook::kTimeoutSec);

    const_cast<ArgsType &>(body)["mediaServerId"] = mediaServerId;
    HttpRequester::Ptr requester(new HttpRequester);
    requester->setMethod("POST");
    auto bodyStr = to_string(body);
    requester->setBody(bodyStr);
    requester->addHeader("Content-Type", getContentType(body));
    auto vhost = getVhost(body);
    if (!vhost.empty()) {
        requester->addHeader("X-VHOST", vhost);
    }
    std::shared_ptr<Ticker> pTicker(new Ticker);
    requester->startRequester(url, [url, func, bodyStr, requester, pTicker](const SockException &ex,
                                                                            const Parser &res) mutable{
        onceToken token(nullptr, [&]() mutable{
            requester.reset();
        });
        parse_http_response(ex, res, [&](const Value &obj, const string &err) {
            if (func) {
                func(obj, err);
            }
            if (!err.empty()) {
                WarnL << "hook " << url << " " << pTicker->elapsedTime() << "ms,failed" << err << ":" << bodyStr;
            } else if (pTicker->elapsedTime() > 500) {
                DebugL << "hook " << url << " " << pTicker->elapsedTime() << "ms,success:" << bodyStr;
            }
        });
    }, hook_timeoutSec);
}

static ArgsType make_json(const MediaInfo &args){
    ArgsType body;
    body["schema"] = args._schema;
    body[VHOST_KEY] = args._vhost;
    body["app"] = args._app;
    body["stream"] = args._streamid;
    body["params"] = args._param_strs;
    return body;
}

static void reportServerStarted(){
    GET_CONFIG(bool,hook_enable,Hook::kEnable);
    GET_CONFIG(string,hook_server_started,Hook::kOnServerStarted);
    if(!hook_enable || hook_server_started.empty()){
        return;
    }

    ArgsType body;
    for (auto &pr : mINI::Instance()) {
        body[pr.first] = (string &) pr.second;
    }
    //执行hook
    do_http_hook(hook_server_started,body, nullptr);
}

// 服务器定时保活定时器
static Timer::Ptr g_keepalive_timer;
static void reportServerKeepalive() {
    GET_CONFIG(bool, hook_enable, Hook::kEnable);
    GET_CONFIG(string, hook_server_keepalive, Hook::kOnServerKeepalive);
    if (!hook_enable || hook_server_keepalive.empty()) {
        return;
    }

    GET_CONFIG(float, alive_interval, Hook::kAliveInterval);
    g_keepalive_timer = std::make_shared<Timer>(alive_interval, []() {
        ArgsType body;
        body["data"] = getStatisticJson();

        //执行hook
        do_http_hook(hook_server_keepalive, body, nullptr);

        return true;
    }, nullptr);
}

void installWebHook(){
    GET_CONFIG(bool,hook_enable,Hook::kEnable);
    GET_CONFIG(string,hook_adminparams,Hook::kAdminParams);

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastMediaPublish,[](BroadcastMediaPublishArgs){
        GET_CONFIG(string,hook_publish,Hook::kOnPublish);
        GET_CONFIG(bool,toHls,General::kPublishToHls);
        GET_CONFIG(bool,toMP4,General::kPublishToMP4);
        if(!hook_enable || args._param_strs == hook_adminparams || hook_publish.empty() || sender.get_peer_ip() == "127.0.0.1"){
            invoker("", toHls, toMP4);
            return;
        }
        //异步执行该hook api，防止阻塞NoticeCenter
        auto body = make_json(args);
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        //执行hook
        do_http_hook(hook_publish,body,[invoker](const Value &obj,const string &err){
            if(err.empty()){
                //推流鉴权成功
                bool enableHls = toHls;
                bool enableMP4 = toMP4;

                //兼容用户不传递enableHls、enableMP4参数
                if (obj.isMember("enableHls")) {
                    enableHls = obj["enableHls"].asBool();
                }
                if (obj.isMember("enableMP4")) {
                    enableMP4 = obj["enableMP4"].asBool();
                }
                invoker(err, enableHls, enableMP4);
            } else {
                //推流鉴权失败
                invoker(err, false, false);
            }

        });
    });

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastMediaPlayed,[](BroadcastMediaPlayedArgs){
        GET_CONFIG(string,hook_play,Hook::kOnPlay);
        if(!hook_enable || args._param_strs == hook_adminparams || hook_play.empty() || sender.get_peer_ip() == "127.0.0.1"){
            invoker("");
            return;
        }
        auto body = make_json(args);
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        //执行hook
        do_http_hook(hook_play,body,[invoker](const Value &obj,const string &err){
            invoker(err);
        });
    });

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastFlowReport,[](BroadcastFlowReportArgs){
        GET_CONFIG(string,hook_flowreport,Hook::kOnFlowReport);
        if(!hook_enable || args._param_strs == hook_adminparams || hook_flowreport.empty() || sender.get_peer_ip() == "127.0.0.1"){
            return;
        }
        auto body = make_json(args);
        body["totalBytes"] = (Json::UInt64)totalBytes;
        body["duration"] = (Json::UInt64)totalDuration;
        body["player"] = isPlayer;
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        //执行hook
        do_http_hook(hook_flowreport,body, nullptr);
    });


    static const string unAuthedRealm = "unAuthedRealm";

    //监听kBroadcastOnGetRtspRealm事件决定rtsp链接是否需要鉴权(传统的rtsp鉴权方案)才能访问
    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastOnGetRtspRealm,[](BroadcastOnGetRtspRealmArgs){
        GET_CONFIG(string,hook_rtsp_realm,Hook::kOnRtspRealm);
        if(!hook_enable || args._param_strs == hook_adminparams || hook_rtsp_realm.empty() || sender.get_peer_ip() == "127.0.0.1"){
            //无需认证
            invoker("");
            return;
        }
        auto body = make_json(args);
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        //执行hook
        do_http_hook(hook_rtsp_realm,body, [invoker](const Value &obj,const string &err){
            if(!err.empty()){
                //如果接口访问失败，那么该rtsp流认证失败
                invoker(unAuthedRealm);
                return;
            }
            invoker(obj["realm"].asString());
        });
    });

    //监听kBroadcastOnRtspAuth事件返回正确的rtsp鉴权用户密码
    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastOnRtspAuth,[](BroadcastOnRtspAuthArgs){
        GET_CONFIG(string,hook_rtsp_auth,Hook::kOnRtspAuth);
        if(unAuthedRealm == realm || !hook_enable || hook_rtsp_auth.empty()){
            //认证失败
            invoker(false,makeRandStr(12));
            return;
        }
        auto body = make_json(args);
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        body["user_name"] = user_name;
        body["must_no_encrypt"] = must_no_encrypt;
        body["realm"] = realm;
        //执行hook
        do_http_hook(hook_rtsp_auth,body, [invoker](const Value &obj,const string &err){
            if(!err.empty()){
                //认证失败
                invoker(false,makeRandStr(12));
                return;
            }
            invoker(obj["encrypted"].asBool(),obj["passwd"].asString());
        });
    });


    //监听rtsp、rtmp源注册或注销事件
    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastMediaChanged,[](BroadcastMediaChangedArgs){
        GET_CONFIG(string,hook_stream_chaned,Hook::kOnStreamChanged);
        if(!hook_enable || hook_stream_chaned.empty()){
            return;
        }
        ArgsType body;
        if (bRegist) {
            body = makeMediaSourceJson(sender);
            body["regist"] = bRegist;
        } else {
            body["schema"] = sender.getSchema();
            body[VHOST_KEY] = sender.getVhost();
            body["app"] = sender.getApp();
            body["stream"] = sender.getId();
            body["regist"] = bRegist;
        }
        //执行hook
        do_http_hook(hook_stream_chaned,body, nullptr);
    });

    //监听播放失败(未找到特定的流)事件
    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastNotFoundStream,[](BroadcastNotFoundStreamArgs){
        GET_CONFIG(string,hook_stream_not_found,Hook::kOnStreamNotFound);
        if(!hook_enable || hook_stream_not_found.empty()){
            //如果确定这个流不存在，可以closePlayer()返回播放器流不存在
            //closePlayer();
            return;
        }
        auto body = make_json(args);
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        //执行hook
        do_http_hook(hook_stream_not_found,body, nullptr);
    });

    static auto getRecordInfo = [](const RecordInfo &info) {
        ArgsType body;
        body["start_time"] = (Json::UInt64) info.start_time;
        body["file_size"] = (Json::UInt64) info.file_size;
        body["time_len"] = info.time_len;
        body["file_path"] = info.file_path;
        body["file_name"] = info.file_name;
        body["folder"] = info.folder;
        body["url"] = info.url;
        body["app"] = info.app;
        body["stream"] = info.stream;
        body[VHOST_KEY] = info.vhost;
        return body;
    };

#ifdef ENABLE_MP4
    //录制mp4文件成功后广播
    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastRecordMP4,[](BroadcastRecordMP4Args){
        GET_CONFIG(string,hook_record_mp4,Hook::kOnRecordMp4);
        if (!hook_enable || hook_record_mp4.empty()) {
            return;
        }
        //执行hook
        do_http_hook(hook_record_mp4, getRecordInfo(info), nullptr);
    });
#endif //ENABLE_MP4

    NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastRecordTs, [](BroadcastRecordTsArgs) {
        GET_CONFIG(string,hook_record_ts,Hook::kOnRecordTs);
        if (!hook_enable || hook_record_ts.empty()) {
            return;
        }
        // 执行 hook
        do_http_hook(hook_record_ts, getRecordInfo(info), nullptr);
    });

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastShellLogin,[](BroadcastShellLoginArgs){
        GET_CONFIG(string,hook_shell_login,Hook::kOnShellLogin);
        if(!hook_enable || hook_shell_login.empty() || sender.get_peer_ip() == "127.0.0.1"){
            invoker("");
            return;
        }
        ArgsType body;
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        body["user_name"] = user_name;
        body["passwd"] = passwd;

        //执行hook
        do_http_hook(hook_shell_login,body, [invoker](const Value &,const string &err){
            invoker(err);
        });
    });

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastStreamNoneReader,[](BroadcastStreamNoneReaderArgs){
        GET_CONFIG(string,hook_stream_none_reader,Hook::kOnStreamNoneReader);
        if(!hook_enable || hook_stream_none_reader.empty()){
            return;
        }

        ArgsType body;
        body["schema"] = sender.getSchema();
        body[VHOST_KEY] = sender.getVhost();
        body["app"] = sender.getApp();
        body["stream"] = sender.getId();
        weak_ptr<MediaSource> weakSrc = sender.shared_from_this();
        //执行hook
        do_http_hook(hook_stream_none_reader,body, [weakSrc](const Value &obj,const string &err){
            bool flag = obj["close"].asBool();
            auto strongSrc = weakSrc.lock();
            if(!flag || !err.empty() || !strongSrc){
                return;
            }
            strongSrc->close(false);
            WarnL << "无人观看主动关闭流:"
                  << strongSrc->getSchema() << "/"
                  << strongSrc->getVhost() << "/"
                  << strongSrc->getApp() << "/"
                  << strongSrc->getId();
        });
    });

    /**
     * kBroadcastHttpAccess事件触发机制
     * 1、根据http请求头查找cookie，找到进入步骤3
     * 2、根据http url参数查找cookie，如果还是未找到cookie则进入步骤5
     * 3、cookie标记是否有权限访问文件，如果有权限，直接返回文件
     * 4、cookie中记录的url参数是否跟本次url参数一致，如果一致直接返回客户端错误码
     * 5、触发kBroadcastHttpAccess事件
     */
    //开发者应该通过该事件判定http客户端是否有权限访问http服务器上的特定文件
    //ZLMediaKit会记录本次鉴权的结果至cookie
    //如果鉴权成功，在cookie有效期内，那么下次客户端再访问授权目录时，ZLMediaKit会直接返回文件
    //如果鉴权失败，在cookie有效期内，如果http url参数不变(否则会立即再次触发鉴权事件)，ZLMediaKit会直接返回错误码
    //如果用户客户端不支持cookie，那么ZLMediaKit会根据url参数查找cookie并追踪用户，
    //如果没有url参数，客户端又不支持cookie，那么会根据ip和端口追踪用户
    //追踪用户的目的是为了缓存上次鉴权结果，减少鉴权次数，提高性能
    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastHttpAccess,[](BroadcastHttpAccessArgs){
        GET_CONFIG(string,hook_http_access,Hook::kOnHttpAccess);
        if(sender.get_peer_ip() == "127.0.0.1" || parser.Params() == hook_adminparams){
            //如果是本机或超级管理员访问，那么不做访问鉴权；权限有效期1个小时
            invoker("","",60 * 60);
            return;
        }
        if(!hook_enable || hook_http_access.empty()){
            //未开启http文件访问鉴权，那么允许访问，但是每次访问都要鉴权；
            //因为后续随时都可能开启鉴权(重载配置文件后可能重新开启鉴权)
            invoker("","",0);
            return;
        }

        ArgsType body;
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        body["path"] = path;
        body["is_dir"] = is_dir;
        body["params"] = parser.Params();
        for(auto &pr : parser.getHeader()){
            body[string("header.") + pr.first] = pr.second;
        }
        //执行hook
        do_http_hook(hook_http_access,body, [invoker](const Value &obj,const string &err){
            if(!err.empty()){
                //如果接口访问失败，那么仅限本次没有访问http服务器的权限
                invoker(err,"",0);
                return;
            }
            //err参数代表不能访问的原因，空则代表可以访问
            //path参数是该客户端能访问或被禁止的顶端目录，如果path为空字符串，则表述为当前目录
            //second参数规定该cookie超时时间，如果second为0，本次鉴权结果不缓存
            invoker(obj["err"].asString(),obj["path"].asString(),obj["second"].asInt());
        });
    });

    //汇报服务器重新启动
    reportServerStarted();

    //定时上报保活
    reportServerKeepalive();
}

void unInstallWebHook(){
    g_keepalive_timer.reset();
}
