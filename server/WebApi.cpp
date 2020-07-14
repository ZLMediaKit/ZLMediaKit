/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <sys/stat.h>
#include <math.h>
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
#include "FFmpegSource.h"
#if defined(ENABLE_RTPPROXY)
#include "Rtp/RtpServer.h"
#endif
using namespace Json;
using namespace toolkit;
using namespace mediakit;

namespace API {
typedef enum {
    Exception = -400,//代码抛异常
    InvalidArgs = -300,//参数不合法
    SqlFailed = -200,//sql执行失败
    AuthFailed = -100,//鉴权失败
    OtherFailed = -1,//业务代码执行失败，
    Success = 0//执行成功
} ApiErr;

#define API_FIELD "api."
const string kApiDebug = API_FIELD"apiDebug";
const string kSecret = API_FIELD"secret";
const string kSnapRoot = API_FIELD"snapRoot";
const string kDefaultSnap = API_FIELD"defaultSnap";

static onceToken token([]() {
    mINI::Instance()[kApiDebug] = "1";
    mINI::Instance()[kSecret] = "035c73f7-bb6b-4889-a715-d9eb2d1925cc";
    mINI::Instance()[kSnapRoot] = "./www/snap/";
    mINI::Instance()[kDefaultSnap] = "./www/logo.png";
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

#define API_ARGS1 SockInfo &sender,HttpSession::KeyValue &headerIn, HttpSession::KeyValue &headerOut, ApiArgsType &allArgs, Json::Value &val
#define API_ARGS2 API_ARGS1, const HttpSession::HttpResponseInvoker &invoker
#define API_ARGS_VALUE1 sender,headerIn,headerOut,allArgs,val
#define API_ARGS_VALUE2 API_ARGS_VALUE1, invoker

typedef map<string, variant, StrCaseCompare> ApiArgsType;
//http api列表
static map<string, std::function<void(API_ARGS2)> > s_map_api;

template<typename FUNC>
static void api_regist1(const string &api_path, FUNC &&func) {
    s_map_api.emplace(api_path, [func](API_ARGS2) {
        func(API_ARGS_VALUE1);
        invoker("200 OK", headerOut, val.toStyledString());
    });
}

template<typename FUNC>
static void api_regist2(const string &api_path, FUNC &&func) {
    s_map_api.emplace(api_path, std::forward<FUNC>(func));
}

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
            return;
        }
        //该api已被消费
        consumed = true;
        //执行API
        Json::Value val;
        val["code"] = API::Success;
        HttpSession::KeyValue headerOut;
        auto allArgs = getAllArgs(parser);
        HttpSession::KeyValue &headerIn = parser.getHeader();
        GET_CONFIG(string,charSet,Http::kCharSet);
        headerOut["Content-Type"] = StrPrinter << "application/json; charset=" << charSet;
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

                if(size && size < 4 * 1024){
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
            val["code"] = API::Exception;
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

//拉流代理器列表
static unordered_map<string ,PlayerProxy::Ptr> s_proxyMap;
static recursive_mutex s_proxyMapMtx;

//FFmpeg拉流代理器列表
static unordered_map<string ,FFmpegSource::Ptr> s_ffmpegMap;
static recursive_mutex s_ffmpegMapMtx;

#if defined(ENABLE_RTPPROXY)
//rtp服务器列表
static unordered_map<string, RtpServer::Ptr> s_rtpServerMap;
static recursive_mutex s_rtpServerMapMtx;
#endif

static inline string getProxyKey(const string &vhost,const string &app,const string &stream){
    return vhost + "/" + app + "/" + stream;
}

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
    api_regist2("/index/api/getThreadsLoad",[](API_ARGS2){
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
    api_regist2("/index/api/getWorkThreadsLoad", [](API_ARGS2){
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
    api_regist1("/index/api/getServerConfig",[](API_ARGS1){
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
    api_regist1("/index/api/setServerConfig",[](API_ARGS1){
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


    static auto s_get_api_list = [](API_ARGS1){
        CHECK_SECRET();
        for(auto &pr : s_map_api){
            val["data"].append(pr.first);
        }
    };

    //获取服务器api列表
    //测试url http://127.0.0.1/index/api/getApiList
    api_regist1("/index/api/getApiList",[](API_ARGS1){
        s_get_api_list(API_ARGS_VALUE1);
    });

    //获取服务器api列表
    //测试url http://127.0.0.1/index/
    api_regist1("/index/",[](API_ARGS1){
        s_get_api_list(API_ARGS_VALUE1);
    });

#if !defined(_WIN32)
    //重启服务器,只有Daemon方式才能重启，否则是直接关闭！
    //测试url http://127.0.0.1/index/api/restartServer
    api_regist1("/index/api/restartServer",[](API_ARGS1){
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


    static auto makeMediaSourceJson = [](const MediaSource::Ptr &media){
        Value item;
        item["schema"] = media->getSchema();
        item["vhost"] = media->getVhost();
        item["app"] = media->getApp();
        item["stream"] = media->getId();
        item["readerCount"] = media->readerCount();
        item["totalReaderCount"] = media->totalReaderCount();
        for(auto &track : media->getTracks()){
            Value obj;
            auto codec_type = track->getTrackType();
            obj["codec_id"] = track->getCodecId();
            obj["codec_id_name"] = track->getCodecName();
            obj["ready"] = track->ready();
            obj["codec_type"] = codec_type;
            switch(codec_type){
                case TrackAudio : {
                    auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
                    obj["sample_rate"] = audio_track->getAudioSampleRate();
                    obj["channels"] = audio_track->getAudioChannel();
                    obj["sample_bit"] = audio_track->getAudioSampleBit();
                    break;
                }
                case TrackVideo : {
                    auto video_track = dynamic_pointer_cast<VideoTrack>(track);
                    obj["width"] = video_track->getVideoWidth();
                    obj["height"] = video_track->getVideoHeight();
                    obj["fps"] = round(video_track->getVideoFps());
                    break;
                }
                default:
                    break;
            }
            item["tracks"].append(obj);
        }
        return item;
    };

    //获取流列表，可选筛选参数
    //测试url0(获取所有流) http://127.0.0.1/index/api/getMediaList
    //测试url1(获取虚拟主机为"__defaultVost__"的流) http://127.0.0.1/index/api/getMediaList?vhost=__defaultVost__
    //测试url2(获取rtsp类型的流) http://127.0.0.1/index/api/getMediaList?schema=rtsp
    api_regist1("/index/api/getMediaList",[](API_ARGS1){
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
            val["data"].append(makeMediaSourceJson(media));
        });
    });

    //测试url http://127.0.0.1/index/api/isMediaOnline?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs
    api_regist1("/index/api/isMediaOnline",[](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("schema","vhost","app","stream");
        val["online"] = (bool) (MediaSource::find(allArgs["schema"],allArgs["vhost"],allArgs["app"],allArgs["stream"]));
    });

    //测试url http://127.0.0.1/index/api/getMediaInfo?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs
    api_regist1("/index/api/getMediaInfo",[](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("schema","vhost","app","stream");
        auto src = MediaSource::find(allArgs["schema"],allArgs["vhost"],allArgs["app"],allArgs["stream"]);
        if(!src){
            val["online"] = false;
            return;
        }
        val = makeMediaSourceJson(src);
        val["online"] = true;
        val["code"] = API::Success;
    });

    //主动关断流，包括关断拉流、推流
    //测试url http://127.0.0.1/index/api/close_stream?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs&force=1
    api_regist1("/index/api/close_stream",[](API_ARGS1){
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
            val["code"] = API::OtherFailed;
        }else{
            val["result"] = -2;
            val["msg"] = "can not find the stream";
            val["code"] = API::OtherFailed;
        }
    });

    //批量主动关断流，包括关断拉流、推流
    //测试url http://127.0.0.1/index/api/close_streams?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs&force=1
    api_regist1("/index/api/close_streams",[](API_ARGS1){
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
    api_regist1("/index/api/getAllSession",[](API_ARGS1){
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
    api_regist1("/index/api/kick_session",[](API_ARGS1){
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
    api_regist1("/index/api/kick_sessions",[](API_ARGS1){
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
    api_regist2("/index/api/addStreamProxy",[](API_ARGS2){
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
    api_regist1("/index/api/delStreamProxy",[](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("key");
        lock_guard<recursive_mutex> lck(s_proxyMapMtx);
        val["data"]["flag"] = s_proxyMap.erase(allArgs["key"]) == 1;
    });

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
    api_regist2("/index/api/addFFmpegSource",[](API_ARGS2){
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


    static auto api_delFFmpegSource = [](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("key");
        lock_guard<decltype(s_ffmpegMapMtx)> lck(s_ffmpegMapMtx);
        val["data"]["flag"] = s_ffmpegMap.erase(allArgs["key"]) == 1;
    };

    //关闭拉流代理
    //测试url http://127.0.0.1/index/api/delFFmepgSource?key=key
    api_regist1("/index/api/delFFmpegSource",[](API_ARGS1){
        api_delFFmpegSource(API_ARGS_VALUE1);
    });

    //此处为了兼容之前的拼写错误
    api_regist1("/index/api/delFFmepgSource",[](API_ARGS1){
        api_delFFmpegSource(API_ARGS_VALUE1);
    });

    //新增http api下载可执行程序文件接口
    //测试url http://127.0.0.1/index/api/downloadBin
    api_regist2("/index/api/downloadBin",[](API_ARGS2){
        CHECK_SECRET();
        invoker.responseFile(headerIn,StrCaseMap(),exePath());
    });

#if defined(ENABLE_RTPPROXY)
    api_regist1("/index/api/getRtpInfo",[](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("stream_id");

        auto process = RtpSelector::Instance().getProcess(allArgs["stream_id"], false);
        if (!process) {
            val["exist"] = false;
            return;
        }
        val["exist"] = true;
        val["peer_ip"] = process->get_peer_ip();
        val["peer_port"] = process->get_peer_port();
        val["local_port"] = process->get_local_port();
        val["local_ip"] = process->get_local_ip();
    });

    api_regist1("/index/api/openRtpServer",[](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("port", "enable_tcp", "stream_id");

        auto stream_id = allArgs["stream_id"];

        lock_guard<recursive_mutex> lck(s_rtpServerMapMtx);
        if(s_rtpServerMap.find(stream_id) != s_rtpServerMap.end()) {
            //为了防止RtpProcess所有权限混乱的问题，不允许重复添加相同的stream_id
            throw InvalidArgsException("该stream_id已存在");
        }

        RtpServer::Ptr server = std::make_shared<RtpServer>();
        server->start(allArgs["port"], stream_id, allArgs["enable_tcp"].as<bool>());
        server->setOnDetach([stream_id]() {
            //设置rtp超时移除事件
            lock_guard<recursive_mutex> lck(s_rtpServerMapMtx);
            s_rtpServerMap.erase(stream_id);
        });

        //保存对象
        s_rtpServerMap.emplace(stream_id, server);
        //回复json
        val["port"] = server->getPort();
    });

    api_regist1("/index/api/closeRtpServer",[](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("stream_id");

        lock_guard<recursive_mutex> lck(s_rtpServerMapMtx);
        val["hit"] = (int) s_rtpServerMap.erase(allArgs["stream_id"]);
    });

    api_regist1("/index/api/listRtpServer",[](API_ARGS1){
        CHECK_SECRET();

        lock_guard<recursive_mutex> lck(s_rtpServerMapMtx);
        for (auto &pr : s_rtpServerMap) {
            Value obj;
            obj["stream_id"] = pr.first;
            obj["port"] = pr.second->getPort();
            val["data"].append(obj);
        }
    });

#endif//ENABLE_RTPPROXY

    // 开始录制hls或MP4
    api_regist1("/index/api/startRecord",[](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("type","vhost","app","stream");
        auto result = Recorder::startRecord((Recorder::type) allArgs["type"].as<int>(),
                                              allArgs["vhost"],
                                              allArgs["app"],
                                              allArgs["stream"],
                                              allArgs["customized_path"]);
        val["result"] = result;
        val["code"] = result ? API::Success : API::OtherFailed;
        val["msg"] = result ? "success" :  "start record failed";
    });

    // 停止录制hls或MP4
    api_regist1("/index/api/stopRecord",[](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("type","vhost","app","stream");
        auto result = Recorder::stopRecord((Recorder::type) allArgs["type"].as<int>(),
                                             allArgs["vhost"],
                                             allArgs["app"],
                                             allArgs["stream"]);
        val["result"] = result;
        val["code"] = result ? API::Success : API::OtherFailed;
        val["msg"] = result ? "success" :  "stop record failed";
    });

    // 获取hls或MP4录制状态
    api_regist1("/index/api/isRecording",[](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("type","vhost","app","stream");
        val["status"] = Recorder::isRecording((Recorder::type) allArgs["type"].as<int>(),
                                              allArgs["vhost"],
                                              allArgs["app"],
                                              allArgs["stream"]);
    });

    //获取录像文件夹列表或mp4文件列表
    //http://127.0.0.1/index/api/getMp4RecordFile?vhost=__defaultVhost__&app=live&stream=ss&period=2020-01
    api_regist1("/index/api/getMp4RecordFile", [](API_ARGS1){
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream");
        auto record_path = Recorder::getRecordPath(Recorder::type_mp4, allArgs["vhost"], allArgs["app"],allArgs["stream"]);
        auto period = allArgs["period"];

        //判断是获取mp4文件列表还是获取文件夹列表
        bool search_mp4 = period.size() == sizeof("2020-02-01") - 1;
        if (search_mp4) {
            record_path = record_path + period + "/";
        }

        Json::Value paths(arrayValue);
        //这是筛选日期，获取文件夹列表
        File::scanDir(record_path, [&](const string &path, bool isDir) {
            int pos = path.rfind('/');
            if (pos != string::npos) {
                string relative_path = path.substr(pos + 1);
                if (search_mp4) {
                    if (!isDir) {
                        //我们只收集mp4文件，对文件夹不感兴趣
                        paths.append(relative_path);
                    }
                } else if (isDir && relative_path.find(period) == 0) {
                    //匹配到对应日期的文件夹
                    paths.append(relative_path);
                }
            }
            return true;
        }, false);

        val["data"]["rootPath"] = record_path;
        val["data"]["paths"] = paths;
    });

    static auto responseSnap = [](const string &snap_path,
                                  const HttpSession::KeyValue &headerIn,
                                  const HttpSession::HttpResponseInvoker &invoker) {
        StrCaseMap headerOut;
        struct stat statbuf = {0};
        GET_CONFIG(string, defaultSnap, API::kDefaultSnap);
        if (!(stat(snap_path.data(), &statbuf) == 0 && statbuf.st_size != 0) && !defaultSnap.empty()) {
            //空文件且设置了预设图，则返回预设图片(也就是FFmpeg生成截图中空档期的默认图片)
            const_cast<string&>(snap_path) = File::absolutePath(defaultSnap, "");
            headerOut["Content-Type"] = HttpFileManager::getContentType(snap_path.data());
        } else {
            //之前生成的截图文件，我们默认为jpeg格式
            headerOut["Content-Type"] = HttpFileManager::getContentType(".jpeg");
        }
        //返回图片给http客户端
        invoker.responseFile(headerIn, headerOut, snap_path);
    };

    //获取截图缓存或者实时截图
    //http://127.0.0.1/index/api/getSnap?url=rtmp://127.0.0.1/record/robot.mp4&timeout_sec=10&expire_sec=3
    api_regist2("/index/api/getSnap", [](API_ARGS2){
        CHECK_SECRET();
        CHECK_ARGS("url", "timeout_sec", "expire_sec");
        GET_CONFIG(string, snap_root, API::kSnapRoot);

        int expire_sec = allArgs["expire_sec"];
        auto scan_path = File::absolutePath(MD5(allArgs["url"]).hexdigest(), snap_root) + "/";
        string snap_path;
        File::scanDir(scan_path, [&](const string &path, bool isDir) {
            if (isDir) {
                //忽略文件夹
                return true;
            }

            //找到截图
            auto tm = FindField(path.data() + scan_path.size(), nullptr, ".jpeg");
            if (atoll(tm.data()) + expire_sec < time(NULL)) {
                //截图已经过期,删除之，后面重新生成
                File::delete_file(path.data());
                return true;
            }

            //截图未过期,中断遍历，返回上次生成的截图
            snap_path = path;
            return false;
        });

        if(!snap_path.empty()){
            responseSnap(snap_path, headerIn, invoker);
            return;
        }

        //无截图或者截图已经过期
        snap_path = StrPrinter << scan_path << time(NULL) << ".jpeg";

        //生成一个空文件，目的是顺便创建文件夹路径，
        //同时防止在FFmpeg生成截图途中不停的尝试调用该api启动FFmpeg生成相同的截图
        auto file = File::create_file(snap_path.data(), "wb");
        if (file) {
            fclose(file);
        }

        //启动FFmpeg进程，开始截图
        FFmpegSnap::makeSnap(allArgs["url"],snap_path,allArgs["timeout_sec"],[invoker,headerIn,snap_path](bool success){
            if(!success){
                //生成截图失败，可能残留空文件
                File::delete_file(snap_path.data());
            }
            responseSnap(snap_path, headerIn, invoker);
        });
    });

    ////////////以下是注册的Hook API////////////
    api_regist1("/index/hook/on_publish",[](API_ARGS1){
        //开始推流事件
        //转换成rtsp或rtmp
        val["enableRtxp"] = true;
        //转换hls
        val["enableHls"] = true;
        //不录制mp4
        val["enableMP4"] = false;
    });

    api_regist1("/index/hook/on_play",[](API_ARGS1){
        //开始播放事件
    });

    api_regist1("/index/hook/on_flow_report",[](API_ARGS1){
        //流量统计hook api
    });

    api_regist1("/index/hook/on_rtsp_realm",[](API_ARGS1){
        //rtsp是否需要鉴权，默认需要鉴权
        val["code"] = API::Success;
        val["realm"] = "zlmediakit_reaml";
    });

    api_regist1("/index/hook/on_rtsp_auth",[](API_ARGS1){
        //rtsp鉴权密码，密码等于用户名
        //rtsp可以有双重鉴权！后面还会触发on_play事件
        CHECK_ARGS("user_name");
        val["code"] = API::Success;
        val["encrypted"] = false;
        val["passwd"] = allArgs["user_name"].data();
    });

    api_regist1("/index/hook/on_stream_changed",[](API_ARGS1){
        //媒体注册或反注册事件
    });


#if !defined(_WIN32)
    api_regist2("/index/hook/on_stream_not_found_ffmpeg",[](API_ARGS2){
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

    api_regist2("/index/hook/on_stream_not_found",[](API_ARGS2){
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

    api_regist1("/index/hook/on_record_mp4",[](API_ARGS1){
        //录制mp4分片完毕事件
    });

    api_regist1("/index/hook/on_shell_login",[](API_ARGS1){
        //shell登录调试事件
    });

    api_regist1("/index/hook/on_stream_none_reader",[](API_ARGS1){
        //无人观看流默认关闭
        val["close"] = true;
    });

    static auto checkAccess = [](const string &params){
        //我们假定大家都要权限访问
        return true;
    };

    api_regist1("/index/hook/on_http_access",[](API_ARGS1){
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


    api_regist1("/index/hook/on_server_started",[](API_ARGS1){
        //服务器重启报告
    });


}

void unInstallWebApi(){
    {
        lock_guard<recursive_mutex> lck(s_proxyMapMtx);
        s_proxyMap.clear();
    }

    {
        lock_guard<recursive_mutex> lck(s_ffmpegMapMtx);
        s_ffmpegMap.clear();
    }
    {
#if defined(ENABLE_RTPPROXY)
        lock_guard<recursive_mutex> lck(s_rtpServerMapMtx);
        s_rtpServerMap.clear();
#endif
    }
}