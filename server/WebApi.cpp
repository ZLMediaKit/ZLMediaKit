/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <signal.h>
#include <functional>
#include <sstream>
#include <unordered_map>
#include "jsoncpp/json.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#ifdef ENABLE_MYSQL
#include "Util/SqlPool.h"
#endif //ENABLE_MYSQL
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Http/HttpRequester.h"
#include "Http/HttpSession.h"
#include "Network/TcpServer.h"
#include "Player/PlayerProxy.h"
#include "Util/MD5.h"
#include "WebApi.h"
#include "WebHook.h"
#include "Thread/WorkThreadPool.h"
#include "Rtp/RtpSelector.h"

#if !defined(_WIN32)
#include "FFmpegSource.h"
#endif//!defined(_WIN32)

using namespace Json;
using namespace toolkit;
using namespace mediakit;


typedef map<string,variant,StrCaseCompare> ApiArgsType;


#define API_ARGS TcpSession &sender, \
                 HttpSession::KeyValue &headerIn, \
                 HttpSession::KeyValue &headerOut, \
                 ApiArgsType &allArgs, \
                 Json::Value &val

#define API_REGIST(field, name, ...) \
    s_map_api.emplace("/index/"#field"/"#name,[](API_ARGS,const HttpSession::HttpResponseInvoker &invoker){ \
         static auto lam = [&](API_ARGS) __VA_ARGS__ ;  \
         lam(sender,headerIn, headerOut, allArgs, val); \
         invoker("200 OK", headerOut, val.toStyledString()); \
     });

#define API_ARGS_VALUE sender,headerIn,headerOut,allArgs,val,invoker

