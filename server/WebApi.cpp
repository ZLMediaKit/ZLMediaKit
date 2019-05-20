#include <signal.h>
#include <functional>
#include <sstream>
#include <unordered_map>
#include "jsoncpp/json.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "Util/SqlPool.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Http/HttpRequester.h"
#include "Http/HttpSession.h"
#include "Network/TcpServer.h"
#include "Player/PlayerProxy.h"

using namespace Json;
using namespace toolkit;
using namespace mediakit;


typedef map<string,variant,StrCaseCompare> ApiArgsType;


#define API_ARGS HttpSession::KeyValue &headerIn, \
                 HttpSession::KeyValue &headerOut, \
                 ApiArgsType &allArgs, \
                 Json::Value &val

#define API_REGIST(field, name, ...) \
    s_map_api.emplace("/index/"#field"/"#name,[](API_ARGS,const HttpSession::HttpResponseInvoker &invoker){ \
         static auto lam = [&](API_ARGS) __VA_ARGS__ ;  \
         lam(headerIn, headerOut, allArgs, val); \
         invoker("200 OK", headerOut, val.toStyledString()); \
     });

#define API_REGIST_INVOKER(field, name, ...) \
    s_map_api.emplace("/index/"#field"/"#name,[](API_ARGS,const HttpSession::HttpResponseInvoker &invoker) __VA_ARGS__);

//异步http api lambad定义
typedef std::function<void(API_ARGS,const HttpSession::HttpResponseInvoker &invoker)> AsyncHttpApi;
//api列表
static map<string, AsyncHttpApi> s_map_api;

namespace API {
typedef enum {
    SqlFailed = -200,
    AuthFailed = -100,
    OtherFailed = -1,
    Success = 0
} ApiErr;

#define API_FIELD "api."
const char kApiDebug[] = API_FIELD"apiDebug";
static onceToken token([]() {
    mINI::Instance()[kApiDebug] = "1";
});
}//namespace API

class ApiRetException: public std::runtime_error {
public:
    ApiRetException(const char *str = "success" ,int code = API::Success):runtime_error(str){
        _code = code;
    }
    ~ApiRetException() = default;
    int code(){ return _code; }
private:
    int _code;
};

class AuthException : public ApiRetException {
public:
    AuthException(const char *str):ApiRetException(str,API::AuthFailed){}
    ~AuthException() = default;
};


//获取HTTP请求中url参数、content参数
static ApiArgsType getAllArgs(const Parser &parser) {
    ApiArgsType allArgs;
    if(parser["Content-Type"].find("application/x-www-form-urlencoded") == 0){
        auto contentArgs = parser.parseArgs(parser.Content());
        for (auto &pr : contentArgs) {
            allArgs[pr.first] = HttpSession::urlDecode(pr.second);
        }
    }else if(parser["Content-Type"].find("application/json") == 0){
        try {
            stringstream ss(parser.Content());
            Value jsonArgs;
            ss >> jsonArgs;
            auto keys = jsonArgs.getMemberNames();
            for (auto key = keys.begin(); key != keys.end(); ++key){
                allArgs[*key] = jsonArgs[*key].asString();
            }
        }catch (std::exception &ex){
            WarnL << ex.what();
        }
    }else if(!parser["Content-Type"].empty()){
        WarnL << "invalid Content-Type:" << parser["Content-Type"];
    }

    auto &urlArgs = parser.getUrlArgs();
    for (auto &pr : urlArgs) {
        allArgs[pr.first] = HttpSession::urlDecode(pr.second);
    }
    return std::move(allArgs);
}

static inline void addHttpListener(){
    GET_CONFIG_AND_REGISTER(bool, api_debug, API::kApiDebug);
    //注册监听kBroadcastHttpRequest事件
    NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastHttpRequest, [](BroadcastHttpRequestArgs) {
        auto it = s_map_api.find(parser.Url());
        if (it == s_map_api.end()) {
            consumed = false;
            return;
        }
        //该api已被消费
        consumed = true;
        AsyncHttpApi api = it->second;
        //异步执行该api，防止阻塞NoticeCenter
        EventPollerPool::Instance().getExecutor()->async([api, parser, invoker]() {
            //执行API
            Json::Value val;
            val["code"] = API::Success;
            HttpSession::KeyValue &headerIn = parser.getValues();
            HttpSession::KeyValue headerOut;
            auto allArgs = getAllArgs(parser);
            headerOut["Content-Type"] = "application/json; charset=utf-8";
            if(api_debug){
                auto newInvoker = [invoker,parser,allArgs](const string &codeOut,
                                                           const HttpSession::KeyValue &headerOut,
                                                           const string &contentOut){
                    stringstream ss;
                    for(auto &pr : allArgs ){
                        ss << pr.first << " : " << pr.second << "\r\n";
                    }

                    DebugL << "\r\n# request:\r\n" << parser.Method() << " " << parser.FullUrl() << "\r\n"
                           << "# content:\r\n" << parser.Content() << "\r\n"
                           << "# args:\r\n" << ss.str()
                           << "# response:\r\n"
                           << contentOut << "\r\n";

                    invoker(codeOut,headerOut,contentOut);
                };
                ((HttpSession::HttpResponseInvoker &)invoker) = newInvoker;
            }

            try {
                api(headerIn, headerOut, allArgs, val, invoker);
            } catch(AuthException &ex){
                val["code"] = API::AuthFailed;
                val["msg"] = ex.what();
                invoker("200 OK", headerOut, val.toStyledString());
            } catch(ApiRetException &ex){
                val["code"] = ex.code();
                val["msg"] = ex.what();
                invoker("200 OK", headerOut, val.toStyledString());
            } catch(SqlException &ex){
                val["code"] = API::SqlFailed;
                val["msg"] = StrPrinter << "操作数据库失败:" << ex.what() << ":" << ex.getSql();
                WarnL << ex.what() << ":" << ex.getSql();
                invoker("200 OK", headerOut, val.toStyledString());
            } catch (std::exception &ex) {
                val["code"] = API::OtherFailed;
                val["msg"] = ex.what();
                invoker("200 OK", headerOut, val.toStyledString());
            }
        });
    });
}

