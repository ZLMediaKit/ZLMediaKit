/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <exception>
#include <sys/stat.h>
#include <math.h>
#include <signal.h>

#ifdef _WIN32
#include <io.h>
#include <iostream>
#include <tchar.h>
#endif // _WIN32

#include <functional>
#include <unordered_map>
#include <regex>
#include "Util/MD5.h"
#include "Util/util.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "Network/TcpServer.h"
#include "Network/UdpServer.h"
#include "Thread/WorkThreadPool.h"

#ifdef ENABLE_MYSQL
#include "Util/SqlPool.h"
#endif //ENABLE_MYSQL

#include "WebApi.h"
#include "WebHook.h"
#include "FFmpegSource.h"

#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Http/HttpSession.h"
#include "Http/HttpRequester.h"
#include "Player/PlayerProxy.h"
#include "Pusher/PusherProxy.h"
#include "Rtp/RtpProcess.h"
#include "Record/MP4Reader.h"

#if defined(ENABLE_RTPPROXY)
#include "Rtp/RtpServer.h"
#endif

#ifdef ENABLE_WEBRTC
#include "../webrtc/WebRtcPlayer.h"
#include "../webrtc/WebRtcPusher.h"
#include "../webrtc/WebRtcEchoTest.h"
#endif

#if defined(ENABLE_VERSION)
#include "ZLMVersion.h"
#endif

#if defined(ENABLE_VIDEOSTACK) && defined(ENABLE_X264) && defined (ENABLE_FFMPEG)
#include "VideoStack.h"
#endif

using namespace std;
using namespace Json;
using namespace toolkit;
using namespace mediakit;

namespace API {
#define API_FIELD "api."
const string kApiDebug = API_FIELD"apiDebug";
const string kSecret = API_FIELD"secret";
const string kSnapRoot = API_FIELD"snapRoot";
const string kDefaultSnap = API_FIELD"defaultSnap";
const string kDownloadRoot = API_FIELD"downloadRoot";

static onceToken token([]() {
    mINI::Instance()[kApiDebug] = "1";
    mINI::Instance()[kSecret] = "035c73f7-bb6b-4889-a715-d9eb2d1925cc";
    mINI::Instance()[kSnapRoot] = "./www/snap/";
    mINI::Instance()[kDefaultSnap] = "./www/logo.png";
    mINI::Instance()[kDownloadRoot] = "./www";
});
}//namespace API

using HttpApi = function<void(const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, SockInfo &sender)>;
// http api列表  [AUTO-TRANSLATED:a05e9d9d]
// http api list
static map<string, HttpApi, StrCaseCompare> s_map_api;

static void responseApi(const Json::Value &res, const HttpSession::HttpResponseInvoker &invoker){
    GET_CONFIG(string, charSet, Http::kCharSet);
    HttpSession::KeyValue headerOut;
    headerOut["Content-Type"] = string("application/json; charset=") + charSet;
    invoker(200, headerOut, res.toStyledString());
};

static void responseApi(int code, const string &msg, const HttpSession::HttpResponseInvoker &invoker){
    Json::Value res;
    res["code"] = code;
    res["msg"] = msg;
    responseApi(res, invoker);
}

static ApiArgsType getAllArgs(const Parser &parser);

static HttpApi toApi(const function<void(API_ARGS_MAP_ASYNC)> &cb) {
    return [cb](const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, SockInfo &sender) {
        GET_CONFIG(string, charSet, Http::kCharSet);
        HttpSession::KeyValue headerOut;
        headerOut["Content-Type"] = string("application/json; charset=") + charSet;

        Json::Value val;
        val["code"] = API::Success;

        // 参数解析成map  [AUTO-TRANSLATED:20e11ff3]
        // Parse parameters into a map
        auto args = getAllArgs(parser);
        cb(sender, headerOut, ArgsMap(parser, args), val, invoker);
    };
}

static HttpApi toApi(const function<void(API_ARGS_MAP)> &cb) {
    return toApi([cb](API_ARGS_MAP_ASYNC) {
        cb(API_ARGS_VALUE);
        invoker(200, headerOut, val.toStyledString());
    });
}

static HttpApi toApi(const function<void(API_ARGS_JSON_ASYNC)> &cb) {
    return [cb](const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, SockInfo &sender) {
        GET_CONFIG(string, charSet, Http::kCharSet);
        HttpSession::KeyValue headerOut;
        headerOut["Content-Type"] = string("application/json; charset=") + charSet;

        Json::Value val;
        val["code"] = API::Success;

        if (parser["Content-Type"].find("application/json") == string::npos) {
            throw InvalidArgsException("该接口只支持json格式的请求");
        }
        // 参数解析成json对象然后处理  [AUTO-TRANSLATED:6f23397b]
        // Parse parameters into a JSON object and then process
        Json::Value args;
        Json::Reader reader;
        reader.parse(parser.content(), args);

        cb(sender, headerOut, ArgsJson(parser, args), val, invoker);
    };
}

static HttpApi toApi(const function<void(API_ARGS_JSON)> &cb) {
    return toApi([cb](API_ARGS_JSON_ASYNC) {
        cb(API_ARGS_VALUE);
        invoker(200, headerOut, val.toStyledString());
    });
}

static HttpApi toApi(const function<void(API_ARGS_STRING_ASYNC)> &cb) {
    return [cb](const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, SockInfo &sender) {
        GET_CONFIG(string, charSet, Http::kCharSet);
        HttpSession::KeyValue headerOut;
        headerOut["Content-Type"] = string("application/json; charset=") + charSet;

        Json::Value val;
        val["code"] = API::Success;

        cb(sender, headerOut, ArgsString(parser, (string &)parser.content()), val, invoker);
    };
}

static HttpApi toApi(const function<void(API_ARGS_STRING)> &cb) {
    return toApi([cb](API_ARGS_STRING_ASYNC) {
        cb(API_ARGS_VALUE);
        invoker(200, headerOut, val.toStyledString());
    });
}

void api_regist(const string &api_path, const function<void(API_ARGS_MAP)> &func) {
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_MAP_ASYNC)> &func) {
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_JSON)> &func) {
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_JSON_ASYNC)> &func) {
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_STRING)> &func){
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_STRING_ASYNC)> &func){
    s_map_api.emplace(api_path, toApi(func));
}

// 获取HTTP请求中url参数、content参数  [AUTO-TRANSLATED:d161a1e1]
// Get URL parameters and content parameters from the HTTP request
static ApiArgsType getAllArgs(const Parser &parser) {
    ApiArgsType allArgs;
    if (parser["Content-Type"].find("application/x-www-form-urlencoded") == 0) {
        auto contentArgs = parser.parseArgs(parser.content());
        for (auto &pr : contentArgs) {
            allArgs[pr.first] = strCoding::UrlDecodeComponent(pr.second);
        }
    } else if (parser["Content-Type"].find("application/json") == 0) {
        try {
            stringstream ss(parser.content());
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
    return allArgs;
}

extern uint64_t getTotalMemUsage();
extern uint64_t getTotalMemBlock();
extern uint64_t getThisThreadMemUsage();
extern uint64_t getThisThreadMemBlock();
extern std::vector<size_t> getBlockTypeSize();
extern uint64_t getTotalMemBlockByType(int type);
extern uint64_t getThisThreadMemBlockByType(int type) ;

static void *web_api_tag = nullptr;

static inline void addHttpListener(){
    GET_CONFIG(bool, api_debug, API::kApiDebug);
    // 注册监听kBroadcastHttpRequest事件  [AUTO-TRANSLATED:4af22c90]
    // Register to listen for the kBroadcastHttpRequest event
    NoticeCenter::Instance().addListener(&web_api_tag, Broadcast::kBroadcastHttpRequest, [](BroadcastHttpRequestArgs) {
        auto it = s_map_api.find(parser.url());
        if (it == s_map_api.end()) {
            return;
        }
        // 该api已被消费  [AUTO-TRANSLATED:db0872fc]
        // This API has been consumed
        consumed = true;

        if(api_debug){
            auto newInvoker = [invoker, parser](int code, const HttpSession::KeyValue &headerOut, const HttpBody::Ptr &body) {
                // body默认为空  [AUTO-TRANSLATED:4fd4ecc8]
                // The body is empty by default
                ssize_t size = 0;
                if (body && body->remainSize()) {
                    // 有body，获取body大小  [AUTO-TRANSLATED:ab1c417d]
                    // If there is a body, get the body size
                    size = body->remainSize();
                }

                LogContextCapture log(getLogger(), toolkit::LDebug, __FILE__, "http api debug", __LINE__);
                log << "\r\n# request:\r\n" << parser.method() << " " << parser.fullUrl() << "\r\n";
                log << "# header:\r\n";

                for (auto &pr : parser.getHeader()) {
                    log << pr.first << " : " << pr.second << "\r\n";
                }

                auto &content = parser.content();
                log << "# content:\r\n" << (content.size() > 4 * 1024 ? content.substr(0, 4 * 1024) : content) << "\r\n";

                if (size > 0 && size < 4 * 1024) {
                    auto response = body->readData(size);
                    log << "# response:\r\n" << response->data() << "\r\n";
                    invoker(code, headerOut, response);
                } else {
                    log << "# response size:" << size << "\r\n";
                    invoker(code, headerOut, body);
                }
            };
            ((HttpSession::HttpResponseInvoker &) invoker) = newInvoker;
        }
        auto helper = static_cast<SocketHelper &>(sender).shared_from_this();
        // 在本poller线程下一次事件循环时执行http api，防止占用NoticeCenter的锁
        helper->getPoller()->async([it, parser, invoker, helper]() {
            try {
                it->second(parser, invoker, *helper);
            } catch (ApiRetException &ex) {
                responseApi(ex.code(), ex.what(), invoker);
                helper->getPoller()->async([helper, ex]() { helper->shutdown(SockException(Err_shutdown, ex.what())); }, false);
            }
#ifdef ENABLE_MYSQL
            catch (SqlException &ex) {
                responseApi(API::SqlFailed, StrPrinter << "操作数据库失败:" << ex.what() << ":" << ex.getSql(), invoker);
            }
#endif // ENABLE_MYSQL
            catch (std::exception &ex) {
                responseApi(API::Exception, ex.what(), invoker);
            }
        },false);
    });
}

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
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        return _map.erase(key);
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
    Pointer makeWithAction(const std::string &key, function<void(Pointer)> action, _Args&& ...__args) {
        // assert(!find(key));

        auto server = std::make_shared<Type>(std::forward<_Args>(__args)...);
        action(server);
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto it = _map.emplace(key, server);
        assert(it.second);
        return server;
    }
};

// 拉流代理器列表  [AUTO-TRANSLATED:6dcfb11f]
// Pull stream proxy list
static ServiceController<PlayerProxy> s_player_proxy;

// 推流代理器列表  [AUTO-TRANSLATED:539a1bcf]
// Push stream proxy list
static ServiceController<PusherProxy> s_pusher_proxy;

// FFmpeg拉流代理器列表  [AUTO-TRANSLATED:4bdedf10]
// FFmpeg pull stream proxy list
static ServiceController<FFmpegSource> s_ffmpeg_src;

#if defined(ENABLE_RTPPROXY)
// rtp服务器列表  [AUTO-TRANSLATED:2e362a8c]
// RTP server list
static ServiceController<RtpServer> s_rtp_server;
#endif


static inline string getPusherKey(const string &schema, const string &vhost, const string &app, const string &stream,
                                  const string &dst_url) {
    return schema + "/" + vhost + "/" + app + "/" + stream + "/" + MD5(dst_url).hexdigest();
}

static void fillSockInfo(Value& val, SockInfo* info) {
    val["peer_ip"] = info->get_peer_ip();
    val["peer_port"] = info->get_peer_port();
    val["local_port"] = info->get_local_port();
    val["local_ip"] = info->get_local_ip();
    val["identifier"] = info->getIdentifier();
}

void dumpMediaTuple(const MediaTuple &tuple, Json::Value& item) {
    item[VHOST_KEY] = tuple.vhost;
    item["app"] = tuple.app;
    item["stream"] = tuple.stream;
    item["params"] = tuple.params;
}

Value ToJson(const PusherProxy::Ptr& p) {
    Value item;
    item["url"] = p->getUrl();
    item["status"] = p->getStatus();
    item["liveSecs"] = p->getLiveSecs();
    item["rePublishCount"] = p->getRePublishCount();    
    item["bytesSpeed"] = (Json::UInt64) p->getSendSpeed();
    item["totalBytes"] =(Json::UInt64) p->getSendTotalBytes();

    if (auto src = p->getSrc()) {
        dumpMediaTuple(src->getMediaTuple(), item["src"]);
    }
    return item;
}

Value ToJson(const PlayerProxy::Ptr& p) {
    Value item;
    item["url"] = p->getUrl();
    item["status"] = p->getStatus();
    item["liveSecs"] = p->getLiveSecs();
    item["rePullCount"] = p->getRePullCount();
    item["totalReaderCount"] = p->totalReaderCount();
    item["bytesSpeed"] = (Json::UInt64) p->getRecvSpeed();
    item["totalBytes"] = (Json::UInt64) p->getRecvTotalBytes();

    dumpMediaTuple(p->getMediaTuple(), item["src"]);
    return item;
}

Value makeMediaSourceJson(MediaSource &media){
    Value item;
    item["schema"] = media.getSchema();
    dumpMediaTuple(media.getMediaTuple(), item);
    item["createStamp"] = (Json::UInt64) media.getCreateStamp();
    item["aliveSecond"] = (Json::UInt64) media.getAliveSecond();
    item["bytesSpeed"] = (Json::UInt64) media.getBytesSpeed();
    item["totalBytes"] = (Json::UInt64) media.getTotalBytes();
    item["readerCount"] = media.readerCount();
    item["totalReaderCount"] = media.totalReaderCount();
    item["originType"] = (int) media.getOriginType();
    item["originTypeStr"] = getOriginTypeString(media.getOriginType());
    item["originUrl"] = media.getOriginUrl();
    item["isRecordingMP4"] = media.isRecording(Recorder::type_mp4);
    item["isRecordingHLS"] = media.isRecording(Recorder::type_hls);
    auto originSock = media.getOriginSock();
    if (originSock) {
        fillSockInfo(item["originSock"], originSock.get());
    } else {
        item["originSock"] = Json::nullValue;
    }

    // getLossRate有线程安全问题；使用getMediaInfo接口才能获取丢包率；getMediaList接口将忽略丢包率  [AUTO-TRANSLATED:b2e927c6]
    // getLossRate has thread safety issues; use the getMediaInfo interface to get the packet loss rate; the getMediaList interface will ignore the packet loss rate
    auto current_thread = false;
    try { current_thread = media.getOwnerPoller()->isCurrentThread();} catch (...) {}
    float last_loss = -1;
    for(auto &track : media.getTracks(false)){
        Value obj;
        auto codec_type = track->getTrackType();
        obj["codec_id"] = track->getCodecId();
        obj["codec_id_name"] = track->getCodecName();
        obj["ready"] = track->ready();
        obj["codec_type"] = codec_type;
        if (current_thread) {
            // rtp推流只有一个统计器，但是可能有多个track，如果短时间多次获取间隔丢包率，第二次会获取为-1  [AUTO-TRANSLATED:5bfbc951]
            // RTP push stream has only one statistics, but may have multiple tracks. If you get the interval packet loss rate multiple times in a short time, the second time will get -1
            auto loss = media.getLossRate(codec_type);
            if (loss == -1) {
                loss = last_loss;
            } else {
                last_loss = loss;
            }
            obj["loss"] = loss;
        }
        obj["frames"] = track->getFrames();
        obj["duration"] = track->getDuration();
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
                obj["key_frames"] = video_track->getVideoKeyFrames();
                int gop_size = video_track->getVideoGopSize();
                int gop_interval_ms = video_track->getVideoGopInterval();
                float fps = video_track->getVideoFps();
                if (fps <= 1 && gop_interval_ms) {
                    fps = gop_size * 1000.0 / gop_interval_ms;
                }
                obj["fps"] = round(fps);
                obj["gop_size"] = gop_size;
                obj["gop_interval_ms"] = gop_interval_ms;
                break;
            }
            default:
                break;
        }
        item["tracks"].append(obj);
    }
    return item;
}