#define API_REGIST_INVOKER(field, name, ...) \
    s_map_api.emplace("/index/"#field"/"#name,[](API_ARGS,const HttpSession::HttpResponseInvoker &invoker) __VA_ARGS__);

//异步http api lambad定义
typedef std::function<void(API_ARGS,const HttpSession::HttpResponseInvoker &invoker)> AsyncHttpApi;
//api列表
static map<string, AsyncHttpApi> s_map_api;

namespace API {
typedef enum {
    InvalidArgs = -300,
    SqlFailed = -200,
    AuthFailed = -100,
    OtherFailed = -1,
    Success = 0
} ApiErr;

#define API_FIELD "api."
const string kApiDebug = API_FIELD"apiDebug";
const string kSecret = API_FIELD"secret";

static onceToken token([]() {
    mINI::Instance()[kApiDebug] = "1";
    mINI::Instance()[kSecret] = "035c73f7-bb6b-4889-a715-d9eb2d1925cc";
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

class InvalidArgsException: public ApiRetException {
public:
    InvalidArgsException(const char *str):ApiRetException(str,API::InvalidArgs){}
    ~InvalidArgsException() = default;
};

class SuccessException: public ApiRetException {
public:
    SuccessException():ApiRetException("success",API::Success){}
    ~SuccessException() = default;
};


//获取HTTP请求中url参数、content参数
static ApiArgsType getAllArgs(const Parser &parser) {
    ApiArgsType allArgs;
    if (parser["Content-Type"].find("application/x-www-form-urlencoded") == 0) {
        auto contentArgs = parser.parseArgs(parser.Content());
        for (auto &pr : contentArgs) {
            allArgs[pr.first] = HttpSession::urlDecode(pr.second);
        }
    } else if (parser["Content-Type"].find("application/json") == 0) {
        try {
            stringstream ss(parser.Content());
            Value jsonArgs;
            ss >> jsonArgs;
            auto keys = jsonArgs.getMemberNames();
            for (auto key = keys.begin(); key != keys.end(); ++key) {
                allArgs[*key] = jsonArgs[*key].asString();
            }
        } catch (std::exception &ex) {
            WarnL << ex.what();
        }
    } else if (!parser["Content-Type"].empty()) {
        WarnL << "invalid Content-Type:" << parser["Content-Type"];
    }

    for (auto &pr :  parser.getUrlArgs()) {
        allArgs[pr.first] = pr.second;
    }
    return std::move(allArgs);
}

static inline void addHttpListener(){
    GET_CONFIG(bool, api_debug, API::kApiDebug);
    //注册监听kBroadcastHttpRequest事件
    NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastHttpRequest, [](BroadcastHttpRequestArgs) {
        auto it = s_map_api.find(parser.Url());
        if (it == s_map_api.end()) {
            consumed = false;
            return;
        }
        //该api已被消费
        consumed = true;
        //执行API
        Json::Value val;
        val["code"] = API::Success;
        HttpSession::KeyValue headerOut;
        auto allArgs = getAllArgs(parser);
        HttpSession::KeyValue &headerIn = parser.getValues();
        headerOut["Content-Type"] = "application/json; charset=utf-8";
        if(api_debug){
            auto newInvoker = [invoker,parser,allArgs](const string &codeOut,
                                                       const HttpSession::KeyValue &headerOut,
                                                       const HttpBody::Ptr &body){
                stringstream ss;
                for(auto &pr : allArgs ){
                    ss << pr.first << " : " << pr.second << "\r\n";
                }

                //body默认为空
                int64_t size = 0;
                if (body && body->remainSize()) {
                    //有body，获取body大小
                    size = body->remainSize();
                }

                if(size < 4 * 1024){
                    string contentOut = body->readData(size)->toString();
                    DebugL << "\r\n# request:\r\n" << parser.Method() << " " << parser.FullUrl() << "\r\n"
                           << "# content:\r\n" << parser.Content() << "\r\n"
                           << "# args:\r\n" << ss.str()
                           << "# response:\r\n"
                           << contentOut << "\r\n";
                    invoker(codeOut,headerOut,contentOut);
                } else{
                    DebugL << "\r\n# request:\r\n" << parser.Method() << " " << parser.FullUrl() << "\r\n"
                           << "# content:\r\n" << parser.Content() << "\r\n"
                           << "# args:\r\n" << ss.str()
                           << "# response size:"
                           << size <<"\r\n";
                    invoker(codeOut,headerOut,body);
                }
            };
            ((HttpSession::HttpResponseInvoker &)invoker) = newInvoker;
        }

        try {
            it->second(sender,headerIn, headerOut, allArgs, val, invoker);
        }  catch(ApiRetException &ex){
            val["code"] = ex.code();
            val["msg"] = ex.what();
            invoker("200 OK", headerOut, val.toStyledString());
        }
#ifdef ENABLE_MYSQL
        catch(SqlException &ex){
            val["code"] = API::SqlFailed;
            val["msg"] = StrPrinter << "操作数据库失败:" << ex.what() << ":" << ex.getSql();
            WarnL << ex.what() << ":" << ex.getSql();
            invoker("200 OK", headerOut, val.toStyledString());
        }
#endif// ENABLE_MYSQL
        catch (std::exception &ex) {
            val["code"] = API::OtherFailed;
            val["msg"] = ex.what();
            invoker("200 OK", headerOut, val.toStyledString());
        }
    });
}

template <typename Args,typename First>
bool checkArgs(Args &&args,First &&first){
    return !args[first].empty();
}

template <typename Args,typename First,typename ...KeyTypes>
bool checkArgs(Args &&args,First &&first,KeyTypes && ...keys){
    return !args[first].empty() && checkArgs(std::forward<Args>(args),std::forward<KeyTypes>(keys)...);
}

#define CHECK_ARGS(...)  \
    if(!checkArgs(allArgs,##__VA_ARGS__)){ \
        throw InvalidArgsException("缺少必要参数:" #__VA_ARGS__); \
    }

#define CHECK_SECRET() \
    if(sender.get_peer_ip() != "127.0.0.1"){ \
        CHECK_ARGS("secret"); \
        if(api_secret != allArgs["secret"]){ \
            throw AuthException("secret错误"); \
        } \
    }

static unordered_map<string ,PlayerProxy::Ptr> s_proxyMap;
static recursive_mutex s_proxyMapMtx;
static inline string getProxyKey(const string &vhost,const string &app,const string &stream){
    return vhost + "/" + app + "/" + stream;
}

#if !defined(_WIN32)
static unordered_map<string ,FFmpegSource::Ptr> s_ffmpegMap;
static recursive_mutex s_ffmpegMapMtx;
#endif//#if !defined(_WIN32)

/**
 * 安装api接口
 * 所有api都支持GET和POST两种方式
 * POST方式参数支持application/json和application/x-www-form-urlencoded方式
 */
void installWebApi() {
    addHttpListener();

    GET_CONFIG(string,api_secret,API::kSecret);

    //获取线程负载
    //测试url http://127.0.0.1/index/api/getThreadsLoad
    API_REGIST_INVOKER(api, getThreadsLoad, {
        EventPollerPool::Instance().getExecutorDelay([invoker, headerOut](const vector<int> &vecDelay) {
            Value val;
            auto vec = EventPollerPool::Instance().getExecutorLoad();
            int i = API::Success;
            for (auto load : vec) {
                Value obj(objectValue);
                obj["load"] = load;
                obj["delay"] = vecDelay[i++];
                val["data"].append(obj);
            }
            val["code"] = API::Success;
            invoker("200 OK", headerOut, val.toStyledString());
        });
    });

    //获取后台工作线程负载
    //测试url http://127.0.0.1/index/api/getWorkThreadsLoad
    API_REGIST_INVOKER(api, getWorkThreadsLoad, {
        WorkThreadPool::Instance().getExecutorDelay([invoker, headerOut](const vector<int> &vecDelay) {
            Value val;
            auto vec = WorkThreadPool::Instance().getExecutorLoad();
            int i = 0;
            for (auto load : vec) {
                Value obj(objectValue);
                obj["load"] = load;
                obj["delay"] = vecDelay[i++];
                val["data"].append(obj);
            }
            val["code"] = API::Success;
            invoker("200 OK", headerOut, val.toStyledString());
        });
    });

    //获取服务器配置
    //测试url http://127.0.0.1/index/api/getServerConfig
    API_REGIST(api, getServerConfig, {
        CHECK_SECRET();
        Value obj;
        for (auto &pr : mINI::Instance()) {
            obj[pr.first] = (string &) pr.second;
        }
        val["data"].append(obj);
    });

    //设置服务器配置
    //测试url(比如关闭http api调试) http://127.0.0.1/index/api/setServerConfig?api.apiDebug=0
    //你也可以通过http post方式传参，可以通过application/x-www-form-urlencoded或application/json方式传参
    API_REGIST(api, setServerConfig, {
        CHECK_SECRET();
        auto &ini = mINI::Instance();
        int changed = API::Success;
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
            ini.dumpFile(g_ini_file);
        }
        val["changed"] = changed;
    });


    //获取服务器api列表
    //测试url http://127.0.0.1/index/api/getApiList
    API_REGIST(api,getApiList,{
        CHECK_SECRET();
        for(auto &pr : s_map_api){
            val["data"].append(pr.first);
        }
    });

#if !defined(_WIN32)
    //重启服务器,只有Daemon方式才能重启，否则是直接关闭！
    //测试url http://127.0.0.1/index/api/restartServer
    API_REGIST(api,restartServer,{
        CHECK_SECRET();
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
#endif//#if !defined(_WIN32)


    //获取流列表，可选筛选参数
    //测试url0(获取所有流) http://127.0.0.1/index/api/getMediaList
    //测试url1(获取虚拟主机为"__defaultVost__"的流) http://127.0.0.1/index/api/getMediaList?vhost=__defaultVost__
    //测试url2(获取rtsp类型的流) http://127.0.0.1/index/api/getMediaList?schema=rtsp
    API_REGIST(api,getMediaList,{
        CHECK_SECRET();
        //获取所有MediaSource列表
        MediaSource::for_each_media([&](const MediaSource::Ptr &media){
            if(!allArgs["schema"].empty() && allArgs["schema"] != media->getSchema()){
                return;
            }
            if(!allArgs["vhost"].empty() && allArgs["vhost"] != media->getVhost()){
                return;
            }
            if(!allArgs["app"].empty() && allArgs["app"] != media->getApp()){
                return;
            }
            Value item;
            item["schema"] = media->getSchema();
            item["vhost"] = media->getVhost();
            item["app"] = media->getApp();
            item["stream"] = media->getId();
            item["readerCount"] = media->readerCount();
            item["totalReaderCount"] = media->totalReaderCount();
            for(auto &track : media->getTracks()){
                Value obj;
                obj["codec_id"] = track->getCodecId();
                obj["codec_type"] = track->getTrackType();
                obj["ready"] = track->ready();
                item["tracks"].append(obj);
            }
            val["data"].append(item);
        });
    });

    //测试url http://127.0.0.1/index/api/isMediaOnline?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs
    API_REGIST(api,isMediaOnline,{
        CHECK_SECRET();
        CHECK_ARGS("schema","vhost","app","stream");
        val["online"] = (bool) (MediaSource::find(allArgs["schema"],allArgs["vhost"],allArgs["app"],allArgs["stream"],false));
    });

    //测试url http://127.0.0.1/index/api/getMediaInfo?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs
    API_REGIST(api,getMediaInfo,{
        CHECK_SECRET();
        CHECK_ARGS("schema","vhost","app","stream");
        auto src = MediaSource::find(allArgs["schema"],allArgs["vhost"],allArgs["app"],allArgs["stream"],false);
        if(!src){
            val["online"] = false;
            return;
        }
        val["online"] = true;
        val["readerCount"] = src->readerCount();
        val["totalReaderCount"] = src->totalReaderCount();
        for(auto &track : src->getTracks()){
            Value obj;
            obj["codec_id"] = track->getCodecId();
            obj["codec_type"] = track->getTrackType();
            obj["ready"] = track->ready();
            val["tracks"].append(obj);
        }
    });

    //主动关断流，包括关断拉流、推流
    //测试url http://127.0.0.1/index/api/close_stream?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs&force=1
    API_REGIST(api,close_stream,{
        CHECK_SECRET();
        CHECK_ARGS("schema","vhost","app","stream");
        //踢掉推流器
        auto src = MediaSource::find(allArgs["schema"],
                                     allArgs["vhost"],
                                     allArgs["app"],
                                     allArgs["stream"]);
        if(src){
            bool flag = src->close(allArgs["force"].as<bool>());
            val["result"] = flag ? 0 : -1;
            val["msg"] = flag ? "success" : "close failed";
        }else{
            val["result"] = -2;
            val["msg"] = "can not find the stream";
        }
    });

    //批量主动关断流，包括关断拉流、推流
    //测试url http://127.0.0.1/index/api/close_streams?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs&force=1
    API_REGIST(api,close_streams,{
        CHECK_SECRET();
        //筛选命中个数
        int count_hit = 0;
        int count_closed = 0;
        list<MediaSource::Ptr> media_list;
        MediaSource::for_each_media([&](const MediaSource::Ptr &media){
            if(!allArgs["schema"].empty() && allArgs["schema"] != media->getSchema()){
                return;
            }
            if(!allArgs["vhost"].empty() && allArgs["vhost"] != media->getVhost()){
                return;
            }
            if(!allArgs["app"].empty() && allArgs["app"] != media->getApp()){
                return;
            }
            if(!allArgs["stream"].empty() && allArgs["stream"] != media->getId()){
                return;
            }
            ++count_hit;
            media_list.emplace_back(media);
        });

        bool force = allArgs["force"].as<bool>();
        for(auto &media : media_list){
            if(media->close(force)){
                ++count_closed;
            }
        }
        val["count_hit"] = count_hit;
        val["count_closed"] = count_closed;
    });

    //获取所有TcpSession列表信息
    //可以根据本地端口和远端ip来筛选
    //测试url(筛选某端口下的tcp会话) http://127.0.0.1/index/api/getAllSession?local_port=1935
    API_REGIST(api,getAllSession,{
        CHECK_SECRET();
        Value jsession;
        uint16_t local_port = allArgs["local_port"].as<uint16_t>();
        string &peer_ip = allArgs["peer_ip"];

        SessionMap::Instance().for_each_session([&](const string &id,const TcpSession::Ptr &session){
            if(local_port != 0 && local_port != session->get_local_port()){
                return;
            }
            if(!peer_ip.empty() && peer_ip != session->get_peer_ip()){
                return;
            }
            jsession["peer_ip"] = session->get_peer_ip();
            jsession["peer_port"] = session->get_peer_port();
            jsession["local_ip"] = session->get_local_ip();
            jsession["local_port"] = session->get_local_port();
            jsession["id"] = id;
            jsession["typeid"] = typeid(*session).name();
            val["data"].append(jsession);
        });
    });

    //断开tcp连接，比如说可以断开rtsp、rtmp播放器等
    //测试url http://127.0.0.1/index/api/kick_session?id=123456
    API_REGIST(api,kick_session,{
        CHECK_SECRET();
        CHECK_ARGS("id");
        //踢掉tcp会话
        auto session = SessionMap::Instance().get(allArgs["id"]);
        if(!session){
            throw ApiRetException("can not find the target",API::OtherFailed);
        }
        session->safeShutdown();
    });


    //批量断开tcp连接，比如说可以断开rtsp、rtmp播放器等
    //测试url http://127.0.0.1/index/api/kick_sessions?local_port=1935
    API_REGIST(api,kick_sessions,{
        CHECK_SECRET();
        uint16_t local_port = allArgs["local_port"].as<uint16_t>();
        string &peer_ip = allArgs["peer_ip"];
        uint64_t count_hit = 0;

        list<TcpSession::Ptr> session_list;
        SessionMap::Instance().for_each_session([&](const string &id,const TcpSession::Ptr &session){
            if(local_port != 0 && local_port != session->get_local_port()){
                return;
            }
            if(!peer_ip.empty() && peer_ip != session->get_peer_ip()){
                return;
            }
            session_list.emplace_back(session);
            ++count_hit;
        });

        for(auto &session : session_list){
            session->safeShutdown();
        }
        val["count_hit"] = (Json::UInt64)count_hit;
    });

    static auto addStreamProxy = [](const string &vhost,
                                    const string &app,
                                    const string &stream,
                                    const string &url,
                                    bool enable_rtsp,
                                    bool enable_rtmp,
                                    bool enable_hls,
                                    bool enable_mp4,
                                    int rtp_type,
                                    const function<void(const SockException &ex,const string &key)> &cb){
        auto key = getProxyKey(vhost,app,stream);
        lock_guard<recursive_mutex> lck(s_proxyMapMtx);
        if(s_proxyMap.find(key) != s_proxyMap.end()){
            //已经在拉流了
            cb(SockException(Err_success),key);
            return;
        }
        //添加拉流代理
        PlayerProxy::Ptr player(new PlayerProxy(vhost,app,stream,enable_rtsp,enable_rtmp,enable_hls,enable_mp4));
        s_proxyMap[key] = player;
        
        //指定RTP over TCP(播放rtsp时有效)
        (*player)[kRtpType] = rtp_type;
        //开始播放，如果播放失败或者播放中止，将会自动重试若干次，默认一直重试
        player->setPlayCallbackOnce([cb,key](const SockException &ex){
            if(ex){
                lock_guard<recursive_mutex> lck(s_proxyMapMtx);
                s_proxyMap.erase(key);
            }
            cb(ex,key);
        });

        //被主动关闭拉流
        player->setOnClose([key](){
            lock_guard<recursive_mutex> lck(s_proxyMapMtx);
            s_proxyMap.erase(key);
        });
        player->play(url);
    };

    //动态添加rtsp/rtmp拉流代理
    //测试url http://127.0.0.1/index/api/addStreamProxy?vhost=__defaultVhost__&app=proxy&enable_rtsp=1&enable_rtmp=1&stream=0&url=rtmp://127.0.0.1/live/obs
    API_REGIST_INVOKER(api,addStreamProxy,{
        CHECK_SECRET();
        CHECK_ARGS("vhost","app","stream","url","enable_rtsp","enable_rtmp");
        addStreamProxy(allArgs["vhost"],
                       allArgs["app"],
                       allArgs["stream"],
                       allArgs["url"],
                       allArgs["enable_rtsp"],/* 是否rtsp转发 */
                       allArgs["enable_rtmp"],/* 是否rtmp转发 */
                       allArgs["enable_hls"],/* 是否hls转发 */
                       allArgs["enable_mp4"],/* 是否MP4录制 */
                       allArgs["rtp_type"],
                       [invoker,val,headerOut](const SockException &ex,const string &key){
                           if(ex){
                               const_cast<Value &>(val)["code"] = API::OtherFailed;
                               const_cast<Value &>(val)["msg"] = ex.what();
                           }else{
                               const_cast<Value &>(val)["data"]["key"] = key;
                           }
                           invoker("200 OK", headerOut, val.toStyledString());
                       });
    });

    //关闭拉流代理
    //测试url http://127.0.0.1/index/api/delStreamProxy?key=__defaultVhost__/proxy/0
    API_REGIST(api,delStreamProxy,{
        CHECK_SECRET();
        CHECK_ARGS("key");
        lock_guard<recursive_mutex> lck(s_proxyMapMtx);
        val["data"]["flag"] = s_proxyMap.erase(allArgs["key"]) == 1;
    });

#if !defined(_WIN32)
    static auto addFFmpegSource = [](const string &src_url,
                                     const string &dst_url,
                                     int timeout_ms,
                                     const function<void(const SockException &ex,const string &key)> &cb){
        auto key = MD5(dst_url).hexdigest();
        lock_guard<decltype(s_ffmpegMapMtx)> lck(s_ffmpegMapMtx);
        if(s_ffmpegMap.find(key) != s_ffmpegMap.end()){
            //已经在拉流了
            cb(SockException(Err_success),key);
            return;
        }

        FFmpegSource::Ptr ffmpeg = std::make_shared<FFmpegSource>();
        s_ffmpegMap[key] = ffmpeg;

        ffmpeg->setOnClose([key](){
            lock_guard<decltype(s_ffmpegMapMtx)> lck(s_ffmpegMapMtx);
            s_ffmpegMap.erase(key);
        });
        ffmpeg->play(src_url, dst_url,timeout_ms,[cb , key](const SockException &ex){
            if(ex){
                lock_guard<decltype(s_ffmpegMapMtx)> lck(s_ffmpegMapMtx);
                s_ffmpegMap.erase(key);
            }
            cb(ex,key);
        });
    };

    //动态添加rtsp/rtmp拉流代理
    //测试url http://127.0.0.1/index/api/addFFmpegSource?src_url=http://live.hkstv.hk.lxdns.com/live/hks2/playlist.m3u8&dst_url=rtmp://127.0.0.1/live/hks2&timeout_ms=10000
    API_REGIST_INVOKER(api,addFFmpegSource,{
        CHECK_SECRET();
        CHECK_ARGS("src_url","dst_url","timeout_ms");
        auto src_url = allArgs["src_url"];
        auto dst_url = allArgs["dst_url"];
        int timeout_ms = allArgs["timeout_ms"];

        addFFmpegSource(src_url,dst_url,timeout_ms,[invoker,val,headerOut](const SockException &ex,const string &key){
            if(ex){
                const_cast<Value &>(val)["code"] = API::OtherFailed;
                const_cast<Value &>(val)["msg"] = ex.what();
            }else{
                const_cast<Value &>(val)["data"]["key"] = key;
            }
            invoker("200 OK", headerOut, val.toStyledString());
        });
    });


    static auto api_delFFmpegSource = [](API_ARGS,const HttpSession::HttpResponseInvoker &invoker){
        CHECK_SECRET();
        CHECK_ARGS("key");
        lock_guard<decltype(s_ffmpegMapMtx)> lck(s_ffmpegMapMtx);
        val["data"]["flag"] = s_ffmpegMap.erase(allArgs["key"]) == 1;
    };

    //关闭拉流代理
    //测试url http://127.0.0.1/index/api/delFFmepgSource?key=key
    API_REGIST(api,delFFmpegSource,{
        api_delFFmpegSource(API_ARGS_VALUE);
    });

    //此处为了兼容之前的拼写错误
    API_REGIST(api,delFFmepgSource,{
        api_delFFmpegSource(API_ARGS_VALUE);
    });
#endif

    //新增http api下载可执行程序文件接口
    //测试url http://127.0.0.1/index/api/downloadBin
    API_REGIST_INVOKER(api,downloadBin,{
        CHECK_SECRET();
        invoker.responseFile(headerIn,StrCaseMap(),exePath());
    });

#if defined(ENABLE_RTPPROXY)
    API_REGIST(api,getSsrcInfo,{
        CHECK_SECRET();
        CHECK_ARGS("ssrc");
        uint32_t ssrc = 0;
        stringstream ss(allArgs["ssrc"]);
        ss >> std::hex >> ssrc;

        auto process = RtpSelector::Instance().getProcess(ssrc,false);
        if(!process){
            val["exist"] = false;
            return;
        }
        val["exist"] = true;
        val["peer_ip"] = process->get_peer_ip();
        val["peer_port"] = process->get_peer_port();
    });
#endif//ENABLE_RTPPROXY

    // 开始录制hls或MP4
    API_REGIST(api,startRecord,{
        CHECK_SECRET();
        CHECK_ARGS("type","vhost","app","stream","wait_for_record","continue_record");

        int result = Recorder::startRecord((Recorder::type)allArgs["type"].as<int>(),
                                           allArgs["vhost"],
                                           allArgs["app"],
                                           allArgs["stream"],
                                           allArgs["customized_path"],
                                           allArgs["wait_for_record"],
                                           allArgs["continue_record"]);
        val["result"] = result;
    });

    // 停止录制hls或MP4
    API_REGIST(api,stopRecord,{
        CHECK_SECRET();
        CHECK_ARGS("type","vhost","app","stream");
        int result = Recorder::stopRecord((Recorder::type)allArgs["type"].as<int>(),
                                          allArgs["vhost"],
                                          allArgs["app"],
                                          allArgs["stream"]);
        val["result"] = result;
    });

    // 获取hls或MP4录制状态
    API_REGIST(api,getRecordStatus,{
        CHECK_SECRET();
        CHECK_ARGS("type","vhost","app","stream");
        auto status = Recorder::getRecordStatus((Recorder::type)allArgs["type"].as<int>(),
                                                allArgs["vhost"],
                                                allArgs["app"],
                                                allArgs["stream"]);
        val["status"] = (int)status;
    });

    ////////////以下是注册的Hook API////////////
    API_REGIST(hook,on_publish,{
        //开始推流事件
        //转换成rtsp或rtmp
        val["enableRtxp"] = true;
        //转换hls
        val["enableHls"] = true;
        //不录制mp4
        val["enableMP4"] = false;
    });

    API_REGIST(hook,on_play,{
        //开始播放事件
        throw SuccessException();
    });

    API_REGIST(hook,on_flow_report,{
        //流量统计hook api
        throw SuccessException();
    });

    API_REGIST(hook,on_rtsp_realm,{
        //rtsp是否需要鉴权，默认需要鉴权
        val["code"] = API::Success;
        val["realm"] = "zlmediakit_reaml";
    });

    API_REGIST(hook,on_rtsp_auth,{
        //rtsp鉴权密码，密码等于用户名
        //rtsp可以有双重鉴权！后面还会触发on_play事件
        CHECK_ARGS("user_name");
        val["code"] = API::Success;
        val["encrypted"] = false;
        val["passwd"] = allArgs["user_name"].data();
    });

    API_REGIST(hook,on_stream_changed,{
        //媒体注册或反注册事件
        throw SuccessException();
    });


#if !defined(_WIN32)
    API_REGIST_INVOKER(hook,on_stream_not_found_ffmpeg,{
        //媒体未找到事件,我们都及时拉流hks作为替代品，目的是为了测试按需拉流
        CHECK_SECRET();
        CHECK_ARGS("vhost","app","stream");
        //通过FFmpeg按需拉流
        GET_CONFIG(int,rtmp_port,Rtmp::kPort);
        GET_CONFIG(int,timeout_sec,Hook::kTimeoutSec);

        string dst_url = StrPrinter
                << "rtmp://127.0.0.1:"
                << rtmp_port << "/"
                << allArgs["app"] << "/"
                << allArgs["stream"] << "?vhost="
                << allArgs["vhost"];

        addFFmpegSource("http://hls-ott-zhibo.wasu.tv/live/272/index.m3u8",/** ffmpeg拉流支持任意编码格式任意协议 **/
                        dst_url,
                        (1000 * timeout_sec) - 500,
                        [invoker,val,headerOut](const SockException &ex,const string &key){
                            if(ex){
                                const_cast<Value &>(val)["code"] = API::OtherFailed;
                                const_cast<Value &>(val)["msg"] = ex.what();
                            }else{
                                const_cast<Value &>(val)["data"]["key"] = key;
                            }
                            invoker("200 OK", headerOut, val.toStyledString());
                        });
    });
#endif//!defined(_WIN32)

    API_REGIST_INVOKER(hook,on_stream_not_found,{
        //媒体未找到事件,我们都及时拉流hks作为替代品，目的是为了测试按需拉流
        CHECK_SECRET();
        CHECK_ARGS("vhost","app","stream");
        //通过内置支持的rtsp/rtmp按需拉流
        addStreamProxy(allArgs["vhost"],
                       allArgs["app"],
                       allArgs["stream"],
                       /** 支持rtsp和rtmp方式拉流 ，rtsp支持h265/h264/aac,rtmp仅支持h264/aac **/
                       "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov",
                       true,/* 开启rtsp转发 */
                       true,/* 开启rtmp转发 */
                       true,/* 开启hls转发 */
                       false,/* 禁用MP4录制 */
                       0,//rtp over tcp方式拉流
                       [invoker,val,headerOut](const SockException &ex,const string &key){
                           if(ex){
                               const_cast<Value &>(val)["code"] = API::OtherFailed;
                               const_cast<Value &>(val)["msg"] = ex.what();
                           }else{
                               const_cast<Value &>(val)["data"]["key"] = key;
                           }
                           invoker("200 OK", headerOut, val.toStyledString());
                       });
    });

    API_REGIST(hook,on_record_mp4,{
        //录制mp4分片完毕事件
        throw SuccessException();
    });

    API_REGIST(hook,on_shell_login,{
        //shell登录调试事件
        throw SuccessException();
    });

    API_REGIST(hook,on_stream_none_reader,{
        //无人观看流默认关闭
        val["close"] = true;
    });

    static auto checkAccess = [](const string &params){
        //我们假定大家都要权限访问
        return true;
    };

    API_REGIST(hook,on_http_access,{
        //在这里根据allArgs["params"](url参数)来判断该http客户端是否有权限访问该文件
        if(!checkAccess(allArgs["params"])){
            //无访问权限
            val["err"] = "无访问权限";
            //仅限制访问当前目录
            val["path"] = "";
            //标记该客户端无权限1分钟
            val["second"] = 60;
            return;
        }

        //可以访问
        val["err"] = "";
        //只能访问当前目录
        val["path"] = "";
        //该http客户端用户被授予10分钟的访问权限，该权限仅限访问当前目录
        val["second"] = 10 * 60;
    });


    API_REGIST(hook,on_server_started,{
        //服务器重启报告
        throw SuccessException();
    });


}

void unInstallWebApi(){
    {
        lock_guard<recursive_mutex> lck(s_proxyMapMtx);
        s_proxyMap.clear();
    }

#if !defined(_WIN32)
    {
        lock_guard<recursive_mutex> lck(s_ffmpegMapMtx);
        s_ffmpegMap.clear();
    }
#endif
}