//安装api接口
void installWebApi() {
    addHttpListener();

    /**
     * 获取线程负载
     */
    API_REGIST_INVOKER(api, getThreadsLoad, {
        EventPollerPool::Instance().getExecutorDelay([invoker, headerOut](const vector<int> &vecDelay) {
            Value val;
            auto vec = EventPollerPool::Instance().getExecutorLoad();
            int i = 0;
            for (auto load : vec) {
                Value obj(objectValue);
                obj["load"] = load;
                obj["delay"] = vecDelay[i++];
                val["data"].append(obj);
            }
            invoker("200 OK", headerOut, val.toStyledString());
        });
    });

    /**
     * 获取服务器配置
     */
    API_REGIST(api, getServerConfig, {
        Value obj;
        for (auto &pr : mINI::Instance()) {
            obj[pr.first] = (string &) pr.second;
        }
        val["data"].append(obj);
    });

    /**
     * 设置服务器配置
     */
    API_REGIST(api, setServerConfig, {
        auto &ini = mINI::Instance();
        int changed = 0;
        for (auto &pr : allArgs) {
            if (ini.find(pr.first) == ini.end()) {
                //没有这个key
                continue;
            }
            if (ini[pr.first] == pr.second) {
                continue;
            }
            ini[pr.first] = pr.second;
            //替换成功
            ++changed;
        }
        if (changed > 0) {
            NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastReloadConfig);
            ini.dumpFile();
        }
        val["changed"] = changed;
    });

    /**
     * 获取服务器api列表
     */
    API_REGIST(api,getApiList,{
        for(auto &pr : s_map_api){
            val["data"].append(pr.first);
        }
    });

    /**
     * 重启服务器
     */
    API_REGIST(api,restartServer,{
        EventPollerPool::Instance().getPoller()->doDelayTask(1000,[](){
            //尝试正常退出
            ::kill(getpid(), SIGINT);

            //3秒后强制退出
            EventPollerPool::Instance().getPoller()->doDelayTask(3000,[](){
                exit(0);
                return 0;
            });

            return 0;
        });
        val["msg"] = "服务器将在一秒后自动重启";
    });


    API_REGIST(api,getMediaList,{
        //获取所有MediaSource列表
        val["code"] = 0;
        val["msg"] = "success";
        MediaSource::for_each_media([&](const string &schema,
                                        const string &vhost,
                                        const string &app,
                                        const string &stream,
                                        const MediaSource::Ptr &media){
            if(!allArgs["schema"].empty() && allArgs["schema"] != schema){
                return;
            }
            if(!allArgs["vhost"].empty() && allArgs["vhost"] != vhost){
                return;
            }
            if(!allArgs["app"].empty() && allArgs["app"] != app){
                return;
            }
            Value item;
            item["schema"] = schema;
            item["vhost"] = vhost;
            item["app"] = app;
            item["stream"] = stream;
            val["data"].append(item);
        });
    });

    API_REGIST(api,kick_pusher,{
        //踢掉推流器
        auto src = MediaSource::find(allArgs["schema"],
                                     allArgs["vhost"],
                                     allArgs["app"],
                                     allArgs["stream"]);
        if(src){
            bool flag = src->close();
            val["code"] = flag ? 0 : -1;
            val["msg"] = flag ? "success" : "kick failed";
        }else{
            val["code"] = -2;
            val["msg"] = "can not find the pusher";
        }
    });

    API_REGIST(api,kick_session,{
        //踢掉tcp会话
        auto id = allArgs["id"];
        if(id.empty()){
            val["code"] = -1;
            val["msg"] = "illegal parameter:id";
            return;
        }
        auto session = SessionMap::Instance().get(id);
        if(!session){
            val["code"] = -2;
            val["msg"] = "can not find the target";
            return;
        }
        session->safeShutdown();
        val["code"] = 0;
        val["msg"] = "success";
    });


    static unordered_map<uint64_t ,PlayerProxy::Ptr> s_proxyMap;
    static recursive_mutex s_proxyMapMtx;
    API_REGIST(api,addStreamProxy,{
        //添加拉流代理
        PlayerProxy::Ptr player(new PlayerProxy(
                allArgs["vhost"],
                allArgs["app"],
                allArgs["stream"],
                allArgs["enable_hls"],
                allArgs["enable_mp4"]
        ));
        //指定RTP over TCP(播放rtsp时有效)
        (*player)[kRtpType] = allArgs["rtp_type"].as<int>();
        //开始播放，如果播放失败或者播放中止，将会自动重试若干次，重试次数在配置文件中配置，默认一直重试
        player->play(allArgs["url"]);

        val["data"]["id"] = player.get();
        lock_guard<recursive_mutex> lck(s_proxyMapMtx);
        s_proxyMap[(uint64_t)player.get()] = player;
    });

    API_REGIST(api,delStreamProxy,{
        lock_guard<recursive_mutex> lck(s_proxyMapMtx);
        val["data"]["flag"] = s_proxyMap.erase(allArgs["id"].as<uint64_t>()) == 1;
    });


    ////////////以下是注册的Hook API////////////
    API_REGIST(hook,on_publish,{
        //开始推流事件
        val["code"] = 0;
        val["msg"] = "success";
    });

    API_REGIST(hook,on_play,{
        //开始播放事件
        val["code"] = 0;
        val["msg"] = "success";
    });

    API_REGIST(hook,on_flow_report,{
        //流量统计hook api
        val["code"] = 0;
        val["msg"] = "success";
    });

    API_REGIST(hook,on_rtsp_realm,{
        //rtsp是否需要鉴权
        val["code"] = 0;
        val["realm"] = "zlmediakit_reaml";
    });

    API_REGIST(hook,on_rtsp_auth,{
        //rtsp鉴权密码，密码等于用户名
        //rtsp可以有双重鉴权！后面还会触发on_play事件
        val["code"] = 0;
        val["encrypted"] = false;
        val["passwd"] = allArgs["user_name"].data();
    });

    API_REGIST(hook,on_stream_changed,{
        //媒体注册或反注册事件
        val["code"] = 0;
        val["msg"] = "success";
    });

    API_REGIST(hook,on_stream_not_found,{
        //媒体未找到事件
        val["code"] = 0;
        val["msg"] = "success";
    });


    API_REGIST(hook,on_record_mp4,{
        //录制mp4分片完毕事件
        val["code"] = 0;
        val["msg"] = "success";
    });



}