#if defined(ENABLE_RTPPROXY)
uint16_t openRtpServer(uint16_t local_port, const mediakit::MediaTuple &tuple, int tcp_mode, const string &local_ip, bool re_use_port, uint32_t ssrc, int only_track, bool multiplex) {
    auto key = tuple.shortUrl();
    if (s_rtp_server.find(key)) {
        // 为了防止RtpProcess所有权限混乱的问题，不允许重复添加相同的key  [AUTO-TRANSLATED:06c7b14c]
        // To prevent the problem of all permissions being messed up in RtpProcess, duplicate keys are not allowed to be added
        return 0;
    }

    auto server = s_rtp_server.makeWithAction(key, [&](RtpServer::Ptr server) {
        server->start(local_port, local_ip.c_str(), tuple, (RtpServer::TcpMode)tcp_mode, re_use_port, ssrc, only_track, multiplex);
    });
    server->setOnDetach([key](const SockException &ex) {
        // 设置rtp超时移除事件  [AUTO-TRANSLATED:98d42cf3]
        // Set RTP timeout removal event
        s_rtp_server.erase(key);
    });

    // 回复json  [AUTO-TRANSLATED:0c443c6a]
    // Reply JSON
    return server->getPort();
}

#endif

void getStatisticJson(const function<void(Value &val)> &cb) {
    auto obj = std::make_shared<Value>(objectValue);
    auto &val = *obj;
    val["MediaSource"] = (Json::UInt64)(ObjectStatistic<MediaSource>::count());
    val["MultiMediaSourceMuxer"] = (Json::UInt64)(ObjectStatistic<MultiMediaSourceMuxer>::count());

    val["TcpServer"] = (Json::UInt64)(ObjectStatistic<TcpServer>::count());
    val["TcpSession"] = (Json::UInt64)(ObjectStatistic<TcpSession>::count());
    val["UdpServer"] = (Json::UInt64)(ObjectStatistic<UdpServer>::count());
    val["UdpSession"] = (Json::UInt64)(ObjectStatistic<UdpSession>::count());
    val["TcpClient"] = (Json::UInt64)(ObjectStatistic<TcpClient>::count());
    val["Socket"] = (Json::UInt64)(ObjectStatistic<Socket>::count());

    val["FrameImp"] = (Json::UInt64)(ObjectStatistic<FrameImp>::count());
    val["Frame"] = (Json::UInt64)(ObjectStatistic<Frame>::count());

    val["Buffer"] = (Json::UInt64)(ObjectStatistic<Buffer>::count());
    val["BufferRaw"] = (Json::UInt64)(ObjectStatistic<BufferRaw>::count());
    val["BufferLikeString"] = (Json::UInt64)(ObjectStatistic<BufferLikeString>::count());
    val["BufferList"] = (Json::UInt64)(ObjectStatistic<BufferList>::count());

    val["RtpPacket"] = (Json::UInt64)(ObjectStatistic<RtpPacket>::count());
    val["RtmpPacket"] = (Json::UInt64)(ObjectStatistic<RtmpPacket>::count());
#ifdef ENABLE_MEM_DEBUG
    auto bytes = getTotalMemUsage();
    val["totalMemUsage"] = (Json::UInt64) bytes;
    val["totalMemUsageMB"] = (int) (bytes >> 20);
    val["totalMemBlock"] = (Json::UInt64) getTotalMemBlock();
    static auto block_type_size = getBlockTypeSize();
    {
        int i = 0;
        string str;
        size_t last = 0;
        for (auto sz : block_type_size) {
            str.append(to_string(last) + "~" + to_string(sz) + ":" + to_string(getTotalMemBlockByType(i++)) + ";");
            last = sz;
        }
        str.pop_back();
        val["totalMemBlockTypeCount"] = str;
    }

    auto thread_size = EventPollerPool::Instance().getExecutorSize() + WorkThreadPool::Instance().getExecutorSize();
    std::shared_ptr<vector<Value> > thread_mem_info = std::make_shared<vector<Value> >(thread_size);

    shared_ptr<void> finished(nullptr, [thread_mem_info, cb, obj](void *) {
        for (auto &val : *thread_mem_info) {
            (*obj)["threadMem"].append(val);
        }
        // 触发回调  [AUTO-TRANSLATED:08ea452d]
        // Trigger callback
        cb(*obj);
    });

    auto pos = 0;
    auto lam0 = [&](TaskExecutor &executor) {
        auto &val = (*thread_mem_info)[pos++];
        executor.async([finished, &val]() {
            auto bytes = getThisThreadMemUsage();
            val["threadName"] = getThreadName();
            val["threadMemUsage"] = (Json::UInt64) bytes;
            val["threadMemUsageMB"] = (Json::UInt64) (bytes >> 20);
            val["threadMemBlock"] = (Json::UInt64) getThisThreadMemBlock();
            {
                int i = 0;
                string str;
                size_t last = 0;
                for (auto sz : block_type_size) {
                    str.append(to_string(last) + "~" + to_string(sz) + ":" + to_string(getThisThreadMemBlockByType(i++)) + ";");
                    last = sz;
                }
                str.pop_back();
                val["threadMemBlockTypeCount"] = str;
            }
        });
    };
    auto lam1 = [lam0](const TaskExecutor::Ptr &executor) {
        lam0(*executor);
    };
    EventPollerPool::Instance().for_each(lam1);
    WorkThreadPool::Instance().for_each(lam1);
#else
    cb(*obj);
#endif
}

void addStreamProxy(const MediaTuple &tuple, const string &url, int retry_count,
                    const ProtocolOption &option, int rtp_type, float timeout_sec, const mINI &args,
                    const function<void(const SockException &ex, const string &key)> &cb) {
    auto key = tuple.shortUrl();
    if (s_player_proxy.find(key)) {
        // 已经在拉流了  [AUTO-TRANSLATED:e06c57d7]
        // Already pulling stream
        cb(SockException(Err_other, "This stream already exists"), key);
        return;
    }
    // 添加拉流代理  [AUTO-TRANSLATED:aa516f44]
    // Add pull stream proxy
    auto player = s_player_proxy.make(key, tuple, option, retry_count);

    // 先透传拷贝参数  [AUTO-TRANSLATED:22b5605e]
    // First pass-through copy parameters
    for (auto &pr : args) {
        (*player)[pr.first] = pr.second;
    }

    // 指定RTP over TCP(播放rtsp时有效)  [AUTO-TRANSLATED:1a062656]
    // Specify RTP over TCP (effective when playing RTSP)
    (*player)[Client::kRtpType] = rtp_type;

    if (timeout_sec > 0.1f) {
        // 播放握手超时时间  [AUTO-TRANSLATED:5a29ae1f]
        // Play handshake timeout
        (*player)[Client::kTimeoutMS] = timeout_sec * 1000;
    }

    // 开始播放，如果播放失败或者播放中止，将会自动重试若干次，默认一直重试  [AUTO-TRANSLATED:ac8499e5]
    // Start playing. If playback fails or is stopped, it will automatically retry several times, by default it will retry indefinitely
    player->setPlayCallbackOnce([cb, key](const SockException &ex) {
        if (ex) {
            s_player_proxy.erase(key);
        }
        cb(ex, key);
    });

    // 被主动关闭拉流  [AUTO-TRANSLATED:41a19476]
    // The pull stream was actively closed
    player->setOnClose([key](const SockException &ex) {
        s_player_proxy.erase(key);
    });
    player->play(url);
};


