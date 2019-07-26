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
#include <tuple>
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
#include "Kf/DbUtil.h"
#include "Kf/Globals.h"
#include "Util/MD5.h"
#include "WebApi.h"
#include <stdio.h>
#include <dirent.h>
#include <regex>
#include <sys/stat.h>
#include "WebHook.h"


#if !defined(_WIN32)

#include "FFmpegSource.h"

#endif//!defined(_WIN32)

using namespace Json;
using namespace toolkit;
using namespace mediakit;

//chenxiaolei 让response返回的值更丰富些, 以便开发新增的一些api
typedef std::vector<map<string, string>> MultipartPartList;

typedef map<string, variant, StrCaseCompare> ApiArgsType;

struct ArgsContentExt {
    ApiArgsType allArgs;
    Json::Value jsonArgs;
    string rawArgs;
    MultipartPartList partList;
};

#define API_ARGS TcpSession &sender, \
                 HttpSession::KeyValue &headerIn, \
                 HttpSession::KeyValue &headerOut, \
                 ApiArgsType &allArgs, \
                 Json::Value &jsonArgs, \
                 ArgsContentExt &argsContent, \
                 Json::Value &val

#define API_REGIST(field, name, ...) \
    s_map_api.emplace("/index/"#field"/"#name,[](API_ARGS,const HttpSession::HttpResponseInvoker &invoker){ \
         static auto lam = [&](API_ARGS) __VA_ARGS__ ;  \
         lam(sender,headerIn, headerOut, allArgs, jsonArgs, argsContent, val); \
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
static ArgsContentExt getAllArgs(const Parser &parser) {
    ApiArgsType allArgs;
    Value jsonArgs;
    string rawArgs = parser.Content();
    MultipartPartList partList;
    if (parser["Content-Type"].find("application/x-www-form-urlencoded") == 0) {
        auto contentArgs = parser.parseArgs(rawArgs);
        for (auto &pr : contentArgs) {
            allArgs[pr.first] = HttpSession::urlDecode(pr.second);
        }

    } else if (parser["Content-Type"].find("application/json") == 0) {
        try {
            stringstream ss(rawArgs);

            ss >> jsonArgs;
            auto keys = jsonArgs.getMemberNames();
            for (auto key = keys.begin(); key != keys.end(); ++key) {
                allArgs[*key] = jsonArgs[*key].asString();
            }
        } catch (std::exception &ex) {
            WarnL << ex.what();
        }
    } else if (parser["Content-Type"].find("multipart/form-data") == 0) {
        //chenxiaolei 让api支持附件上传,目的是后面的上传通道配置接口(csv)
        //InfoL << "form-data: " << rawArgs;
        auto contentType = parser["Content-Type"];
        auto boundary = FindField((contentType + "\r\n").c_str(), "boundary=", "\r\n");

        string partStartFlag = "--" + boundary;
        string endFlag = "--" + boundary + "--";

        map<string, string> attr;
        _StrPrinter partContent;

        const char *start = rawArgs.c_str();
        int i = 0;
        while (true) {
            auto line = FindField(start, NULL, "\r\n");
            bool isEnd = line.compare(endFlag) == 0;
            bool isNewStart = line.compare(partStartFlag) == 0;

            if (isEnd || isNewStart) {
                if (attr.size() > 0) {
                    auto _partContent = string(partContent);
                    _partContent= trim(_partContent,"\r\n");

                    attr.emplace("PartContent", _partContent);
                    partList.push_back(attr);
                    allArgs[attr["PartName"]] = _partContent;
                }
                if (isEnd) {
                    break;
                }

                attr = map<string, string>();
                partContent = _StrPrinter();
            } else if (line.find("Content-Disposition") == 0) {
                attr.emplace("Content-Disposition", FindField(line.data(), "Content-Disposition: ", ";"));
                attr.emplace("PartName", FindField(line.data(), "name=\"", "\""));
                attr.emplace("OriginalFileName", FindField(line.data(), "filename=\"", "\""));
            } else if (line.find("Content-Type") == 0) {
                attr.emplace("Content-Type", FindField((line + "\r\n").data(), "Content-Type: ", "\r\n"));
            } else {
                partContent << line << "\r\n";
            }
            start = start + line.size() + 2;
            i++;
        }
    } else if (!parser["Content-Type"].empty()) {
        WarnL << "invalid Content-Type:" << parser["Content-Type"];
    }

    auto &urlArgs = parser.getUrlArgs();
    for (auto &pr : urlArgs) {
        allArgs[pr.first] = HttpSession::urlDecode(pr.second);
    }
    //chenxiaolei 让response返回的值更丰富些, 以便开发新增的一些api
    struct ArgsContentExt ret;
    ret.allArgs = std::move(allArgs);
    ret.jsonArgs = jsonArgs;
    ret.rawArgs = rawArgs;
    ret.partList = partList;
    return ret;
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
        //chenxiaolei 让response返回的值更丰富些, 以便开发新增的一些api
        auto argsContentExt = getAllArgs(parser);
        auto allArgs = argsContentExt.allArgs;
        auto jsonArgs = argsContentExt.jsonArgs;
        HttpSession::KeyValue &headerIn = parser.getValues();
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
            it->second(sender, headerIn, headerOut, allArgs, jsonArgs, argsContentExt, val, invoker);
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

//chenxiaolei 判断录像父目录中是否存在录像,避免一些空目录,为[按月查询是否有录像]的接口支持
bool directoryMp4RecordExists(const std::string &directory) {
    DIR *dr = opendir(directory.data());
    if (dr != NULL) {
        struct dirent *de;
        bool exist = false;
        while ((de = readdir(dr)) != NULL) {
            if (std::regex_match(de->d_name, std::regex(("\\d{2}-\\d{2}-\\d{2}\\.mp4\\.json")))) {
                exist = true;
                break;
            }
        }
        closedir(dr);
        return exist;
    }
    return false;
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

#if !defined(_WIN32)
static unordered_map<string, FFmpegSource::Ptr> s_ffmpegMap;
static recursive_mutex s_ffmpegMapMtx;
#endif//#if !defined(_WIN32)

//chenxiaolei 数据库配置的通道使用的proxyMap
unordered_map<string, PlayerProxy::Ptr> m_s_proxyMap;
recursive_mutex m_s_proxyMapMtx;

//chenxiaolei 数据库配置的通道使用的proxyMap
unordered_map<string, FFmpegSource::Ptr> m_s_ffmpegMap;
recursive_mutex m_s_ffmpegMapMtx;

//chenxiaolei 推流实时快照截图支持
struct Snapshot_Info {
    string url;
    string storePath;
};

std::shared_ptr<Timer> snapshotTimer;
recursive_mutex m_s_snapshotTimerMtx;
unordered_map<string, Snapshot_Info> snapshotInfoMap;

//chenxiaolei 快照截图方法
void snapShot(Snapshot_Info info) {
    //InfoL << "snapshot: " <<info.url << " , " <<  info.storePath;
    WorkThreadPool::Instance().getExecutor()->async([info]() {
        //先把之前的删除掉,当目标流中间某段时间不可用,删除保证快照是最新的,不会一直是之前最后一次正常时的截图
        File::delete_file(info.storePath.data());

        char cmd[1024] = {0};
        snprintf(cmd, sizeof(cmd),
                 "nohup ffmpeg -probesize 32768 %s -i %s -y -t 0.001 -ss 1 -f image2 -r 1 -s 720*480 -strftime 1 %s >/dev/null 2>&1",
                 (strncmp(info.url.c_str(), "rtsp", 4) == 0 ? "-rtsp_transport tcp " : ""),
                 info.url.data(),
                 info.storePath.data());
        InfoL << "snapshot_cmd: " << cmd;
        system(cmd);
    });

}

//chenxiaolei 每30秒进行一次快照截图
void processSnapShot() {
    snapshotTimer.reset(new Timer(30, []() {

        unordered_map<string, Snapshot_Info>::iterator p;
        for (p = snapshotInfoMap.begin(); p != snapshotInfoMap.end(); p++) {
            snapShot(p->second);
        }
        return true;
    }, nullptr));
}

//chenxiaolei 配置生效方法
void processProxyCfg(const Json::Value &proxyData, const bool initialize = false) {
    bool vActive = proxyData["active"].asBool();
    std::string proxyKey = proxyData["proxyKey"].asString();

    if (!vActive) {
        //停用了,就停止进行快照截图
        if (snapshotInfoMap.find(proxyKey) != snapshotInfoMap.end()) {
            lock_guard<decltype(m_s_snapshotTimerMtx)> lck(m_s_snapshotTimerMtx);
            snapshotInfoMap.erase(proxyKey);
        }
        return;
    }

    std::string vName = proxyData["name"].asString();
    std::string vHost = proxyData.get("vhost", DEFAULT_VHOST).asString();
    std::string vUrl = proxyData["source_url"].asString();
    std::string vApp = proxyData["app"].asString();
    std::string vStream = proxyData["stream"].asString();
    std::string vFFmpegCmd = proxyData["ffmpeg_cmd"].asString();
    bool vEnableHls = proxyData.get("enable_hls", 1).asInt();
    bool vOnDemand = proxyData.get("on_demand", 1).asInt();  //按需拉流,用户播放时才为频道拉流(不录像才有效)
    int vRecordMp4 = proxyData.get("record_mp4", 0).asInt();    //最长录像天数(0表示不录像)
    int vRtspTransport = proxyData.get("rtsp_transport", 1).asInt();    //最长录像天数(0表示不录像)

    bool realOnDemand = vRecordMp4 ? false : vOnDemand;


    InfoL << "\n========================================================================\n"
          << "========== " << (initialize ? "[初始化]" : "[按需重拉]") << "应用频道代理规则 : " << vName << " ==========\n"
          << "========================================================================\n"
          << "active        :  " << vActive << "\n"
          << "url        :  " << vUrl << "\n"
          << "vhost        :  " << vHost << "\n"
          << "app        :  " << vApp << "\n"
          << "stream     :  " << vStream << "\n"
          << "enable_hls :  " << vEnableHls << "\n"
          << "record_mp4 :  " << vRecordMp4 << "\n"
          << "ffmpeg_cmd :  " << vFFmpegCmd << "\n"
          << "ffmpeg_cmd :  " << vFFmpegCmd << "\n"
          << "on_demand  :  " << vOnDemand << "\n\n";



    //创建存放截图的dir
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    GET_CONFIG(string, rootPath, Http::kRootPath);
    string snapshotStorePath;
    if (enableVhost) {
        snapshotStorePath = rootPath + "/snapshot/" + DEFAULT_VHOST + +"/" + vApp + "/";
    } else {
        snapshotStorePath = rootPath + "/snapshot/" + vApp + "/";
    }
    File::createfile_path(snapshotStorePath.data(), S_IRWXO | S_IRWXG | S_IRWXU);

    string targetRtmpChannelUrl = "rtmp://127.0.0.1:" + mINI::Instance()[Rtmp::kPort] + "/" + vApp + "/" + vStream;
    //加入定时快照截图队列, 按需直播的按接入地址来做截图(没有拉流时从接入地址进行快照,避免按需直播失效), 否者转发出的RTMP流来截
    Snapshot_Info s = {realOnDemand ? vUrl : targetRtmpChannelUrl, snapshotStorePath + vStream + ".png"};
    lock_guard<decltype(m_s_snapshotTimerMtx)> lck(m_s_snapshotTimerMtx);
    snapshotInfoMap[proxyKey] = s;


    if (realOnDemand) {
        //立即截图
        snapShot(s);
    }

    /* 如果关闭录像 或者初始化并开启按需拉流时, 直接退出, 拉流交给kBroadcastNotFoundStream事件*/
    if (initialize && realOnDemand) {
        InfoL << "规则\"" << vName << "\"执行按需拉流模式： " << DEFAULT_VHOST << "/" << vApp << "/" << vStream;
        return;
    }
    if (initialize)
        InfoL << "规则\"" << vName << "\"执行长连拉流模式： " << DEFAULT_VHOST << "/" << vApp << "/" << vStream;

    MediaInfo _media_info;
    _media_info.parse(vUrl);

    //如果是hls, 就使用ffmpeg拉流
    if (_media_info._schema == HTTP_SCHEMA) {

        lock_guard<decltype(m_s_ffmpegMapMtx)> lck(m_s_ffmpegMapMtx);
        if (m_s_ffmpegMap.find(proxyKey) != m_s_ffmpegMap.end()) {
            InfoL << "启用FFmpeg进行推送(忽略,之前已经在拉流了) : " << DEFAULT_VHOST << "/" << vApp << "/" << vStream;
            return;
        }
        FFmpegSource::Ptr ffmpeg = std::make_shared<FFmpegSource>();
        m_s_ffmpegMap[proxyKey] = ffmpeg;
        ffmpeg->setOnClose([proxyKey]() {
            lock_guard<recursive_mutex> lck(m_s_ffmpegMapMtx);
            m_s_ffmpegMap.erase(proxyKey);
        });


        ffmpeg->play(vUrl, targetRtmpChannelUrl, 10000, vFFmpegCmd, [](const SockException &ex) {});
        InfoL << "启用FFmpeg进行推送的 : " << DEFAULT_VHOST << "/" << vApp << "/" << vStream;
    } else {
        lock_guard<recursive_mutex> lck(m_s_proxyMapMtx);
        if (m_s_proxyMap.find(proxyKey) != m_s_proxyMap.end()) {
            InfoL << "启用PlayerProxy进行推送(忽略,之前已经在拉流了)  : " << DEFAULT_VHOST << "/" << vApp << "/" << vStream;
            return;
        }

        PlayerProxy::Ptr player(new PlayerProxy(DEFAULT_VHOST, vApp, vStream, vEnableHls, vRecordMp4));
        m_s_proxyMap[proxyKey] = player;

        //指定RTP over TCP(播放rtsp时有效)
        (*player)[kRtpType] = vRtspTransport == 1 ? Rtsp::RTP_TCP : Rtsp::RTP_UDP;
        player->setPlayCallbackOnce([proxyKey, s](const SockException &ex) {
            /*if(ex){
                lock_guard<recursive_mutex> lck(m_s_proxyMapMtx);
                m_s_proxyMap.erase(proxyKey);
            }*/
            if (!ex) {
                //立即截图
                snapShot(s);
            }
        });

        //被主动关闭拉流
        player->setOnClose([proxyKey]() {
            lock_guard<recursive_mutex> lck(m_s_proxyMapMtx);
            m_s_proxyMap.erase(proxyKey);
        });

        //开始播放，如果播放失败或者播放中止，将会自动重试若干次，重试次数在配置文件中配置，默认一直重试
        player->play(vUrl);

        InfoL << "启用PlayerProxy进行推送的 : " << DEFAULT_VHOST << "/" << vApp << "/" << vStream;
    }


}
//chenxiaolei 配置生效方法
void processProxyCfgs(const Json::Value &cfg_root) {
    for (unsigned int index = 0; index < cfg_root.size(); ++index) {
        Json::Value proxyData = cfg_root[index];
        processProxyCfg(proxyData, true);
    }
    processSnapShot();
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
            //chenxiaolei 统一每一个接口都必须返回 code
            val["code"] = API::Success;
            invoker("200 OK", headerOut, val.toStyledString());
        });
    })


    //chenxiaolei 登录, 判断 secret 是否正确
    //测试url http://127.0.0.1/index/api/login
    API_REGIST(api, login, {
        CHECK_ARGS("secret"); \
        if (api_secret != allArgs["secret"]) {
            val["code"] = API::AuthFailed;
            val["msg"] = "无效的 secret";
        } else {
            val["code"] = API::Success;
            val["secret"] = api_secret;
            val["msg"] = "success";
        }
    })

    //获取服务器配置
    //测试url http://127.0.0.1/index/api/getServerConfig
    API_REGIST(api, getServerConfig, {
        CHECK_SECRET();
        Value obj;
        for (auto &pr : mINI::Instance()) {
            obj[pr.first] = (string &) pr.second;
        }
        //chenxiaolei 统一每一个接口都必须返回 code,  data改为 jsonObj (从 arr改)
        val["data"] = obj;
        val["code"] = API::Success;
    })

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
            ini.dumpFile();
        }
        val["changed"] = changed;
        //chenxiaolei 统一每一个接口都必须返回 code
        val["code"] = API::Success;
    })


    //chenxiaolei 删除通道配置
    //http://127.0.0.1:8099/index/api/deleteChannelConfig
    API_REGIST(api, deleteChannelConfig, {
        CHECK_SECRET();
        CHECK_ARGS("id");

        int id = atoi(allArgs["id"].c_str());
        Json::Value channel = searchChannel(id);

        int rc = deleteChannel(id, [channel]() {
            if (!channel.isNull()) {
                auto proxyKey = channel["proxyKey"].asString();
                lock_guard<recursive_mutex> lck(m_s_proxyMapMtx);
                m_s_proxyMap.erase(proxyKey);
            }
        });

        //SQLITE_OK
        if (rc == 0) {
            val["code"] = API::Success;
            val["msg"] = "success";
        } else {
            val["code"] = API::SqlFailed;
            val["msg"] = "SQL错误码:" + std::to_string(rc);
        }
    })

    //chenxiaolei 保存通道配置(有id更新,没有就创建)
    //http://127.0.0.1:8099/index/api/saveChannelConfig
    API_REGIST(api, saveChannelConfig, {
        CHECK_SECRET();
        //"id", "name"名称, "vhost", "app", "stream", "source_url"接入地址, "active",是否开启 "enable_hls", "record_mp4"录像最大天数,0是不录, "rtsp_transport"RTSP协议, "on_demand"按需播放
        CHECK_ARGS("name", "app", "stream", "source_url", "active", "enable_hls", "record_mp4");

        int rc = 0;
        auto idStr = allArgs["id"];
        int id=idStr.empty() ? 0:  atoi(idStr.c_str());

        rc = saveChannel(id,jsonArgs,[](bool isCreate,Json::Value originalChannel, Json::Value channel){
            if(isCreate){
                processProxyCfg(channel, true);
            }else{
                //先把原来的尝试删除了
                if (!originalChannel.isNull()) {
                    auto proxyKey = originalChannel["proxyKey"].asString();
                    lock_guard<recursive_mutex> lck(m_s_proxyMapMtx);
                    m_s_proxyMap.erase(proxyKey);
                }
                processProxyCfg(channel, true);
            }
        });

        //SQLITE_OK
        if (rc == 0) {
            val["code"] = API::Success;
            val["msg"] = "success";
        } else {
            val["code"] = API::SqlFailed;
            val["msg"] = "SQL错误码:" + std::to_string(rc);
        }
    })


    //chenxiaolei 批量保存通道配置(有id更新,没有就创建)
    //http://127.0.0.1:8099/index/api/saveChannelConfigs
    API_REGIST(api, saveChannelConfigs, {
        CHECK_SECRET();
        //CHECK_ARGS("data");

        Json::Value configArray = jsonArgs["data"];

        for (Json::Value::ArrayIndex i = 0; i != configArray.size(); i++) {
            auto cConfig = configArray[i];
            int rc = 0;
            string idStr = cConfig["id"].asString();
            int id= idStr.empty() ? 0:  atoi(idStr.c_str());
            rc = saveChannel(id,cConfig,[](bool isCreate,Json::Value originalChannel, Json::Value channel){
                if(isCreate){
                    processProxyCfg(channel, true);
                }else{
                    //先把原来的尝试删除了
                    if (!originalChannel.isNull()) {
                        auto proxyKey = originalChannel["proxyKey"].asString();
                        lock_guard<recursive_mutex> lck(m_s_proxyMapMtx);
                        m_s_proxyMap.erase(proxyKey);
                    }
                    processProxyCfg(channel, true);
                }
            });
        }

        val["code"] = API::Success;
        val["msg"] = "success";
    })


    //chenxiaolei 创建通道配置
    //http://127.0.0.1:8099/index/api/createChannelConfig
    API_REGIST(api, createChannelConfig, {
        CHECK_SECRET();
        //"id", "name"名称, "vhost", "app", "stream", "source_url"接入地址, "active",是否开启 "enable_hls", "record_mp4"录像最大天数,0是不录, "rtsp_transport"RTSP协议, "on_demand"按需播放
        CHECK_ARGS("name", "app", "stream", "source_url", "active", "enable_hls", "record_mp4");
        int rc = createChannel(jsonArgs, [](Json::Value channel) {
            processProxyCfg(channel, true);
        });
        //SQLITE_OK
        if (rc == 0) {
            val["code"] = API::Success;
            val["msg"] = "success";
        } else {
            val["code"] = API::SqlFailed;
            val["msg"] = "SQL错误码:" + std::to_string(rc);
        }
    })

    //chenxiaolei 更新通道配置
    //http://127.0.0.1:8099/index/api/updateChannelConfig
    API_REGIST(api, updateChannelConfig, {
        CHECK_SECRET();
        //"id", "name"名称, "vhost", "app", "stream", "source_url"接入地址, "active",是否开启 "enable_hls", "record_mp4"录像最大天数,0是不录, "rtsp_transport"RTSP协议, "on_demand"按需播放
        CHECK_ARGS("id", "name", "app", "stream", "source_url", "active", "enable_hls", "record_mp4");

        int id = atoi(allArgs["id"].c_str());
        Json::Value originalChannel = searchChannel(id);

        int rc = updateChannel(id, jsonArgs, [originalChannel](Json::Value channel) {
            //先把原来的尝试删除了
            if (!originalChannel.isNull()) {
                auto proxyKey = originalChannel["proxyKey"].asString();
                lock_guard<recursive_mutex> lck(m_s_proxyMapMtx);
                m_s_proxyMap.erase(proxyKey);
            }

            processProxyCfg(channel, true);
        });
        //SQLITE_OK
        if (rc == 0) {
            val["code"] = API::Success;
            val["msg"] = "success";
        } else {
            val["code"] = API::SqlFailed;
            val["msg"] = "SQL错误码:" + std::to_string(rc);
        }
    })

    API_REGIST_INVOKER(api, uploadChannelConfigs, {

        EventPollerPool::Instance().getPoller()->async([invoker, argsContent, headerOut]() {
            Value val;
            auto partList = argsContent.partList;
            if (partList.size() > 0) {
                map<string, string> firstPart = partList.front();
                if (firstPart["Content-Type"] != "text/csv") {
                    val["code"] = API::InvalidArgs;
                    val["msg"] = "上传文件Content-Type无效,请检查是否为text/csv";
                } else {
                    auto partContent = partList.front()["PartContent"];
                    Json::Value configArray=channelsCsvStrToJson(partContent);

                    for (Json::Value::ArrayIndex i = 0; i != configArray.size(); i++) {
                        auto cConfig = configArray[i];
                        int rc = 0;
                        string idStr = cConfig["id"].asString();
                        int id= idStr.empty() ? 0:  atoi(idStr.c_str());
                        rc = saveChannel(id,cConfig,[](bool isCreate,Json::Value originalChannel, Json::Value channel){
                            if(isCreate){
                                processProxyCfg(channel, true);
                            }else{
                                //先把原来的尝试删除了
                                if (!originalChannel.isNull()) {
                                    auto proxyKey = originalChannel["proxyKey"].asString();
                                    lock_guard<recursive_mutex> lck(m_s_proxyMapMtx);
                                    m_s_proxyMap.erase(proxyKey);
                                }
                                processProxyCfg(channel, true);
                            }
                        });
                    }


                    val["code"] = API::Success;
                    val["msg"] = "上传成功," + (configArray.isNull()? "0":to_string(configArray.size()-1)) + "条记录变化";
                }
            }


            invoker("200 OK", headerOut, val.toStyledString());
        }, false);


    })

    //chenxiaolei 下载通道配置(列表 csv)
    API_REGIST_INVOKER(api, downloadChannelConfigs, {
        headerOut["Content-Type"] = "text/csv;charset=UTF-8";
        headerOut["Content-disposition"] = "attachment;filename=zlmedia_channels.csv";

        EventPollerPool::Instance().getPoller()->async([invoker, headerOut]() {
            auto channels = searchChannels();
            auto strSend = channelsJsonToCsvStr(channels) ;
            invoker("200 OK", headerOut, strSend);
        }, false);
    })


    //chenxiaolei 获取通道配置(列表)
    //http://127.0.0.1:8099/index/api/searchChannelConfigs
    API_REGIST(api, searchChannelConfigs, {
        CHECK_SECRET();

        auto searchText = allArgs["searchText"];
        auto enableMp4 = allArgs["enableMp4"];
        auto active = allArgs["active"];
        auto page = allArgs["page"];
        auto pageSize = allArgs["pageSize"];
        if (searchText.empty()) {
            searchText = "";
        }
        if (page.empty()) {
            page = 1;
        }
        if (pageSize.empty()) {
            pageSize = 4;
        }

        auto channels = searchChannels(searchText, enableMp4, active, page, pageSize);


        auto channelsData = channels.isNull() ? Json::Value(ValueType::arrayValue) : channels;
        val["data"] = channelsData;
        val["total"] = countChannels(searchText, enableMp4, active);
        val["page"] = page;
        val["pageSize"] = pageSize;
        val["code"] = API::Success;
        val["msg"] = "success";
    })

    //chenxiaolei 获取通道配置(单个详情)
    //http://127.0.0.1:8099/index/api/searchChannelConfig
    API_REGIST(api, searchChannelConfig, {
        CHECK_SECRET();
        CHECK_ARGS("id");
        int id = atoi(allArgs["id"].c_str());
        Json::Value ret = searchChannel(id);

        if (ret.isNull()) {
            val["code"] = API::InvalidArgs;
            val["msg"] = "未找到指定通道";
        } else {
            string sIp = headerIn["Host"];
            string sPort = "80";
            auto mh_pos = sIp.find(":");
            if (mh_pos != string::npos) {
                sPort = sIp.substr(mh_pos + 1);
                sIp = sIp.substr(0, mh_pos);
            }
            Json::Value playAddrs;
            if (ret["enable_hls"].asInt()) {
                playAddrs["hls"] =
                        "http://" + sIp + ":" + sPort + "/" + ret["app"].asString() + "/" + ret["stream"].asString() +
                        "/hls.m3u8";
            }
            playAddrs["rtmp"] =
                    "rtmp://" + sIp + ":" + mINI::Instance()[Rtmp::kPort] + "/" + ret["app"].asString() + "/" +
                    ret["stream"].asString();
            playAddrs["rtsp"] =
                    "rtsp://" + sIp + ":" + mINI::Instance()[Rtsp::kPort] + "/" + ret["app"].asString() + "/" +
                    ret["stream"].asString();
            playAddrs["flv"] =
                    "http://" + sIp + ":" + sPort + "/" + ret["app"].asString() + "/" + ret["stream"].asString() +
                    ".flv";
            playAddrs["ws"] =
                    "ws://" + sIp + ":" + sPort + "/" + ret["app"].asString() + "/" + ret["stream"].asString() +
                    ".flv";
            playAddrs["snapshot"] = "http://" + sIp + ":" + sPort + "/snapshot/" + ret["app"].asString() + "/" +
                                    ret["stream"].asString() + ".png";
            ret["play_addrs"] = playAddrs;

            val["data"] = ret;
            val["code"] = API::Success;
            val["msg"] = "success";
        }

    })

    //chenxiaolei 根据 vhost,app,stream,获取通道配置(单个详情)
    //http://127.0.0.1:8099/index/api/searchChannelConfigByVas
    API_REGIST(api, searchChannelConfigByVas, {
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream");

        Json::Value ret = searchChannel(allArgs["vhost"], allArgs["app"], allArgs["stream"]);

        if (ret.isNull()) {
            val["code"] = API::InvalidArgs;
            val["msg"] = "未找到指定通道";
        } else {

            string sIp = headerIn["Host"];
            string sPort = "80";
            auto mh_pos = sIp.find(":");
            if (mh_pos != string::npos) {
                sPort = sIp.substr(mh_pos + 1);
                sIp = sIp.substr(0, mh_pos);
            }
            Json::Value playAddrs;
            if (ret["enable_hls"].asInt()) {
                playAddrs["hls"] =
                        "http://" + sIp + ":" + sPort + "/" + ret["app"].asString() + "/" + ret["stream"].asString() +
                        "/hls.m3u8";
            }
            playAddrs["rtmp"] =
                    "rtmp://" + sIp + ":" + mINI::Instance()[Rtmp::kPort] + "/" + ret["app"].asString() + "/" +
                    ret["stream"].asString();
            playAddrs["rtsp"] =
                    "rtsp://" + sIp + ":" + mINI::Instance()[Rtsp::kPort] + "/" + ret["app"].asString() + "/" +
                    ret["stream"].asString();
            playAddrs["flv"] =
                    "http://" + sIp + ":" + sPort + "/" + ret["app"].asString() + "/" + ret["stream"].asString() +
                    ".flv";
            playAddrs["snapshot"] = "http://" + sIp + ":" + sPort + "/snapshot/" + ret["app"].asString() + "/" +
                                    ret["stream"].asString() + ".png";
            ret["play_addrs"] = playAddrs;

            val["data"] = ret;
            val["code"] = API::Success;
            val["msg"] = "success";
        }

    })



    //chenxiaolei 获取录像列表(按月)
    //http://127.0.0.1:8099/index/api/queryRecordMonthly?app=pzstll&stream=stream_4&period=201906
    API_REGIST(api, queryRecordMonthly, {
        CHECK_SECRET();
        CHECK_ARGS("app", "stream", "period");

        GET_CONFIG(string, recordAppName, Record::kAppName);
        GET_CONFIG(string, recordPath, Record::kFilePath);
        GET_CONFIG(bool, enableVhost, General::kEnableVhost);

        auto _vhost = allArgs["vhost"];
        auto _app = allArgs["app"];
        auto _stream = allArgs["stream"];
        auto _period = allArgs["period"];

        if (!std::regex_match(_period, std::regex("\\d{6}"))) {
            throw InvalidArgsException("period参数格式错误(YYYYMM)");
        }

        if (_vhost.empty()) {
            _vhost = DEFAULT_VHOST;
        }
        string mp4StreamPath;
        if (enableVhost) {
            mp4StreamPath = recordPath + "/" + _vhost + "/" + recordAppName + "/" + _app + "/" + _stream;
        } else {
            mp4StreamPath = recordPath + "/" + recordAppName + "/" + _app + "/" + _stream;
        }

        Json::Value nVal;
        nVal["vhost"] = _vhost.data();
        nVal["app"] = _app.data();
        nVal["stream"] = _stream.data();
        nVal["app"] = _app.data();

        int dayOfMonth = getNumberOfDays(_period);
        char *dayOfMonthRecordExists = new char[dayOfMonth];

        for (int i = 0; i < dayOfMonth; i++) {
            int d = i + 1;
            string dStr = to_string(d);
            string fDay = (d < 10) ? ("0" + dStr) : dStr;
            dayOfMonthRecordExists[i] = directoryMp4RecordExists(mp4StreamPath + "/" + _period + fDay) ? '1' : '0';
        }

        WarnL << "_period:" << _period;
        WarnL << "dayOfMonth:" << dayOfMonth;
        WarnL << "dayOfMonthRecordExists:" << dayOfMonthRecordExists;

        //flag 由查询月的天数长度的0,1字符串组成, 0就是没有录像,1就是有录像,  如: 001010000000000000000000000000, 意思就是3号,5号有录像
        nVal["flag"] = std::string(dayOfMonthRecordExists, dayOfMonth);
        val["data"] = nVal;

        val["code"] = API::Success;
        val["msg"] = "success";

    })


    //chenxiaolei 获取录像列表(按天)
    //http://127.0.0.1:8099/index/api/queryRecordDaily?app=pzstll&stream=stream_4&period=20190618
    API_REGIST(api, queryRecordDaily, {
        CHECK_SECRET();
        CHECK_ARGS("app", "stream", "period");

        GET_CONFIG(string, recordAppName, Record::kAppName);
        GET_CONFIG(string, recordPath, Record::kFilePath);
        GET_CONFIG(bool, enableVhost, General::kEnableVhost);

        auto _vhost = allArgs["vhost"];
        auto _app = allArgs["app"];
        auto _stream = allArgs["stream"];
        auto _period = allArgs["period"];

        if (!std::regex_match(_period, std::regex("\\d{8}"))) {
            throw InvalidArgsException("period参数格式错误(YYYYMMDD)");
        }

        if (_vhost.empty()) {
            _vhost = DEFAULT_VHOST;
        }
        string mp4FilePath;
        if (enableVhost) {
            mp4FilePath = recordPath + "/" + _vhost + "/" + recordAppName + "/" + _app + "/" + _stream + "/" + _period;
        } else {
            mp4FilePath = recordPath + "/" + recordAppName + "/" + _app + "/" + _stream + "/" + _period;
        }
        Json::Value nVal;
        nVal["vhost"] = _vhost.data();
        nVal["app"] = _app.data();
        nVal["stream"] = _stream.data();
        nVal["app"] = _app.data();

        struct dirent **namelist;
        int n;
        n = scandir(mp4FilePath.c_str(), &namelist, 0, alphasort);
        if (n < 0) {
            val["code"] = API::InvalidArgs;
            val["msg"] = "未找到录像文件";
        } else {

            string sIp = headerIn["Host"];
            string sPort = "80";
            auto mh_pos = sIp.find(":");
            if (mh_pos != string::npos) {
                sPort = sIp.substr(mh_pos + 1);
                sIp = sIp.substr(0, mh_pos);
            }


            int index = 0;
            while (index < n) {
                printf("d_name: %s\n", namelist[index]->d_name);
                if (std::regex_match(namelist[index]->d_name, std::regex("\\d{2}-\\d{2}-\\d{2}\\.mp4\\.json"))) {
                    Json::Value recordInfo;   // will contain the root value after parsing.
                    std::ifstream stream(mp4FilePath + "/" + namelist[index]->d_name, std::ifstream::binary);
                    stream >> recordInfo;
                    //nVal["list"].append(de->d_name);

                    recordInfo["mp4Full"] = "http://" + sIp + ":" + sPort + "/" + recordInfo["mp4"].asString();
                    nVal["list"].append(recordInfo);
                }
                free(namelist[index]);
                index++;
            }
            free(namelist);

            val["code"] = API::Success;
            val["msg"] = "success";
        }
        val["data"] = nVal;
    });

    //chenxiaolei 保活配置的通道转发的的直播拉流,解决某些场景下,按需播放的通道依靠kBroadcastNotFoundStream事件导致播放时总是先要黑屏1,2秒的问题, 前端可以在加载播放器之前提前调用此接口预先把流拉上
    //TODO 此接口有点问题,打不到预想效果,待后面处理
    //http://127.0.0.1:8099/index/api/touchChannelProxyStream?app=pzstll&stream=stream_33
    API_REGIST(api, touchChannelProxyStream, {
        CHECK_SECRET();
        CHECK_ARGS("app", "stream");

        auto _vhost = allArgs["vhost"];
        auto _app = allArgs["app"];
        auto _stream = allArgs["stream"];
        auto _schema = allArgs["schema"];

        if (_vhost.empty()) {
            _vhost = DEFAULT_VHOST;
        }

        if (_schema.empty()) {
            _schema = "RTMP";
        }

        Json::Value nVal;
        nVal["vhost"] = _vhost.data();
        nVal["app"] = _app.data();
        nVal["stream"] = _stream.data();
        nVal["app"] = _app.data();


        string sIp = headerIn["Host"];
        string sPort = "80";
        auto mh_pos = sIp.find(":");
        if (mh_pos != string::npos) {
            sPort = sIp.substr(mh_pos + 1);
            sIp = sIp.substr(0, mh_pos);
        }

        Json::Value tProxyData = searchChannel(_vhost, _app, _stream);
        if (!tProxyData.isNull()) {

            InfoL << "为频道重新拉流:" << _schema << "/" << _vhost << "/" << _app << "/" << _stream;
            processProxyCfg(tProxyData, false);


            nVal["hls"] = "http://" + sIp + ":" + sPort + "/" + _app + "/" + _stream + "/hls.m3u8";
            nVal["rtmp"] = "rtmp://" + sIp + ":" + mINI::Instance()[Rtmp::kPort] + "/" + _app + "/" + _stream;
            nVal["rtsp"] = "rtsp://" + sIp + ":" + mINI::Instance()[Rtsp::kPort] + "/" + _app + "/" + _stream;
            nVal["flv"] = "http://" + sIp + ":" + sPort + "/" + _app + "/" + _stream + ".flv";
            nVal["ws"] = "ws://" + sIp + ":" + sPort + "/" + _app + "/" + _stream + ".flv";
            nVal["snapshot"] = "http://" + sIp + ":" + sPort + "/snapshot/" + _app + "/" + _stream + ".png";
            val["code"] = API::Success;
            val["msg"] = "success";
        } else {
            val["code"] = API::InvalidArgs;
            val["msg"] = "频道未找到: " + _schema + "/" + _vhost + "/" + _app + "/" + _stream + "";
        }

        val["data"] = nVal;
    });

    //获取服务器api列表
    //测试url http://127.0.0.1/index/api/getApiList
    API_REGIST(api,getApiList,{
        CHECK_SECRET();
        for(auto &pr : s_map_api){
            val["data"].append(pr.first);
        }
        //chenxiaolei 统一每一个接口都必须返回 code
        val["code"] = API::Success;
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
        //chenxiaolei 统一每一个接口都必须返回 code
        val["code"] = API::Success;
    });
