#include <sstream>
#include <unordered_map>
#include <mutex>
#include "System.h"
#include "jsoncpp/json.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Http/HttpRequester.h"
#include "Network/TcpSession.h"

using namespace Json;
using namespace toolkit;
using namespace mediakit;

namespace Hook {
#define HOOK_FIELD "hook."

const char kEnable[] = HOOK_FIELD"enable";
const char kTimeoutSec[] = HOOK_FIELD"timeoutSec";
const char kOnPublish[] = HOOK_FIELD"on_publish";
const char kOnPlay[] = HOOK_FIELD"on_play";
const char kOnFlowReport[] = HOOK_FIELD"on_flow_report";
const char kAdminParams[] = HOOK_FIELD"admin_params";

onceToken token([](){
    mINI::Instance()[kEnable] = false;
    mINI::Instance()[kTimeoutSec] = 10;
    mINI::Instance()[kOnPublish] = "http://127.0.0.1/index/hook/on_publish";
    mINI::Instance()[kOnPlay] = "http://127.0.0.1/index/hook/on_play";
    mINI::Instance()[kOnFlowReport] = "http://127.0.0.1/index/hook/on_flow_report";
    mINI::Instance()[kAdminParams] = "token=035c73f7-bb6b-4889-a715-d9eb2d1925cc";
},nullptr);
}//namespace Hook


static void parse_http_response(const SockException &ex,
                                const string &status,
                                const HttpClient::HttpHeader &header,
                                const string &strRecvBody,
                                const function<void(const Value &,const string &)> &fun){
    if(ex){
        auto errStr = StrPrinter << "[network err]:" << ex.what() << endl;
        fun(Json::nullValue,errStr);
        return;
    }
    if(status != "200"){
        auto errStr = StrPrinter << "[bad http status code]:" << status << endl;
        fun(Json::nullValue,errStr);
        return;
    }
    try {
        stringstream ss(strRecvBody);
        Value result;
        ss >> result;
        if(result["code"].asInt() != 0) {
            auto errStr = StrPrinter << "[json code]:" << "code=" << result["code"] << ",msg=" << result["msg"] << endl;
            fun(Json::nullValue,errStr);
            return;
        }
        fun(result,"");
    }catch (std::exception &ex){
        auto errStr = StrPrinter << "[parse json failed]:" << ex.what() << endl;
        fun(Json::nullValue,errStr);
    }
}

static void do_http_hook(const string &url,const Value &body,const function<void(const Value &,const string &)> &fun){
    GET_CONFIG_AND_REGISTER(float,hook_timeoutSec,Hook::kTimeoutSec);
    HttpRequester::Ptr requester(new HttpRequester);
    requester->setMethod("POST");
    requester->setBody(body.toStyledString());
    requester->addHeader("Content-Type","application/json; charset=utf-8");
    std::shared_ptr<Ticker> pTicker(new Ticker);
    requester->startRequester(url,[url,fun,body,requester,pTicker](const SockException &ex,
                                                    const string &status,
                                                    const HttpClient::HttpHeader &header,
                                                    const string &strRecvBody){
        onceToken token(nullptr,[&](){
            const_cast<HttpRequester::Ptr &>(requester).reset();
        });
        parse_http_response(ex,status,header,strRecvBody,[&](const Value &obj,const string &err){
            if(fun){
                fun(obj,err);
            }
            if(!err.empty()) {
                WarnL << "hook " << url << " " <<pTicker->elapsedTime() << "ms,failed" << err << ":" << body;
            }else if(pTicker->elapsedTime() > 500){
                DebugL << "hook " << url << " " <<pTicker->elapsedTime() << "ms,success:" << body;
            }
        });
    },hook_timeoutSec);
}

static Value make_json(const MediaInfo &args){
    Value body;
    body["schema"] = args._schema;
    body["vhost"] = args._vhost;
    body["app"] = args._app;
    body["stream"] = args._streamid;
    body["params"] = args._param_strs;
    return body;
}


void installWebHook(){
    GET_CONFIG_AND_REGISTER(bool,hook_enable,Hook::kEnable);
    GET_CONFIG_AND_REGISTER(string,hook_publish,Hook::kOnPublish);
    GET_CONFIG_AND_REGISTER(string,hook_play,Hook::kOnPlay);
    GET_CONFIG_AND_REGISTER(string,hook_flowreport,Hook::kOnFlowReport);
    GET_CONFIG_AND_REGISTER(string,hook_adminparams,Hook::kAdminParams);

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastRtmpPublish,[](BroadcastRtmpPublishArgs){
        if(!hook_enable || args._param_strs == hook_adminparams){
            invoker("");
            return;
        }
        //异步执行该hook api，防止阻塞NoticeCenter
        Value body = make_json(args);
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        EventPollerPool::Instance().getExecutor()->async([body,invoker](){
            //执行hook
            do_http_hook(hook_publish,body,[invoker](const Value &obj,const string &err){
                invoker(err);
            });
        });
    });

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastMediaPlayed,[](BroadcastMediaPlayedArgs){
        if(!hook_enable || args._param_strs == hook_adminparams){
            invoker("");
            return;
        }
        Value body = make_json(args);
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        //异步执行该hook api，防止阻塞NoticeCenter
        EventPollerPool::Instance().getExecutor()->async([body,invoker](){
            //执行hook
            do_http_hook(hook_play,body,[invoker](const Value &obj,const string &err){
                invoker(err);
            });
        });
    });

    NoticeCenter::Instance().addListener(nullptr,Broadcast::kBroadcastFlowReport,[](BroadcastFlowReportArgs){
        if(!hook_enable || args._param_strs == hook_adminparams){
            return;
        }
        Value body = make_json(args);
        body["ip"] = sender.get_peer_ip();
        body["port"] = sender.get_peer_port();
        body["id"] = sender.getIdentifier();
        body["totalBytes"] = (Json::UInt64)totalBytes;
        body["duration"] = (Json::UInt64)totalDuration;

        //流量统计事件
        EventPollerPool::Instance().getExecutor()->async([body,totalBytes](){
            //执行hook
            do_http_hook(hook_flowreport,body, nullptr);
        });
    });
}