void addStreamPusherProxy(const string &schema,
                          const string &vhost,
                          const string &app,
                          const string &stream,
                          const string &url,
                          int retry_count,
                          int rtp_type,
                          float timeout_sec,
                          const mINI &args,
                          const function<void(const SockException &ex, const string &key)> &cb) {
    auto key = getPusherKey(schema, vhost, app, stream, url);
    auto src = MediaSource::find(schema, vhost, app, stream);
    if (!src) {
        cb(SockException(Err_other, "can not find the source stream"), key);
        return;
    }
    if (s_pusher_proxy.find(key)) {
        // 已经在推流了  [AUTO-TRANSLATED:81fcd202]
        // Already pushing stream
        cb(SockException(Err_success), key);
        return;
    }

    // 添加推流代理  [AUTO-TRANSLATED:f9dbc76d]
    // Add push stream proxy
    auto pusher = s_pusher_proxy.make(key, src, retry_count);

    // 先透传拷贝参数  [AUTO-TRANSLATED:22b5605e]
    // First pass-through copy parameters
    for (auto &pr : args) {
        (*pusher)[pr.first] = pr.second;
    }

    // 指定RTP over TCP(播放rtsp时有效)  [AUTO-TRANSLATED:1a062656]
    // Specify RTP over TCP (effective when playing RTSP)
    (*pusher)[Client::kRtpType] = rtp_type;

    if (timeout_sec > 0.1f) {
        // 推流握手超时时间  [AUTO-TRANSLATED:00762fc1]
        // Push stream handshake timeout
        (*pusher)[Client::kTimeoutMS] = timeout_sec * 1000;
    }

    // 开始推流，如果推流失败或者推流中止，将会自动重试若干次，默认一直重试  [AUTO-TRANSLATED:c8b95088]
    // Start pushing stream. If the push stream fails or is stopped, it will automatically retry several times, by default it will retry indefinitely
    pusher->setPushCallbackOnce([cb, key, url](const SockException &ex) {
        if (ex) {
            WarnL << "Push " << url << " failed, key: " << key << ", err: " << ex;
            s_pusher_proxy.erase(key);
        }
        cb(ex, key);
    });

    // 被主动关闭推流  [AUTO-TRANSLATED:bf216f82]
    // Stream closed actively
    pusher->setOnClose([key, url](const SockException &ex) {
        WarnL << "Push " << url << " failed, key: " << key << ", err: " << ex;
        s_pusher_proxy.erase(key);
    });
    pusher->publish(url);
}


/**
 * 安装api接口
 * 所有api都支持GET和POST两种方式
 * POST方式参数支持application/json和application/x-www-form-urlencoded方式
 * Install api interface
 * All apis support GET and POST methods
 * POST method parameters support application/json and application/x-www-form-urlencoded methods
 
 * [AUTO-TRANSLATED:62e68c43]
 */
void installWebApi() {
    addHttpListener();
    GET_CONFIG(string,api_secret,API::kSecret);

    // 获取线程负载  [AUTO-TRANSLATED:3b0ece5c]
    // Get thread load
    // 测试url http://127.0.0.1/index/api/getThreadsLoad  [AUTO-TRANSLATED:de1c93e7]
    // Test url http://127.0.0.1/index/api/getThreadsLoad
    api_regist("/index/api/getThreadsLoad", [](API_ARGS_MAP_ASYNC) {
        CHECK_SECRET();
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
            invoker(200, headerOut, val.toStyledString());
        });
    });

    // 获取后台工作线程负载  [AUTO-TRANSLATED:6166e265]
    // Get background worker thread load
    // 测试url http://127.0.0.1/index/api/getWorkThreadsLoad  [AUTO-TRANSLATED:209a8bc1]
    // Test url http://127.0.0.1/index/api/getWorkThreadsLoad
    api_regist("/index/api/getWorkThreadsLoad", [](API_ARGS_MAP_ASYNC) {
        CHECK_SECRET();
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
            invoker(200, headerOut, val.toStyledString());
        });
    });

    // 获取服务器配置  [AUTO-TRANSLATED:7dd2f3da]
    // Get server configuration
    // 测试url http://127.0.0.1/index/api/getServerConfig  [AUTO-TRANSLATED:59cd0d71]
    // Test url http://127.0.0.1/index/api/getServerConfig
    api_regist("/index/api/getServerConfig",[](API_ARGS_MAP){
        CHECK_SECRET();
        Value obj;
        for (auto &pr : mINI::Instance()) {
            obj[pr.first] = (string &) pr.second;
        }
        val["data"].append(obj);
    });

    // 设置服务器配置  [AUTO-TRANSLATED:3de7bd37]
    // Set server configuration
    // 测试url(比如关闭http api调试) http://127.0.0.1/index/api/setServerConfig?api.apiDebug=0  [AUTO-TRANSLATED:9471d218]
    // Test url (e.g. disable http api debugging) http://127.0.0.1/index/api/setServerConfig?api.apiDebug=0
    // 你也可以通过http post方式传参，可以通过application/x-www-form-urlencoded或application/json方式传参  [AUTO-TRANSLATED:d493a7c0]
    // You can also pass parameters through http post method, you can pass parameters through application/x-www-form-urlencoded or application/json methods
    api_regist("/index/api/setServerConfig",[](API_ARGS_MAP){
        CHECK_SECRET();
        auto &ini = mINI::Instance();
        int changed = API::Success;
        for (auto &pr : allArgs.args) {
            if (ini.find(pr.first) == ini.end()) {
#if 1
                // 没有这个key  [AUTO-TRANSLATED:d6855e02]
                // This key does not exist
                continue;
#else
                // 新增配置选项,为了动态添加多个ffmpeg cmd 模板  [AUTO-TRANSLATED:0f977fcd]
                // Add configuration options to dynamically add multiple ffmpeg cmd templates
                ini[pr.first] = pr.second;
                // 防止changed变化  [AUTO-TRANSLATED:f8ad7e59]
                // Prevent changed changes
                continue;
#endif
            }
            if (pr.first == FFmpeg::kBin) {
                WarnL << "Configuration named " << FFmpeg::kBin << " is not allowed to be set by setServerConfig api.";
                continue;
            }
            if (ini[pr.first] == pr.second) {
                continue;
            }
            ini[pr.first] = pr.second;
            // 替换成功  [AUTO-TRANSLATED:b5d4fec1]
            // Replacement successful
            ++changed;
        }
        if (changed > 0) {
            NOTICE_EMIT(BroadcastReloadConfigArgs, Broadcast::kBroadcastReloadConfig);
            ini.dumpFile(g_ini_file);
        }
        val["changed"] = changed;
    });


    static auto s_get_api_list = [](API_ARGS_MAP){
        CHECK_SECRET();
        for(auto &pr : s_map_api){
            val["data"].append(pr.first);
        }
    };

    // 获取服务器api列表  [AUTO-TRANSLATED:e4c0dd9d]
    // Get server api list
    // 测试url http://127.0.0.1/index/api/getApiList  [AUTO-TRANSLATED:df09e368]
    // Test url http://127.0.0.1/index/api/getApiList
    api_regist("/index/api/getApiList",[](API_ARGS_MAP){
        s_get_api_list(API_ARGS_VALUE);
    });

    // 获取服务器api列表  [AUTO-TRANSLATED:e4c0dd9d]
    // Get server api list
    // 测试url http://127.0.0.1/index/  [AUTO-TRANSLATED:76934dd3]
    // Test url http://127.0.0.1/index/
    api_regist("/index/",[](API_ARGS_MAP){
        s_get_api_list(API_ARGS_VALUE);
    });

#if !defined(_WIN32)
    // 重启服务器,只有Daemon方式才能重启，否则是直接关闭！  [AUTO-TRANSLATED:9d8a1c32]
    // Restart server, only Daemon mode can restart, otherwise it will be closed directly!
    // 测试url http://127.0.0.1/index/api/restartServer  [AUTO-TRANSLATED:8beaaa8a]
    // Test url http://127.0.0.1/index/api/restartServer
    api_regist("/index/api/restartServer",[](API_ARGS_MAP){
        CHECK_SECRET();
        EventPollerPool::Instance().getPoller()->doDelayTask(1000,[](){
            // 尝试正常退出  [AUTO-TRANSLATED:93828d0f]
            // Try to exit normally
            ::kill(getpid(), SIGINT);

            // 3秒后强制退出  [AUTO-TRANSLATED:fdc82920]
            // Force exit after 3 seconds
            EventPollerPool::Instance().getPoller()->doDelayTask(3000,[](){
                exit(0);
                return 0;
            });

            return 0;
        });
        val["msg"] = "MediaServer will reboot in on 1 second";
    });
#else
    // 增加Windows下的重启代码  [AUTO-TRANSLATED:dcba12d5]
    // Add restart code for Windows
    api_regist("/index/api/restartServer", [](API_ARGS_MAP) {
        CHECK_SECRET();
        // 创建重启批处理脚本文件  [AUTO-TRANSLATED:cc18c259]
        // Create a restart batch script file
        FILE *pf;
        errno_t err = ::_wfopen_s(&pf, L"RestartServer.cmd", L"w"); //“w”如果该文件存在，其内容将被覆盖
        if (err == 0) {
            char szExeName[1024];
            char drive[_MAX_DRIVE] = { 0 };
            char dir[_MAX_DIR] = { 0 };
            char fname[_MAX_FNAME] = { 0 };
            char ext[_MAX_EXT] = { 0 };
            char exeName[_MAX_FNAME] = { 0 };
            GetModuleFileNameA(NULL, szExeName, 1024); //获取进程的全路径
            _splitpath(szExeName, drive, dir, fname, ext);
            strcpy(exeName, fname);
            strcat(exeName, ext);
            fprintf(pf, "@echo off\ntaskkill /f /im %s\nstart \"\" \"%s\"\ndel %%0", exeName, szExeName);
            fclose(pf);
            // 1秒后执行创建的批处理脚本  [AUTO-TRANSLATED:596dbca9]
            // Execute the created batch script after 1 second
            EventPollerPool::Instance().getPoller()->doDelayTask(1000, []() {
                STARTUPINFO si;
                PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof si);
                ZeroMemory(&pi, sizeof pi);
                si.cb = sizeof si;
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                TCHAR winSysDir[1024];
                ZeroMemory(winSysDir, sizeof winSysDir);
                GetSystemDirectory(winSysDir, 1024);
                TCHAR appName[1024];
                ZeroMemory(appName, sizeof appName);

                _stprintf(appName, "%s\\cmd.exe", winSysDir);
                BOOL bRet = CreateProcess(appName, " /c RestartServer.cmd", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

                if (bRet == FALSE) {
                    int err = GetLastError();
                    cout << endl << "无法执行重启操作，错误代码：" << err << endl;
                }
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return 0;
            });
            val["msg"] = "服务器将在一秒后自动重启";
        } else {
            val["msg"] = "创建重启脚本文件失败";
            val["code"] = API::OtherFailed;
        }
    });