#endif//#if !defined(_WIN32)


    //获取流列表，可选筛选参数
    //测试url0(获取所有流) http://127.0.0.1/index/api/getMediaList
    //测试url1(获取虚拟主机为"__defaultVost__"的流) http://127.0.0.1/index/api/getMediaList?vhost=__defaultVost__
    //测试url2(获取rtsp类型的流) http://127.0.0.1/index/api/getMediaList?schema=rtsp
    API_REGIST(api,getMediaList,{
        CHECK_SECRET();
        //获取所有MediaSource列表
        val["code"] = API::Success;
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
            val["code"] = flag ? 0 : -1;
            val["msg"] = flag ? "success" : "close failed";
        }else{
            val["code"] = -2;
            val["msg"] = "can not find the stream";
        }
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
            if(local_port != API::Success && local_port != session->get_local_port()){
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
        //chenxiaolei 统一每一个接口都必须返回 code
        val["code"] = API::Success;
    });

    //断开tcp连接，比如说可以断开rtsp、rtmp播放器等
    //测试url http://127.0.0.1/index/api/kick_session?id=123456
    //TODO 此接口有些问题,HTTP-FLV的 TCP 无法断开
    API_REGIST(api,kick_session,{
        CHECK_SECRET();
        CHECK_ARGS("id");
        //踢掉tcp会话
        auto session = SessionMap::Instance().get(allArgs["id"]);
        if(!session){
            val["code"] = API::OtherFailed;
            val["msg"] = "can not find the target";
            return;
        }
        session->safeShutdown();
        val["code"] = API::Success;
        val["msg"] = "success";
    });

    static auto addStreamProxy = [](const string &vhost,
                                    const string &app,
                                    const string &stream,
                                    const string &url,
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
        PlayerProxy::Ptr player(new PlayerProxy(vhost,app,stream,enable_hls,enable_mp4));
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
    //测试url http://127.0.0.1/index/api/addStreamProxy?vhost=__defaultVhost__&app=proxy&stream=0&url=rtmp://127.0.0.1/live/obs
    API_REGIST_INVOKER(api,addStreamProxy,{
        CHECK_SECRET();
        CHECK_ARGS("vhost","app","stream","url");
        addStreamProxy(allArgs["vhost"],
                       allArgs["app"],
                       allArgs["stream"],
                       allArgs["url"],
                       allArgs["enable_hls"],
                       allArgs["enable_mp4"],
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
        //chenxiaolei 统一每一个接口都必须返回 code
        val["code"] = API::Success;
    });

    //关闭拉流代理
    //测试url http://127.0.0.1/index/api/delStreamProxy?key=__defaultVhost__/proxy/0
    API_REGIST(api,delStreamProxy,{
        CHECK_SECRET();
        CHECK_ARGS("key");
        lock_guard<recursive_mutex> lck(s_proxyMapMtx);
        val["data"]["flag"] = s_proxyMap.erase(allArgs["key"]) == 1;
        //chenxiaolei 统一每一个接口都必须返回 code
        val["code"] = API::Success;
    });

#if !defined(_WIN32)
    static auto addFFmepgSource = [](const string &src_url,
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
        //chenxiaolei  支持单独为每一次 play 单独配置 ffmpeg 参数
        ffmpeg->play(src_url, dst_url, timeout_ms, "", [cb, key](const SockException &ex) {
            if (ex) {
                lock_guard<decltype(s_ffmpegMapMtx)> lck(s_ffmpegMapMtx);
                s_ffmpegMap.erase(key);
            }
            cb(ex, key);
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

        addFFmepgSource(src_url,dst_url,timeout_ms,[invoker,val,headerOut](const SockException &ex,const string &key){
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
    //测试url http://127.0.0.1/index/api/delFFmepgSource?key=key
    API_REGIST(api,delFFmepgSource,{
        CHECK_SECRET();
        CHECK_ARGS("key");
        lock_guard<decltype(s_ffmpegMapMtx)> lck(s_ffmpegMapMtx);
        val["data"]["flag"] = s_ffmpegMap.erase(allArgs["key"]) == 1;
        //chenxiaolei 统一每一个接口都必须返回 code
        val["code"] = API::Success;
    });
#endif

    ////////////以下是注册的Hook API////////////
    API_REGIST(hook,on_publish,{
        //开始推流事件
        throw SuccessException();
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
    API_REGIST_INVOKER(hook,on_stream_not_found,{
        //媒体未找到事件,我们都及时拉流hks作为替代品，目的是为了测试按需拉流
        /* CHECK_SECRET();
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

         addFFmepgSource("http://live.hkstv.hk.lxdns.com/live/hks2/playlist.m3u8",*//** ffmpeg拉流支持任意编码格式任意协议 **//*
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
                        });*/
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
                       "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov",//rtmp://live.hkstv.hk.lxdns.com/live/hks2
                       false,
                       false,
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