#endif//#if !defined(_WIN32)

    // 获取流列表，可选筛选参数  [AUTO-TRANSLATED:68ffc6b6]
    // Get stream list, optional filtering parameters
    // 测试url0(获取所有流) http://127.0.0.1/index/api/getMediaList  [AUTO-TRANSLATED:434652ea]
    // Test url0 (get all streams) http://127.0.0.1/index/api/getMediaList
    // 测试url1(获取虚拟主机为"__defaultVost__"的流) http://127.0.0.1/index/api/getMediaList?vhost=__defaultVost__  [AUTO-TRANSLATED:5d9bd1ee]
    // Test url1 (get streams with virtual host "__defaultVost__") http://127.0.0.1/index/api/getMediaList?vhost=__defaultVost__
    // 测试url2(获取rtsp类型的流) http://127.0.0.1/index/api/getMediaList?schema=rtsp  [AUTO-TRANSLATED:21c2c15d]
    // Test url2 (get rtsp type streams) http://127.0.0.1/index/api/getMediaList?schema=rtsp
    api_regist("/index/api/getMediaList",[](API_ARGS_MAP){
        CHECK_SECRET();
        // 获取所有MediaSource列表  [AUTO-TRANSLATED:7bf16dc2]
        // Get all MediaSource lists
        MediaSource::for_each_media([&](const MediaSource::Ptr &media) {
            val["data"].append(makeMediaSourceJson(*media));
        }, allArgs["schema"], allArgs["vhost"], allArgs["app"], allArgs["stream"]);
    });

    // 测试url http://127.0.0.1/index/api/isMediaOnline?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs  [AUTO-TRANSLATED:126a75e8]
    // Test url http://127.0.0.1/index/api/isMediaOnline?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs
    api_regist("/index/api/isMediaOnline",[](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("schema","vhost","app","stream");
        val["online"] = (bool) (MediaSource::find(allArgs["schema"],allArgs["vhost"],allArgs["app"],allArgs["stream"]));
    });

    // 获取媒体流播放器列表  [AUTO-TRANSLATED:bcadf31c]
    // Get media stream player list
    // 测试url http://127.0.0.1/index/api/getMediaPlayerList?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs  [AUTO-TRANSLATED:2aab7522]
    // Test url http://127.0.0.1/index/api/getMediaPlayerList?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs
    api_regist("/index/api/getMediaPlayerList",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("schema", "vhost", "app", "stream");
        auto src = MediaSource::find(allArgs["schema"], allArgs["vhost"], allArgs["app"], allArgs["stream"]);
        if (!src) {
            throw ApiRetException("can not find the stream", API::NotFound);
        }
        src->getPlayerList(
            [=](const std::list<toolkit::Any> &info_list) mutable {
                val["code"] = API::Success;
                auto &data = val["data"];
                data = Value(arrayValue);
                for (auto &info : info_list) {
                    auto &obj = info.get<Value>();
                    data.append(std::move(obj));
                }
                invoker(200, headerOut, val.toStyledString());
            },
            [](toolkit::Any &&info) -> toolkit::Any {
                auto obj = std::make_shared<Value>();
                auto &session = info.get<Session>();
                fillSockInfo(*obj, &session);
                (*obj)["typeid"] = toolkit::demangle(typeid(session).name());
                toolkit::Any ret;
                ret.set(obj);
                return ret;
            });
    });

    api_regist("/index/api/broadcastMessage", [](API_ARGS_MAP) {
        CHECK_SECRET();
        CHECK_ARGS("schema", "vhost", "app", "stream", "msg");
        auto src = MediaSource::find(allArgs["schema"], allArgs["vhost"], allArgs["app"], allArgs["stream"]);
        if (!src) {
            throw ApiRetException("can not find the stream", API::NotFound);
        }
        Any any;
        Buffer::Ptr buffer = std::make_shared<BufferLikeString>(allArgs["msg"]);
        any.set(std::move(buffer));
        src->broadcastMessage(any);
    });

    // 测试url http://127.0.0.1/index/api/getMediaInfo?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs  [AUTO-TRANSLATED:9402e811]
    // Test url http://127.0.0.1/index/api/getMediaInfo?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs
    api_regist("/index/api/getMediaInfo",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("schema","vhost","app","stream");
        auto src = MediaSource::find(allArgs["schema"],allArgs["vhost"],allArgs["app"],allArgs["stream"]);
        if(!src){
            throw ApiRetException("can not find the stream", API::NotFound);
        }
        src->getOwnerPoller()->async([=]() mutable {
            auto val = makeMediaSourceJson(*src);
            val["code"] = API::Success;
            invoker(200, headerOut, val.toStyledString());
        });
    });

    // 主动关断流，包括关断拉流、推流  [AUTO-TRANSLATED:80506955]
    // Actively close the stream, including closing the pull stream and push stream
    // 测试url http://127.0.0.1/index/api/close_stream?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs&force=1  [AUTO-TRANSLATED:c3831592]
    // Test url http://127.0.0.1/index/api/close_stream?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs&force=1
    api_regist("/index/api/close_stream",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("schema","vhost","app","stream");
        // 踢掉推流器  [AUTO-TRANSLATED:61e39b14]
        // Kick out the pusher
        auto src = MediaSource::find(allArgs["schema"],
                                     allArgs["vhost"],
                                     allArgs["app"],
                                     allArgs["stream"]);
        if (!src) {
            throw ApiRetException("can not find the stream", API::NotFound);
        }

        bool force = allArgs["force"].as<bool>();
        src->getOwnerPoller()->async([=]() mutable {
            bool flag = src->close(force);
            val["result"] = flag ? 0 : -1;
            val["msg"] = flag ? "success" : "close failed";
            val["code"] = flag ? API::Success : API::OtherFailed;
            invoker(200, headerOut, val.toStyledString());
        });
    });

    // 批量主动关断流，包括关断拉流、推流  [AUTO-TRANSLATED:5d180cd8]
    // Batch actively close the stream, including closing the pull stream and push stream
    // 测试url http://127.0.0.1/index/api/close_streams?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs&force=1  [AUTO-TRANSLATED:786933db]
    // Test url http://127.0.0.1/index/api/close_streams?schema=rtsp&vhost=__defaultVhost__&app=live&stream=obs&force=1
    api_regist("/index/api/close_streams",[](API_ARGS_MAP){
        CHECK_SECRET();
        // 筛选命中个数  [AUTO-TRANSLATED:6db1e8c7]
        // Filter hit count
        int count_hit = 0;
        int count_closed = 0;
        list<MediaSource::Ptr> media_list;
        MediaSource::for_each_media([&](const MediaSource::Ptr &media) {
            ++count_hit;
            media_list.emplace_back(media);
        }, allArgs["schema"], allArgs["vhost"], allArgs["app"], allArgs["stream"]);

        bool force = allArgs["force"].as<bool>();
        for (auto &media : media_list) {
            if (media->close(force)) {
                ++count_closed;
            }
        }
        val["count_hit"] = count_hit;
        val["count_closed"] = count_closed;
    });

    // 获取所有Session列表信息  [AUTO-TRANSLATED:e785052d]
    // Get all Session list information
    // 可以根据本地端口和远端ip来筛选  [AUTO-TRANSLATED:4d4c9d61]
    // You can filter by local port and remote ip
    // 测试url(筛选某端口下的tcp会话) http://127.0.0.1/index/api/getAllSession?local_port=1935  [AUTO-TRANSLATED:ef845193]
    // Test url (filter tcp session under a certain port) http://127.0.0.1/index/api/getAllSession?local_port=1935
    api_regist("/index/api/getAllSession",[](API_ARGS_MAP){
        CHECK_SECRET();
        Value jsession;
        uint16_t local_port = allArgs["local_port"].as<uint16_t>();
        string peer_ip = allArgs["peer_ip"];

        SessionMap::Instance().for_each_session([&](const string &id,const Session::Ptr &session){
            if(local_port != 0 && local_port != session->get_local_port()){
                return;
            }
            if(!peer_ip.empty() && peer_ip != session->get_peer_ip()){
                return;
            }
            fillSockInfo(jsession, session.get());
            jsession["id"] = id;
            jsession["typeid"] = toolkit::demangle(typeid(*session).name());
            val["data"].append(jsession);
        });
    });

    // 断开tcp连接，比如说可以断开rtsp、rtmp播放器等  [AUTO-TRANSLATED:9147ffec]
    // Disconnect the tcp connection, for example, you can disconnect the rtsp, rtmp player, etc.
    // 测试url http://127.0.0.1/index/api/kick_session?id=123456  [AUTO-TRANSLATED:c2880cb5]
    // Test url http://127.0.0.1/index/api/kick_session?id=123456
    api_regist("/index/api/kick_session",[](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("id");
        // 踢掉tcp会话  [AUTO-TRANSLATED:f6f318bd]
        // Kick out the tcp session
        auto session = SessionMap::Instance().get(allArgs["id"]);
        if(!session){
            throw ApiRetException("can not find the target",API::OtherFailed);
        }
        session->safeShutdown();
    });


    // 批量断开tcp连接，比如说可以断开rtsp、rtmp播放器等  [AUTO-TRANSLATED:fef59eb8]
    // Batch disconnect tcp connections, for example, you can disconnect rtsp, rtmp players, etc.
    // 测试url http://127.0.0.1/index/api/kick_sessions?local_port=1935  [AUTO-TRANSLATED:5891b482]
    // Test url http://127.0.0.1/index/api/kick_sessions?local_port=1935
    api_regist("/index/api/kick_sessions", [](API_ARGS_MAP) {
        CHECK_SECRET();
        uint16_t local_port = allArgs["local_port"].as<uint16_t>();
        string peer_ip = allArgs["peer_ip"];
        size_t count_hit = 0;

        list<Session::Ptr> session_list;
        SessionMap::Instance().for_each_session([&](const string &id, const Session::Ptr &session) {
            if (local_port != 0 && local_port != session->get_local_port()) {
                return;
            }
            if (!peer_ip.empty() && peer_ip != session->get_peer_ip()) {
                return;
            }
            if (session->getIdentifier() == sender.getIdentifier()) {
                // 忽略本http链接  [AUTO-TRANSLATED:9fb4bf76]
                // Ignore this http link
                return;
            }
            session_list.emplace_back(session);
            ++count_hit;
        });

        for (auto &session : session_list) {
            session->safeShutdown();
        }
        val["count_hit"] = (Json::UInt64)count_hit;
    });

    // 动态添加rtsp/rtmp推流代理  [AUTO-TRANSLATED:2eb09bc9]
    // Dynamically add rtsp/rtmp push stream proxy
    // 测试url http://127.0.0.1/index/api/addStreamPusherProxy?schema=rtmp&vhost=__defaultVhost__&app=proxy&stream=0&dst_url=rtmp://127.0.0.1/live/obs  [AUTO-TRANSLATED:25d7d4b0]
    // Test url http://127.0.0.1/index/api/addStreamPusherProxy?schema=rtmp&vhost=__defaultVhost__&app=proxy&stream=0&dst_url=rtmp://127.0.0.1/live/obs
    api_regist("/index/api/addStreamPusherProxy", [](API_ARGS_MAP_ASYNC) {
        CHECK_SECRET();
        CHECK_ARGS("schema", "vhost", "app", "stream", "dst_url");

        mINI args;
        for (auto &pr : allArgs.args) {
            args.emplace(pr.first, pr.second);
        }

        auto dst_url = allArgs["dst_url"];
        auto retry_count = allArgs["retry_count"].empty() ? -1 : allArgs["retry_count"].as<int>();
        EventPollerPool::Instance().getPoller(false)->async([=]() mutable {
            addStreamPusherProxy(allArgs["schema"],
                                 allArgs["vhost"],
                                 allArgs["app"],
                                 allArgs["stream"],
                                 allArgs["dst_url"],
                                 retry_count,
                                 allArgs["rtp_type"],
                                 allArgs["timeout_sec"],
                                 args,
                                 [invoker, val, headerOut, dst_url](const SockException &ex, const string &key) mutable {
                                     if (ex) {
                                         val["code"] = API::OtherFailed;
                                         val["msg"] = ex.what();
                                     } else {
                                         val["data"]["key"] = key;
                                         InfoL << "Publish success, please play with player:" << dst_url;
                                     }
                                     invoker(200, headerOut, val.toStyledString());
                                 });
        });
    });

    // 关闭推流代理  [AUTO-TRANSLATED:91602b75]
    // Close the push stream proxy
    // 测试url http://127.0.0.1/index/api/delStreamPusherProxy?key=__defaultVhost__/proxy/0  [AUTO-TRANSLATED:2671206c]
    // Test url http://127.0.0.1/index/api/delStreamPusherProxy?key=__defaultVhost__/proxy/0
    api_regist("/index/api/delStreamPusherProxy", [](API_ARGS_MAP) {
        CHECK_SECRET();
        CHECK_ARGS("key");
        val["data"]["flag"] = s_pusher_proxy.erase(allArgs["key"]) == 1;
    });
    api_regist("/index/api/listStreamPusherProxy", [](API_ARGS_MAP) {
        CHECK_SECRET();
        s_pusher_proxy.for_each([&val](const std::string& key, const PusherProxy::Ptr& p) {
            Json::Value item = ToJson(p);
            item["key"] = key;
            val["data"].append(item);
        });
    });
    api_regist("/index/api/listStreamProxy", [](API_ARGS_MAP) {
        CHECK_SECRET();
        s_player_proxy.for_each([&val](const std::string& key, const PlayerProxy::Ptr& p) {
            Json::Value item = ToJson(p);
            item["key"] = key;
            val["data"].append(item);
        });
    });
    // 动态添加rtsp/rtmp拉流代理  [AUTO-TRANSLATED:2616537c]
    // Dynamically add rtsp/rtmp pull stream proxy
    // 测试url http://127.0.0.1/index/api/addStreamProxy?vhost=__defaultVhost__&app=proxy&enable_rtsp=1&enable_rtmp=1&stream=0&url=rtmp://127.0.0.1/live/obs  [AUTO-TRANSLATED:71ddce15]
    // Test url http://127.0.0.1/index/api/addStreamProxy?vhost=__defaultVhost__&app=proxy&enable_rtsp=1&enable_rtmp=1&stream=0&url=rtmp://127.0.0.1/live/obs
    api_regist("/index/api/addStreamProxy",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("vhost","app","stream","url");

        mINI args;
        for (auto &pr : allArgs.args) {
            args.emplace(pr.first, pr.second);
        }

        ProtocolOption option(allArgs);
        auto retry_count = allArgs["retry_count"].empty()? -1: allArgs["retry_count"].as<int>();

        std::string vhost = DEFAULT_VHOST;
        if (!allArgs["vhost"].empty()) {
            vhost = allArgs["vhost"];
        }
        auto tuple = MediaTuple { vhost, allArgs["app"], allArgs["stream"], "" };
        EventPollerPool::Instance().getPoller(false)->async([=]() mutable {
            addStreamProxy(tuple,
                           allArgs["url"],
                           retry_count,
                           option,
                           allArgs["rtp_type"],
                           allArgs["timeout_sec"],
                           args,
                           [invoker,val,headerOut](const SockException &ex,const string &key) mutable {
                               if (ex) {
                                   val["code"] = API::OtherFailed;
                                   val["msg"] = ex.what();
                               } else {
                                   val["data"]["key"] = key;
                               }
                               invoker(200, headerOut, val.toStyledString());
                           });
        });
    });

    // 关闭拉流代理  [AUTO-TRANSLATED:5204f128]
    // Close the pull stream proxy
    // 测试url http://127.0.0.1/index/api/delStreamProxy?key=__defaultVhost__/proxy/0  [AUTO-TRANSLATED:2b0903ef]
    // Test url http://127.0.0.1/index/api/delStreamProxy?key=__defaultVhost__/proxy/0
    api_regist("/index/api/delStreamProxy",[](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("key");
        val["data"]["flag"] = s_player_proxy.erase(allArgs["key"]) == 1;
    });

    static auto addFFmpegSource = [](const string &ffmpeg_cmd_key,
                                     const string &src_url,
                                     const string &dst_url,
                                     int timeout_ms,
                                     bool enable_hls,
                                     bool enable_mp4,
                                     const function<void(const SockException &ex, const string &key)> &cb) {
        auto key = MD5(dst_url).hexdigest();
        if (s_ffmpeg_src.find(key)) {
            // 已经在拉流了  [AUTO-TRANSLATED:e06c57d7]
            // Already pulling
            cb(SockException(Err_success), key);
            return;
        }

        auto ffmpeg = s_ffmpeg_src.make(key);

        ffmpeg->setOnClose([key]() {
            s_ffmpeg_src.erase(key);
        });
        ffmpeg->setupRecordFlag(enable_hls, enable_mp4);
        ffmpeg->play(ffmpeg_cmd_key, src_url, dst_url, timeout_ms, [cb, key](const SockException &ex) {
            if (ex) {
                s_ffmpeg_src.erase(key);
            }
            cb(ex, key);
        });
    };

    // 动态添加rtsp/rtmp拉流代理  [AUTO-TRANSLATED:2616537c]
    // Dynamically add rtsp/rtmp pull stream proxy
    // 测试url http://127.0.0.1/index/api/addFFmpegSource?src_url=http://live.hkstv.hk.lxdns.com/live/hks2/playlist.m3u8&dst_url=rtmp://127.0.0.1/live/hks2&timeout_ms=10000  [AUTO-TRANSLATED:501cdd89]
    // // Test url http://127.0.0.1/index/api/addFFmpegSource?src_url=http://live.hkstv.hk.lxdns.com/live/hks2/playlist.m3u8&dst_url=rtmp://127.0.0.1/live/hks2&timeout_ms=10000
    api_regist("/index/api/addFFmpegSource",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("src_url","dst_url","timeout_ms");
        auto src_url = allArgs["src_url"];
        auto dst_url = allArgs["dst_url"];
        int timeout_ms = allArgs["timeout_ms"];
        auto enable_hls = allArgs["enable_hls"].as<int>();
        auto enable_mp4 = allArgs["enable_mp4"].as<int>();

        addFFmpegSource(allArgs["ffmpeg_cmd_key"], src_url, dst_url, timeout_ms, enable_hls, enable_mp4,
                        [invoker, val, headerOut](const SockException &ex, const string &key) mutable{
            if (ex) {
                val["code"] = API::OtherFailed;
                val["msg"] = ex.what();
            } else {
                val["data"]["key"] = key;
            }
            invoker(200, headerOut, val.toStyledString());
        });
    });

    // 关闭拉流代理  [AUTO-TRANSLATED:5204f128]
    // Close the pull stream proxy
    // 测试url http://127.0.0.1/index/api/delFFmepgSource?key=key  [AUTO-TRANSLATED:ed6fa147]
    // Test url http://127.0.0.1/index/api/delFFmepgSource?key=key
    api_regist("/index/api/delFFmpegSource",[](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("key");
        val["data"]["flag"] = s_ffmpeg_src.erase(allArgs["key"]) == 1;
    });
    api_regist("/index/api/listFFmpegSource", [](API_ARGS_MAP) {
        CHECK_SECRET();
        s_ffmpeg_src.for_each([&val](const std::string& key, const FFmpegSource::Ptr& src) {
            Json::Value item;
            item["src_url"] = src->getSrcUrl();
            item["dst_url"] = src->getDstUrl();
            item["cmd"] = src->getCmd();
            item["ffmpeg_cmd_key"] = src->getCmdKey();
            item["key"] = key;
            val["data"].append(item);
        });
    });
    // 新增http api下载可执行程序文件接口  [AUTO-TRANSLATED:d6e44e84]
    // Add a new http api to download executable files
    // 测试url http://127.0.0.1/index/api/downloadBin  [AUTO-TRANSLATED:9525e834]
    // Test url http://127.0.0.1/index/api/downloadBin
    api_regist("/index/api/downloadBin",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        invoker.responseFile(allArgs.parser.getHeader(), StrCaseMap(), exePath());
    });

#if defined(ENABLE_RTPPROXY)
    api_regist("/index/api/getRtpInfo",[](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("stream_id");
        std::string vhost = DEFAULT_VHOST;
        if (!allArgs["vhost"].empty()) {
            vhost = allArgs["vhost"];
        }
        std::string app = kRtpAppName;
        if (!allArgs["app"].empty()) {
            app = allArgs["app"];
        }
        auto src = MediaSource::find(vhost, app, allArgs["stream_id"]);
        auto process = src ? src->getRtpProcess() : nullptr;
        if (!process) {
            val["exist"] = false;
            return;
        }
        val["exist"] = true;
        fillSockInfo(val, process.get());
    });

    api_regist("/index/api/openRtpServer",[](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("port", "stream_id");
        std::string vhost = DEFAULT_VHOST;
        if (!allArgs["vhost"].empty()) {
            vhost = allArgs["vhost"];
        }
        std::string app = kRtpAppName;
        if (!allArgs["app"].empty()) {
            app = allArgs["app"];
        }
        auto stream_id = allArgs["stream_id"];
        auto tuple = MediaTuple { vhost, app, stream_id, "" };
        auto tcp_mode = allArgs["tcp_mode"].as<int>();
        if (allArgs["enable_tcp"].as<int>() && !tcp_mode) {
            // 兼容老版本请求，新版本去除enable_tcp参数并新增tcp_mode参数  [AUTO-TRANSLATED:3b6a5ab5]
            // Compatible with old version requests, the new version removes the enable_tcp parameter and adds the tcp_mode parameter
            tcp_mode = 1;
        }
        auto only_track = allArgs["only_track"].as<int>();
        if (allArgs["only_audio"].as<bool>()) {
            // 兼容老版本请求，新版本去除only_audio参数并新增only_track参数  [AUTO-TRANSLATED:a7a40942]
            // Compatible with old version requests, the new version removes the only_audio parameter and adds the only_track parameter
            only_track = 1;
        }
        GET_CONFIG(std::string, local_ip, General::kListenIP)
        if (!allArgs["local_ip"].empty()) {
            local_ip = allArgs["local_ip"];
        }
        auto port = openRtpServer(allArgs["port"], tuple, tcp_mode, local_ip, allArgs["re_use_port"].as<bool>(),
                                  allArgs["ssrc"].as<uint32_t>(), only_track);
        if (port == 0) {
            throw InvalidArgsException("This stream already exists");
        }
        // 回复json  [AUTO-TRANSLATED:0c443c6a]
        // Reply json
        val["port"] = port;
    });

    api_regist("/index/api/openRtpServerMultiplex", [](API_ARGS_MAP) {
        CHECK_SECRET();
        CHECK_ARGS("port", "stream_id");
        std::string vhost = DEFAULT_VHOST;
        if (!allArgs["vhost"].empty()) {
            vhost = allArgs["vhost"];
        }
        std::string app = kRtpAppName;
        if (!allArgs["app"].empty()) {
            app = allArgs["app"];
        }
        auto stream_id = allArgs["stream_id"];
        auto tuple = MediaTuple { vhost, app, stream_id, "" };
        auto tcp_mode = allArgs["tcp_mode"].as<int>();
        if (allArgs["enable_tcp"].as<int>() && !tcp_mode) {
            // 兼容老版本请求，新版本去除enable_tcp参数并新增tcp_mode参数  [AUTO-TRANSLATED:b5f8f5df]
            // Compatible with old version requests, the new version removes the enable_tcp parameter and adds the tcp_mode parameter
            tcp_mode = 1;
        }
        auto only_track = allArgs["only_track"].as<int>();
        if (allArgs["only_audio"].as<bool>()) {
            // 兼容老版本请求，新版本去除only_audio参数并新增only_track参数  [AUTO-TRANSLATED:a7a40942]
            // Compatible with old version requests, the new version removes the only_audio parameter and adds the only_track parameter
            only_track = 1;
        }
        std::string local_ip = "::";
        if (!allArgs["local_ip"].empty()) {
            local_ip = allArgs["local_ip"];
        }

        auto port = openRtpServer(allArgs["port"], tuple, tcp_mode, local_ip, true, 0, only_track, true);
        if (port == 0) {
            throw InvalidArgsException("This stream already exists");
        }
        // 回复json  [AUTO-TRANSLATED:e80815cd]
        // Reply json
        val["port"] = port;
    });

    api_regist("/index/api/connectRtpServer", [](API_ARGS_MAP_ASYNC) {
        CHECK_SECRET();
        CHECK_ARGS("stream_id", "dst_url", "dst_port");
        auto cb = [val, headerOut, invoker](const SockException &ex) mutable {
            if (ex) {
                val["code"] = API::OtherFailed;
                val["msg"] = ex.what();
            }
            invoker(200, headerOut, val.toStyledString());
        };

        std::string vhost = DEFAULT_VHOST;
        if (!allArgs["vhost"].empty()) {
            vhost = allArgs["vhost"];
        }
        std::string app = kRtpAppName;
        if (!allArgs["app"].empty()) {
            app = allArgs["app"];
        }
        auto stream_id = allArgs["stream_id"];
        auto tuple = MediaTuple { vhost, app, stream_id, "" };
        auto server = s_rtp_server.find(tuple.shortUrl());
        if (!server) {
            cb(SockException(Err_other, "can not find the stream"));
            return;
        }
        server->connectToServer(allArgs["dst_url"], allArgs["dst_port"], cb);
    });

    api_regist("/index/api/closeRtpServer",[](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("stream_id");

        std::string vhost = DEFAULT_VHOST;
        if (!allArgs["vhost"].empty()) {
            vhost = allArgs["vhost"];
        }
        std::string app = kRtpAppName;
        if (!allArgs["app"].empty()) {
            app = allArgs["app"];
        }
        auto stream_id = allArgs["stream_id"];
        auto tuple = MediaTuple { vhost, app, stream_id, "" };
        if (s_rtp_server.erase(tuple.shortUrl()) == 0) {
            val["hit"] = 0;
            return;
        }
        val["hit"] = 1;
    });

    api_regist("/index/api/updateRtpServerSSRC",[](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("stream_id", "ssrc");

        std::string vhost = DEFAULT_VHOST;
        if (!allArgs["vhost"].empty()) {
            vhost = allArgs["vhost"];
        }
        std::string app = kRtpAppName;
        if (!allArgs["app"].empty()) {
            app = allArgs["app"];
        }
        auto stream_id = allArgs["stream_id"];
        auto tuple = MediaTuple { vhost, app, stream_id, "" };
        auto server = s_rtp_server.find(tuple.shortUrl());
        if (!server) {
            throw ApiRetException("RtpServer not found by stream_id", API::NotFound);
        }
        server->updateSSRC(allArgs["ssrc"]);
    });

    api_regist("/index/api/listRtpServer",[](API_ARGS_MAP){
        CHECK_SECRET();

        std::lock_guard<std::recursive_mutex> lck(s_rtp_server._mtx);
        for (auto &pr : s_rtp_server._map) {
            auto vec = split(pr.first, "/");
            Value obj;
            obj["vhost"] = vec[0];
            obj["app"] = vec[1];
            obj["stream_id"] = vec[2];
            auto& rtps = pr.second;
            obj["port"] = rtps->getPort();
            obj["ssrc"] = rtps->getSSRC();
            obj["tcp_mode"] = rtps->getTcpMode();
            obj["only_track"] = rtps->getOnlyTrack();
            val["data"].append(obj);
        }
    });

    static auto start_send_rtp = [] (bool passive, API_ARGS_MAP_ASYNC) {
        auto src = MediaSource::find(allArgs["vhost"], allArgs["app"], allArgs["stream"], allArgs["from_mp4"].as<int>());
        if (!src) {
            throw ApiRetException("can not find the source stream", API::NotFound);
        }
        auto type = allArgs["type"].empty() ? (int)MediaSourceEvent::SendRtpArgs::kRtpPS : allArgs["type"].as<int>();
        if (!allArgs["use_ps"].empty()) {
            // 兼容之前的use_ps参数  [AUTO-TRANSLATED:0193f489]
            // Compatible with the previous use_ps parameter
            type = allArgs["use_ps"].as<int>();
        }
        MediaSourceEvent::SendRtpArgs args;
        if (passive) {
            args.con_type = allArgs["is_udp"].as<bool>() ? mediakit::MediaSourceEvent::SendRtpArgs::kUdpPassive : mediakit::MediaSourceEvent::SendRtpArgs::kTcpPassive;
        } else {
            args.con_type = allArgs["is_udp"].as<bool>() ? mediakit::MediaSourceEvent::SendRtpArgs::kUdpActive : mediakit::MediaSourceEvent::SendRtpArgs::kTcpActive;
        }
        args.dst_url = allArgs["dst_url"];
        args.dst_port = allArgs["dst_port"];
        args.ssrc_multi_send = allArgs["ssrc_multi_send"].empty() ? false : allArgs["ssrc_multi_send"].as<bool>();
        args.ssrc = allArgs["ssrc"];
        args.src_port = allArgs["src_port"];
        args.pt = allArgs["pt"].empty() ? 96 : allArgs["pt"].as<int>();
        args.data_type = (MediaSourceEvent::SendRtpArgs::DataType)type;
        args.only_audio = allArgs["only_audio"].as<bool>();
        args.udp_rtcp_timeout = allArgs["udp_rtcp_timeout"];
        args.recv_stream_id = allArgs["recv_stream_id"];
        args.close_delay_ms = allArgs["close_delay_ms"];
        // 记录发送流的app和vhost  [AUTO-TRANSLATED:ee1b41d5]
        // Record the app and vhost of the sending stream
        args.recv_stream_app = allArgs["app"];
        args.recv_stream_vhost = allArgs["vhost"];
        src->getOwnerPoller()->async([=]() mutable {
            try {
                src->startSendRtp(args, [val, headerOut, invoker](uint16_t local_port, const SockException &ex) mutable {
                    if (ex) {
                        val["code"] = API::OtherFailed;
                        val["msg"] = ex.what();
                    }
                    val["local_port"] = local_port;
                    invoker(200, headerOut, val.toStyledString());
                });
            } catch (std::exception &ex) {
                val["code"] = API::Exception;
                val["msg"] = ex.what();
                invoker(200, headerOut, val.toStyledString());
            }
        });
    };

    api_regist("/index/api/startSendRtp",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream", "ssrc", "dst_url", "dst_port", "is_udp");
        start_send_rtp(false, API_ARGS_VALUE, invoker);
    });

    api_regist("/index/api/startSendRtpPassive",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream", "ssrc");
        start_send_rtp(true, API_ARGS_VALUE, invoker);
    });

    api_regist("/index/api/startSendRtpTalk",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream", "ssrc", "recv_stream_id");
        auto src = MediaSource::find(allArgs["vhost"], allArgs["app"], allArgs["stream"], allArgs["from_mp4"].as<int>());
        if (!src) {
            throw ApiRetException("can not find the source stream", API::NotFound);
        }
        MediaSourceEvent::SendRtpArgs args;
        args.con_type = mediakit::MediaSourceEvent::SendRtpArgs::kVoiceTalk;
        args.ssrc = allArgs["ssrc"];
        args.pt = allArgs["pt"].empty() ? 96 : allArgs["pt"].as<int>();
        args.data_type = allArgs["type"].empty() ? MediaSourceEvent::SendRtpArgs::kRtpPS : (MediaSourceEvent::SendRtpArgs::DataType)(allArgs["type"].as<int>());
        args.only_audio = allArgs["only_audio"].as<bool>();
        args.recv_stream_id = allArgs["recv_stream_id"];
        args.recv_stream_app = allArgs["app"];
        args.recv_stream_vhost = allArgs["vhost"];

        src->getOwnerPoller()->async([=]() mutable {
            try {
                src->startSendRtp(args, [val, headerOut, invoker](uint16_t local_port, const SockException &ex) mutable {
                    if (ex) {
                        val["code"] = API::OtherFailed;
                        val["msg"] = ex.what();
                    }
                    val["local_port"] = local_port;
                    invoker(200, headerOut, val.toStyledString());
                });
            } catch (std::exception &ex) {
                val["code"] = API::Exception;
                val["msg"] = ex.what();
                invoker(200, headerOut, val.toStyledString());
            }
        });
    });

    api_regist("/index/api/listRtpSender",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream");

        auto src = MediaSource::find(allArgs["vhost"], allArgs["app"], allArgs["stream"]);
        if (!src) {
            throw ApiRetException("can not find the source stream", API::NotFound);
        }

        auto muxer = src->getMuxer();
        CHECK(muxer, "get muxer from media source failed");

        src->getOwnerPoller()->async([=]() mutable {
            muxer->forEachRtpSender([&](const std::string &ssrc, const RtpSender &sender) mutable {
                val["data"].append(ssrc);
                val["bytesSpeed"] = (Json::UInt64)sender.getSendSpeed();
                val["totalBytes"] = (Json::UInt64)sender.getSendTotalBytes();
            });
            invoker(200, headerOut, val.toStyledString());
        });
    });

    api_regist("/index/api/stopSendRtp",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream");

        auto src = MediaSource::find(allArgs["vhost"], allArgs["app"], allArgs["stream"]);
        if (!src) {
            throw ApiRetException("can not find the stream", API::NotFound);
        }

        src->getOwnerPoller()->async([=]() mutable {
            // ssrc如果为空，关闭全部  [AUTO-TRANSLATED:e0955dab]
            // If ssrc is empty, close all
            if (!src->stopSendRtp(allArgs["ssrc"])) {
                val["code"] = API::OtherFailed;
                val["msg"] = "stopSendRtp failed";
                invoker(200, headerOut, val.toStyledString());
                return;
            }
            invoker(200, headerOut, val.toStyledString());
        });
    });

    api_regist("/index/api/pauseRtpCheck", [](API_ARGS_MAP) {
        CHECK_SECRET();
        CHECK_ARGS("stream_id");
        std::string vhost = DEFAULT_VHOST;
        if (!allArgs["vhost"].empty()) {
            vhost = allArgs["vhost"];
        }
        std::string app = kRtpAppName;
        if (!allArgs["app"].empty()) {
            app = allArgs["app"];
        }
        // 只是暂停流的检查，流媒体服务器做为流负载服务，收流就转发，RTSP/RTMP有自己暂停协议  [AUTO-TRANSLATED:dda6ee31]
        // Only pause the stream check, the media server acts as a stream load balancing service, receiving the stream and forwarding it, RTSP/RTMP has its own pause protocol
        auto src = MediaSource::find(vhost, app, allArgs["stream_id"]);
        auto process = src ? src->getRtpProcess() : nullptr;
        if (process) {
            process->setStopCheckRtp(true);
        } else {
            val["code"] = API::NotFound;
        }
    });

    api_regist("/index/api/resumeRtpCheck", [](API_ARGS_MAP) {
        CHECK_SECRET();
        CHECK_ARGS("stream_id");
        std::string vhost = DEFAULT_VHOST;
        if (!allArgs["vhost"].empty()) {
            vhost = allArgs["vhost"];
        }
        std::string app = kRtpAppName;
        if (!allArgs["app"].empty()) {
            app = allArgs["app"];
        }
        auto src = MediaSource::find(vhost, app, allArgs["stream_id"]);
        auto process = src ? src->getRtpProcess() : nullptr;
        if (process) {
            process->setStopCheckRtp(false);
        } else {
            val["code"] = API::NotFound;
        }
    });

#endif//ENABLE_RTPPROXY

    // 开始录制hls或MP4  [AUTO-TRANSLATED:0818775e]
    // Start recording hls or MP4
    api_regist("/index/api/startRecord",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("type","vhost","app","stream");

        auto src = MediaSource::find(allArgs["vhost"], allArgs["app"], allArgs["stream"] );
        if (!src) {
            throw ApiRetException("can not find the stream", API::NotFound);
        }

        src->getOwnerPoller()->async([=]() mutable {
            auto result = src->setupRecord((Recorder::type)allArgs["type"].as<int>(), true, allArgs["customized_path"], allArgs["max_second"].as<size_t>());
            val["result"] = result;
            val["code"] = result ? API::Success : API::OtherFailed;
            val["msg"] = result ? "success" :  "start record failed";
            invoker(200, headerOut, val.toStyledString());
        });
    });

    // 设置录像流播放速度  [AUTO-TRANSLATED:a8d82298]
    // Set the playback speed of the recording stream
    api_regist("/index/api/setRecordSpeed", [](API_ARGS_MAP_ASYNC) {
        CHECK_SECRET();
        CHECK_ARGS("schema", "vhost", "app", "stream", "speed");
        auto src = MediaSource::find(allArgs["schema"],
                                     allArgs["vhost"],
                                     allArgs["app"],
                                     allArgs["stream"]);
        if (!src) {
            throw ApiRetException("can not find the stream", API::NotFound);
        }

        auto speed = allArgs["speed"].as<float>();
        src->getOwnerPoller()->async([=]() mutable {
            bool flag = src->speed(speed);
            val["result"] = flag ? 0 : -1;
            val["msg"] = flag ? "success" : "set failed";
            val["code"] = flag ? API::Success : API::OtherFailed;
            invoker(200, headerOut, val.toStyledString());
        });
    });

    api_regist("/index/api/seekRecordStamp", [](API_ARGS_MAP_ASYNC) {
        CHECK_SECRET();
        CHECK_ARGS("schema", "vhost", "app", "stream", "stamp");
        auto src = MediaSource::find(allArgs["schema"],
                                     allArgs["vhost"],
                                     allArgs["app"],
                                     allArgs["stream"]);
        if (!src) {
            throw ApiRetException("can not find the stream", API::NotFound);
        }

        auto stamp = allArgs["stamp"].as<size_t>();
        src->getOwnerPoller()->async([=]() mutable {
            bool flag = src->seekTo(stamp);
            val["result"] = flag ? 0 : -1;
            val["msg"] = flag ? "success" : "seek failed";
            val["code"] = flag ? API::Success : API::OtherFailed;
            invoker(200, headerOut, val.toStyledString());
        });
    });

    // 停止录制hls或MP4  [AUTO-TRANSLATED:24d11a0c]
    // Stop recording hls or MP4
    api_regist("/index/api/stopRecord",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("type","vhost","app","stream");

        auto src = MediaSource::find(allArgs["vhost"], allArgs["app"], allArgs["stream"] );
        if (!src) {
            throw ApiRetException("can not find the stream", API::NotFound);
        }

        auto type = (Recorder::type)allArgs["type"].as<int>();
        src->getOwnerPoller()->async([=]() mutable {
            auto result = src->setupRecord(type, false, "", 0);
            val["result"] = result;
            val["code"] = result ? API::Success : API::OtherFailed;
            val["msg"] = result ? "success" : "stop record failed";
            invoker(200, headerOut, val.toStyledString());
        });
    });

    // 获取hls或MP4录制状态  [AUTO-TRANSLATED:a08a2f1a]
    // Get the recording status of hls or MP4
    api_regist("/index/api/isRecording",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("type","vhost","app","stream");

        auto src = MediaSource::find(allArgs["vhost"], allArgs["app"], allArgs["stream"]);
        if (!src) {
            throw ApiRetException("can not find the stream", API::NotFound);
        }

        auto type = (Recorder::type)allArgs["type"].as<int>();
        src->getOwnerPoller()->async([=]() mutable {
            val["status"] = src->isRecording(type);
            invoker(200, headerOut, val.toStyledString());
        });
    });

    api_regist("/index/api/getProxyPusherInfo", [](API_ARGS_MAP_ASYNC) {
        CHECK_SECRET();
        CHECK_ARGS("key");
        auto pusher = s_pusher_proxy.find(allArgs["key"]);
        if (!pusher) {
            throw ApiRetException("can not find pusher", API::NotFound);
        }

        val["data"] = ToJson(pusher);
        invoker(200, headerOut, val.toStyledString());
    });

    api_regist("/index/api/getProxyInfo", [](API_ARGS_MAP_ASYNC) {
        CHECK_SECRET();
        CHECK_ARGS("key");
        auto proxy = s_player_proxy.find(allArgs["key"]);
        if (!proxy) {
            throw ApiRetException("can not find the proxy", API::NotFound);
        }

        val["data"] = ToJson(proxy);
        invoker(200, headerOut, val.toStyledString());
    });

    // 删除录像文件夹  [AUTO-TRANSLATED:821aed07]
    // Delete the recording folder
    // http://127.0.0.1/index/api/deleteRecordDirectroy?vhost=__defaultVhost__&app=live&stream=ss&period=2020-01-01
    api_regist("/index/api/deleteRecordDirectory", [](API_ARGS_MAP) {
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream", "period");
        auto tuple = MediaTuple{allArgs["vhost"], allArgs["app"], allArgs["stream"], ""};
        auto record_path = Recorder::getRecordPath(Recorder::type_mp4, tuple, allArgs["customized_path"]);
        auto period = allArgs["period"];
        record_path = record_path + period + "/";

        bool recording = false;
        auto name = allArgs["name"];
        if (!name.empty()) {
            // 删除指定文件  [AUTO-TRANSLATED:e8ee7bfa]
            // Delete the specified file
            record_path += name;
        } else {
            // 删除文件夹，先判断该流是否正在录制中  [AUTO-TRANSLATED:9f124786]
            // Delete the folder, first check if the stream is being recorded
            auto src = MediaSource::find(allArgs["vhost"], allArgs["app"], allArgs["stream"]);
            if (src && src->isRecording(Recorder::type_mp4)) {
                recording = true;
            }
        }
        val["path"] = record_path;
        if (!recording) {
            val["code"] = File::delete_file(record_path, true);
            return;
        }
        File::scanDir(record_path, [](const string &path, bool is_dir) {
            if (is_dir) {
                return true;
            }
            if (path.find("/.") == std::string::npos) {
                File::delete_file(path);
            } else {
                TraceL << "Ignore tmp mp4 file: " << path;
            }
            return true;
        }, true, true);
        File::deleteEmptyDir(record_path);
    });

    // 获取录像文件夹列表或mp4文件列表  [AUTO-TRANSLATED:f7e299bc]
    // Get the list of recording folders or mp4 files
    //http://127.0.0.1/index/api/getMP4RecordFile?vhost=__defaultVhost__&app=live&stream=ss&period=2020-01
    api_regist("/index/api/getMP4RecordFile", [](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream");
        auto tuple = MediaTuple{allArgs["vhost"], allArgs["app"], allArgs["stream"], ""};
        auto record_path = Recorder::getRecordPath(Recorder::type_mp4, tuple, allArgs["customized_path"]);
        auto period = allArgs["period"];

        // 判断是获取mp4文件列表还是获取文件夹列表  [AUTO-TRANSLATED:b9c86d2f]
        // Determine whether to get the mp4 file list or the folder list
        bool search_mp4 = period.size() == sizeof("2020-02-01") - 1;
        if (search_mp4) {
            record_path = record_path + period + "/";
        }

        Json::Value paths(arrayValue);
        // 这是筛选日期，获取文件夹列表  [AUTO-TRANSLATED:786fa49d]
        // This is to filter the date and get the folder list
        File::scanDir(record_path, [&](const string &path, bool isDir) {
            auto pos = path.rfind('/');
            if (pos != string::npos) {
                string relative_path = path.substr(pos + 1);
                if (search_mp4) {
                    if (!isDir) {
                        // 我们只收集mp4文件，对文件夹不感兴趣  [AUTO-TRANSLATED:254d9f25]
                        // We only collect mp4 files, we are not interested in folders
                        paths.append(relative_path);
                    }
                } else if (isDir && relative_path.find(period) == 0) {
                    // 匹配到对应日期的文件夹  [AUTO-TRANSLATED:cd3d10b9]
                    // Match the folder for the corresponding date
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
                                  const HttpSession::HttpResponseInvoker &invoker,
                                  const string &err_msg = "") {
        static bool s_snap_success_once = false;
        StrCaseMap headerOut;
        GET_CONFIG(string, defaultSnap, API::kDefaultSnap);
        if (!File::fileSize(snap_path)) {
            if (!err_msg.empty() && (!s_snap_success_once || defaultSnap.empty())) {
                // 重来没截图成功过或者默认截图图片为空，那么直接返回FFmpeg错误日志  [AUTO-TRANSLATED:5bde510f]
                // If the screenshot has never been successful or the default screenshot image is empty, then directly return the FFmpeg error log
                headerOut["Content-Type"] = HttpFileManager::getContentType(".txt");
                invoker.responseFile(headerIn, headerOut, err_msg, false, false);
                return;
            }
            // 截图成功过一次，那么认为配置无错误，截图失败时，返回预设默认图片  [AUTO-TRANSLATED:ffe4d807]
            // If the screenshot has been successful once, then it is considered that the configuration is error-free, and when the screenshot fails, the preset default image is returned
            const_cast<string &>(snap_path) = File::absolutePath("", defaultSnap);
            headerOut["Content-Type"] = HttpFileManager::getContentType(snap_path.data());
        } else {
            s_snap_success_once = true;
            // 之前生成的截图文件，我们默认为jpeg格式  [AUTO-TRANSLATED:5cc5c1ff]
            // The previously generated screenshot file, we default to jpeg format
            headerOut["Content-Type"] = HttpFileManager::getContentType(".jpeg");
        }
        // 返回图片给http客户端  [AUTO-TRANSLATED:58a1f64e]
        // Return image to http client
        invoker.responseFile(headerIn, headerOut, snap_path);
    };

    // 获取截图缓存或者实时截图  [AUTO-TRANSLATED:78e2fe1e]
    // Get screenshot cache or real-time screenshot
    //http://127.0.0.1/index/api/getSnap?url=rtmp://127.0.0.1/record/robot.mp4&timeout_sec=10&expire_sec=3
    api_regist("/index/api/getSnap", [](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        CHECK_ARGS("url", "timeout_sec", "expire_sec");
        GET_CONFIG(string, snap_root, API::kSnapRoot);

        bool have_old_snap = false, res_old_snap = false;
        int expire_sec = allArgs["expire_sec"];
        auto scan_path = File::absolutePath(MD5(allArgs["url"]).hexdigest(), snap_root) + "/";
        string new_snap = StrPrinter << scan_path << time(NULL) << ".jpeg";

        File::scanDir(scan_path, [&](const string &path, bool isDir) {
            if (isDir || !end_with(path, ".jpeg")) {
                // 忽略文件夹或其他类型的文件  [AUTO-TRANSLATED:3ecffcae]
                // Ignore folders or other types of files
                return true;
            }

            // 找到截图  [AUTO-TRANSLATED:b784cfec]
            // Find screenshot
            auto tm = findSubString(path.data() + scan_path.size(), nullptr, ".jpeg");
            if (atoll(tm.data()) + expire_sec < time(NULL)) {
                // 截图已经过期，改名，以便再次请求时，可以返回老截图  [AUTO-TRANSLATED:94fac79b]
                // Screenshot has expired, rename it so that it can be returned when requested again
                rename(path.data(), new_snap.data());
                have_old_snap = true;
                return true;
            }

            // 截图存在，且未过期，那么返回之  [AUTO-TRANSLATED:6f53d3d1]
            // Screenshot exists and has not expired, so return it
            res_old_snap = true;
            responseSnap(path, allArgs.parser.getHeader(), invoker);
            // 中断遍历  [AUTO-TRANSLATED:7893aab3]
            // Interrupt traversal
            return false;
        });

        if (res_old_snap) {
            // 已经回复了旧的截图  [AUTO-TRANSLATED:9051a3e6]
            // Old screenshot has been replied
            return;
        }

        // 无截图或者截图已经过期  [AUTO-TRANSLATED:89c46415]
        // No screenshot or screenshot has expired
        if (!have_old_snap) {
            // 无过期截图，生成一个空文件，目的是顺便创建文件夹路径  [AUTO-TRANSLATED:bdbfdbcb]
            // No expired screenshot, generate an empty file, the purpose is to create the folder path by the way
            // 同时防止在FFmpeg生成截图途中不停的尝试调用该api多次启动FFmpeg进程  [AUTO-TRANSLATED:a04e1ee2]
            // At the same time, prevent the FFmpeg process from being started multiple times by continuously trying to call this API during the FFmpeg screenshot generation process
            auto file = File::create_file(new_snap, "wb");
            if (file) {
                fclose(file);
            }
        }

        // 启动FFmpeg进程，开始截图，生成临时文件，截图成功后替换为正式文件  [AUTO-TRANSLATED:7d589e3f]
        // Start the FFmpeg process, start taking screenshots, generate temporary files, replace them with formal files after successful screenshots
        auto new_snap_tmp = new_snap + ".tmp";
        FFmpegSnap::makeSnap(allArgs["async"], allArgs["url"], new_snap_tmp, allArgs["timeout_sec"], [invoker, allArgs, new_snap, new_snap_tmp](bool success, const string &err_msg) {
            if (!success) {
                // 生成截图失败，可能残留空文件  [AUTO-TRANSLATED:c96a4468]
                // Screenshot generation failed, there may be residual empty files
                File::delete_file(new_snap_tmp);
            } else {
                // 临时文件改成正式文件  [AUTO-TRANSLATED:eca24dfd]
                // Temporary file changed to formal file
                File::delete_file(new_snap);
                rename(new_snap_tmp.data(), new_snap.data());
            }
            responseSnap(new_snap, allArgs.parser.getHeader(), invoker, err_msg);
        });
    });

    api_regist("/index/api/getStatistic",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        getStatisticJson([headerOut, val, invoker](const Value &data) mutable{
            val["data"] = data;
            invoker(200, headerOut, val.toStyledString());
        });
    });

#ifdef ENABLE_WEBRTC
    class WebRtcArgsImp : public WebRtcArgs {
    public:
        WebRtcArgsImp(const ArgsString &args, std::string session_id)
            : _args(args)
            , _session_id(std::move(session_id)) {}
        ~WebRtcArgsImp() override = default;

        toolkit::variant operator[](const string &key) const override {
            if (key == "url") {
                return getUrl();
            }
            return _args[key];
        }

    private:
        string getUrl() const{
            auto &allArgs = _args;
            CHECK_ARGS("app", "stream");

            string auth = _args["Authorization"]; // Authorization  Bearer
            return StrPrinter << "rtc://" << _args["Host"] << "/" << _args["app"] << "/" << _args["stream"] << "?"
                              << _args.parser.params() + "&session=" + _session_id + (auth.empty() ? "" : ("&Authorization=" + auth));
        }

    private:
        ArgsString _args;
        std::string _session_id;
    };

    api_regist("/index/api/webrtc",[](API_ARGS_STRING_ASYNC){
        CHECK_ARGS("type");
        auto type = allArgs["type"];
        auto offer = allArgs.args;
        CHECK(!offer.empty(), "http body(webrtc offer sdp) is empty");

        auto &session = static_cast<Session&>(sender);
        auto args = std::make_shared<WebRtcArgsImp>(allArgs, sender.getIdentifier());
        WebRtcPluginManager::Instance().negotiateSdp(session, type, *args, [invoker, val, offer, headerOut](const WebRtcInterface &exchanger) mutable {
            auto &handler = const_cast<WebRtcInterface &>(exchanger);
            try {
                val["sdp"] = handler.getAnswerSdp(offer);
                val["id"] = exchanger.getIdentifier();
                val["type"] = "answer";
                invoker(200, headerOut, val.toStyledString());
            } catch (std::exception &ex) {
                val["code"] = API::Exception;
                val["msg"] = ex.what();
                invoker(200, headerOut, val.toStyledString());
            }
        });
    });

    static constexpr char delete_webrtc_url [] = "/index/api/delete_webrtc";
    static auto whip_whep_func = [](const char *type, API_ARGS_STRING_ASYNC) {
        auto offer = allArgs.args;
        CHECK(!offer.empty(), "http body(webrtc offer sdp) is empty");

        auto &session = static_cast<Session&>(sender);
        auto location = std::string(session.overSsl() ? "https://" : "http://") + allArgs["host"] + delete_webrtc_url;
        auto args = std::make_shared<WebRtcArgsImp>(allArgs, sender.getIdentifier());
        WebRtcPluginManager::Instance().negotiateSdp(session, type, *args, [invoker, offer, headerOut, location](const WebRtcInterface &exchanger) mutable {
            auto &handler = const_cast<WebRtcInterface &>(exchanger);
            try {
                // 设置返回类型  [AUTO-TRANSLATED:ffc2a31a]
                // Set return type
                headerOut["Content-Type"] = "application/sdp";
                headerOut["Location"] = location + "?id=" + exchanger.getIdentifier() + "&token=" + exchanger.deleteRandStr();
                invoker(201, headerOut, handler.getAnswerSdp(offer));
            } catch (std::exception &ex) {
                headerOut["Content-Type"] = "text/plain";
                invoker(406, headerOut, ex.what());
            }
        });
    };

    api_regist("/index/api/whip", [](API_ARGS_STRING_ASYNC) { whip_whep_func("push", API_ARGS_VALUE, invoker); });
    api_regist("/index/api/whep", [](API_ARGS_STRING_ASYNC) { whip_whep_func("play", API_ARGS_VALUE, invoker); });

    api_regist(delete_webrtc_url, [](API_ARGS_MAP_ASYNC) {
        CHECK_ARGS("id", "token");
        CHECK(allArgs.parser.method() == "DELETE", "http method is not DELETE: " + allArgs.parser.method());
        auto obj = WebRtcTransportManager::Instance().getItem(allArgs["id"]);
        if (!obj) {
            invoker(404, headerOut, "id not found");
            return;
        }
        if (obj->deleteRandStr() != allArgs["token"]) {
            invoker(401, headerOut, "token incorrect");
            return;
        }
        obj->safeShutdown(SockException(Err_shutdown, "deleted by http api"));
        invoker(200, headerOut, "");
    });
#endif

#if defined(ENABLE_VERSION)
    api_regist("/index/api/version",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        Value ver;
        ver["buildTime"] = BUILD_TIME;
        ver["branchName"] = BRANCH_NAME;
        ver["commitHash"] = COMMIT_HASH;
        val["data"] = ver;
        invoker(200, headerOut, val.toStyledString());
    });
#endif

#if ENABLE_MP4
    api_regist("/index/api/loadMP4File", [](API_ARGS_MAP) {
        CHECK_SECRET();
        CHECK_ARGS("vhost", "app", "stream", "file_path");

        ProtocolOption option;
        // mp4支持多track  [AUTO-TRANSLATED:b9688762]
        // mp4 supports multiple tracks
        option.max_track = 16;
        // 默认解复用mp4不生成mp4  [AUTO-TRANSLATED:11f2dcee]
        // By default, demultiplexing mp4 does not generate mp4
        option.enable_mp4 = false;
        // 但是如果参数明确指定开启mp4, 那么也允许之  [AUTO-TRANSLATED:b143a9e3]
        // But if the parameter explicitly specifies to enable mp4, then it is also allowed
        option.load(allArgs);
        // 强制无人观看时自动关闭  [AUTO-TRANSLATED:f7c85948]
        // Force automatic shutdown when no one is watching
        option.auto_close = true;
        auto tuple = MediaTuple{allArgs["vhost"], allArgs["app"], allArgs["stream"], ""};
        auto reader = std::make_shared<MP4Reader>(tuple, allArgs["file_path"], option);
        // sample_ms设置为0，从配置文件加载；file_repeat可以指定，如果配置文件也指定循环解复用，那么强制开启  [AUTO-TRANSLATED:23e826b4]
        // sample_ms is set to 0, loaded from the configuration file; file_repeat can be specified, if the configuration file also specifies loop demultiplexing, then force it to be enabled
        reader->startReadMP4(0, true, allArgs["file_repeat"]);
        val["data"]["duration_ms"] = (Json::UInt64)reader->getDemuxer()->getDurationMS();
    });
#endif

    GET_CONFIG_FUNC(std::set<std::string>, download_roots, API::kDownloadRoot, [](const string &str) -> std::set<std::string> {
        std::set<std::string> ret;
        auto vec = toolkit::split(str, ";");
        for (auto &item : vec) {
            auto root = File::absolutePath("", item, true);
            ret.emplace(std::move(root));
        }
        return ret;
    });

    api_regist("/index/api/downloadFile", [](API_ARGS_MAP_ASYNC) {
        CHECK_ARGS("file_path");
        auto file_path = allArgs["file_path"];

        if (file_path.find("..") != std::string::npos) {
            invoker(401, StrCaseMap{}, "You can not access parent directory");
            return;
        }
        bool safe = false;
        for (auto &root : download_roots) {
            if (start_with(file_path, root)) {
                safe = true;
                break;
            }
        }
        if (!safe) {
            invoker(401, StrCaseMap{}, "You can not download files outside the root directory");
            return;
        }

        // 通过on_http_access完成文件下载鉴权，请务必确认访问鉴权url参数以及访问文件路径是否合法  [AUTO-TRANSLATED:73507988]
        // File download authentication is completed through on_http_access. Please make sure that the access authentication URL parameters and the access file path are legal
        HttpSession::HttpAccessPathInvoker file_invoker = [allArgs, invoker](const string &err_msg, const string &cookie_path_in, int life_second) mutable {
            if (!err_msg.empty()) {
                invoker(401, StrCaseMap{}, err_msg);
            } else {
                StrCaseMap res_header;
                auto save_name = allArgs["save_name"];
                if (!save_name.empty()) {
                    res_header.emplace("Content-Disposition", "attachment;filename=\"" + save_name + "\"");
                }
                invoker.responseFile(allArgs.parser.getHeader(), res_header, allArgs["file_path"]);
            }
        };

        bool flag = NOTICE_EMIT(BroadcastHttpAccessArgs, Broadcast::kBroadcastHttpAccess, allArgs.parser, file_path, false, file_invoker, sender);
        if (!flag) {
            // 文件下载鉴权事件无人监听，不允许下载  [AUTO-TRANSLATED:5e02f0ce]
            // No one is listening to the file download authentication event, download is not allowed
            invoker(401, StrCaseMap {}, "None http access event listener");
        }
    });

#if defined(ENABLE_VIDEOSTACK) && defined(ENABLE_X264) && defined(ENABLE_FFMPEG)
    VideoStackManager::Instance().loadBgImg("novideo.yuv");
    NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastStreamNoneReader, [](BroadcastStreamNoneReaderArgs) {
        auto id = sender.getMediaTuple().stream;
        VideoStackManager::Instance().stopVideoStack(id);
    });

    api_regist("/index/api/stack/start", [](API_ARGS_JSON_ASYNC) {
        CHECK_SECRET();
        int ret = 0;
        try {
            ret = VideoStackManager::Instance().startVideoStack(allArgs.args);
            val["code"] = ret;
            val["msg"] = ret ? "failed" : "success";
        } catch (const std::exception &e) {
            val["code"] = -1;
            val["msg"] = e.what();
        }
        invoker(200, headerOut, val.toStyledString());
    });

    api_regist("/index/api/stack/reset", [](API_ARGS_JSON_ASYNC) {
        CHECK_SECRET();
        int ret = 0;
        try {
            auto ret = VideoStackManager::Instance().resetVideoStack(allArgs.args);
            val["code"] = ret;
            val["msg"] = ret ? "failed" : "success";
        } catch (const std::exception &e) {
            val["code"] = -1;
            val["msg"] = e.what();
        }
        invoker(200, headerOut, val.toStyledString());
    });

    api_regist("/index/api/stack/stop", [](API_ARGS_MAP_ASYNC) {
        CHECK_SECRET();
        CHECK_ARGS("id");
        auto ret = VideoStackManager::Instance().stopVideoStack(allArgs["id"]);
        val["code"] = ret;
        val["msg"] = ret ? "failed" : "success";
        invoker(200, headerOut, val.toStyledString());
    });
#endif
}

void unInstallWebApi(){
    s_player_proxy.clear();
    s_ffmpeg_src.clear();
    s_pusher_proxy.clear();
#if defined(ENABLE_RTPPROXY)
    s_rtp_server.clear();
#endif
#if defined(ENABLE_VIDEOSTACK) && defined(ENABLE_FFMPEG) && defined(ENABLE_X264)
    VideoStackManager::Instance().clear();
#endif

    NoticeCenter::Instance().delListener(&web_api_tag);
}
