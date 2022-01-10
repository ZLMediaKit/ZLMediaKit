/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include "Common/config.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"

using namespace toolkit;

namespace mediakit {

bool loadIniConfig(const char *ini_path){
    string ini;
    if(ini_path && ini_path[0] != '\0'){
        ini = ini_path;
    }else{
        ini = exePath() + ".ini";
    }
    try{
        mINI::Instance().parseFile(ini);
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastReloadConfig);
        return true;
    }catch (std::exception &) {
        InfoL << "dump ini file to:" << ini;
        mINI::Instance().dumpFile(ini);
        return false;
    }
}
////////////广播名称///////////
namespace Broadcast {
const string kBroadcastMediaChanged = "kBroadcastMediaChanged";
const string kBroadcastRecordMP4 = "kBroadcastRecordMP4";
const string kBroadcastRecordTs = "kBroadcastRecoredTs";
const string kBroadcastHttpRequest = "kBroadcastHttpRequest";
const string kBroadcastHttpAccess = "kBroadcastHttpAccess";
const string kBroadcastOnGetRtspRealm = "kBroadcastOnGetRtspRealm";
const string kBroadcastOnRtspAuth = "kBroadcastOnRtspAuth";
const string kBroadcastMediaPlayed = "kBroadcastMediaPlayed";
const string kBroadcastMediaPublish = "kBroadcastMediaPublish";
const string kBroadcastFlowReport = "kBroadcastFlowReport";
const string kBroadcastReloadConfig = "kBroadcastReloadConfig";
const string kBroadcastShellLogin = "kBroadcastShellLogin";
const string kBroadcastNotFoundStream = "kBroadcastNotFoundStream";
const string kBroadcastStreamNoneReader = "kBroadcastStreamNoneReader";
const string kBroadcastHttpBeforeAccess = "kBroadcastHttpBeforeAccess";
} //namespace Broadcast

//通用配置项目
namespace General{
#define GENERAL_FIELD "general."
const string kMediaServerId = GENERAL_FIELD"mediaServerId";
const string kFlowThreshold = GENERAL_FIELD"flowThreshold";
const string kStreamNoneReaderDelayMS = GENERAL_FIELD"streamNoneReaderDelayMS";
const string kMaxStreamWaitTimeMS = GENERAL_FIELD"maxStreamWaitMS";
const string kEnableVhost = GENERAL_FIELD"enableVhost";
const string kAddMuteAudio = GENERAL_FIELD"addMuteAudio";
const string kResetWhenRePlay = GENERAL_FIELD"resetWhenRePlay";
const string kPublishToHls = GENERAL_FIELD"publishToHls";
const string kPublishToMP4 = GENERAL_FIELD"publishToMP4";
const string kMergeWriteMS = GENERAL_FIELD"mergeWriteMS";
const string kModifyStamp = GENERAL_FIELD"modifyStamp";
const string kHlsDemand = GENERAL_FIELD"hls_demand";
const string kRtspDemand = GENERAL_FIELD"rtsp_demand";
const string kRtmpDemand = GENERAL_FIELD"rtmp_demand";
const string kTSDemand = GENERAL_FIELD"ts_demand";
const string kFMP4Demand = GENERAL_FIELD"fmp4_demand";
const string kEnableAudio = GENERAL_FIELD"enable_audio";
const string kWaitTrackReadyMS = GENERAL_FIELD"wait_track_ready_ms";
const string kWaitAddTrackMS = GENERAL_FIELD"wait_add_track_ms";
const string kUnreadyFrameCache = GENERAL_FIELD"unready_frame_cache";
const string kContinuePushMS = GENERAL_FIELD"continue_push_ms";

onceToken token([](){
    mINI::Instance()[kFlowThreshold] = 1024;
    mINI::Instance()[kStreamNoneReaderDelayMS] = 20 * 1000;
    mINI::Instance()[kMaxStreamWaitTimeMS] = 15 * 1000;
    mINI::Instance()[kEnableVhost] = 0;
    mINI::Instance()[kAddMuteAudio] = 1;
    mINI::Instance()[kResetWhenRePlay] = 1;
    mINI::Instance()[kPublishToHls] = 1;
    mINI::Instance()[kPublishToMP4] = 0;
    mINI::Instance()[kMergeWriteMS] = 0;
    mINI::Instance()[kModifyStamp] = 0;
    mINI::Instance()[kMediaServerId] = makeRandStr(16);
    mINI::Instance()[kHlsDemand] = 0;
    mINI::Instance()[kRtspDemand] = 0;
    mINI::Instance()[kRtmpDemand] = 0;
    mINI::Instance()[kTSDemand] = 0;
    mINI::Instance()[kFMP4Demand] = 0;
    mINI::Instance()[kEnableAudio] = 1;
    mINI::Instance()[kWaitTrackReadyMS] = 10000;
    mINI::Instance()[kWaitAddTrackMS] = 3000;
    mINI::Instance()[kUnreadyFrameCache] = 100;
    mINI::Instance()[kContinuePushMS] = 15 * 1000;
},nullptr);

}//namespace General

////////////HTTP配置///////////
namespace Http {
#define HTTP_FIELD "http."
//http 文件发送缓存大小
const string kSendBufSize = HTTP_FIELD"sendBufSize";
//http 最大请求字节数
const string kMaxReqSize = HTTP_FIELD"maxReqSize";
//http keep-alive秒数
const string kKeepAliveSecond = HTTP_FIELD"keepAliveSecond";
//http 字符编码
const string kCharSet = HTTP_FIELD"charSet";
//http 服务器根目录
const string kRootPath = HTTP_FIELD"rootPath";
//http 服务器虚拟目录
const string kVirtualPath = HTTP_FIELD "virtualPath";
//http 404错误提示内容
const string kNotFound = HTTP_FIELD"notFound";
//是否显示文件夹菜单
const string kDirMenu = HTTP_FIELD"dirMenu";

onceToken token([](){
    mINI::Instance()[kSendBufSize] = 64 * 1024;
    mINI::Instance()[kMaxReqSize] = 4 * 10240;
    mINI::Instance()[kKeepAliveSecond] = 15;
    mINI::Instance()[kDirMenu] = true;
    mINI::Instance()[kVirtualPath] = "";

#if defined(_WIN32)
    mINI::Instance()[kCharSet] = "gb2312";
#else
    mINI::Instance()[kCharSet] ="utf-8";
#endif

    mINI::Instance()[kRootPath] = "./www";
    mINI::Instance()[kNotFound] = StrPrinter << "<html>"
                                                "<head><title>404 Not Found</title></head>"
                                                "<body bgcolor=\"white\">"
                                                "<center><h1>您访问的资源不存在！</h1></center>"
                                                "<hr><center>"
                                             << kServerName
                                             << "</center>"
                                                "</body>"
                                                "</html>"
                                             << endl;
},nullptr);

}//namespace Http

////////////SHELL配置///////////
namespace Shell {
#define SHELL_FIELD "shell."
const string kMaxReqSize = SHELL_FIELD"maxReqSize";

onceToken token([](){
    mINI::Instance()[kMaxReqSize] = 1024;
},nullptr);
} //namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {
#define RTSP_FIELD "rtsp."
const string kAuthBasic = RTSP_FIELD"authBasic";
const string kHandshakeSecond = RTSP_FIELD"handshakeSecond";
const string kKeepAliveSecond = RTSP_FIELD"keepAliveSecond";
const string kDirectProxy = RTSP_FIELD"directProxy";

onceToken token([](){
    //默认Md5方式认证
    mINI::Instance()[kAuthBasic] = 0;
    mINI::Instance()[kHandshakeSecond] = 15;
    mINI::Instance()[kKeepAliveSecond] = 15;
    mINI::Instance()[kDirectProxy] = 1;
},nullptr);
} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
#define RTMP_FIELD "rtmp."
const string kModifyStamp = RTMP_FIELD"modifyStamp";
const string kHandshakeSecond = RTMP_FIELD"handshakeSecond";
const string kKeepAliveSecond = RTMP_FIELD"keepAliveSecond";

onceToken token([](){
    mINI::Instance()[kModifyStamp] = false;
    mINI::Instance()[kHandshakeSecond] = 15;
    mINI::Instance()[kKeepAliveSecond] = 15;
},nullptr);
} //namespace RTMP


////////////RTP配置///////////
namespace Rtp {
#define RTP_FIELD "rtp."
//RTP打包最大MTU,公网情况下更小
const string kVideoMtuSize = RTP_FIELD"videoMtuSize";
const string kAudioMtuSize = RTP_FIELD"audioMtuSize";
//rtp包最大长度限制，单位是KB
const string kRtpMaxSize = RTP_FIELD"rtpMaxSize";

onceToken token([](){
    mINI::Instance()[kVideoMtuSize] = 1400;
    mINI::Instance()[kAudioMtuSize] = 600;
    mINI::Instance()[kRtpMaxSize] = 10;
},nullptr);
} //namespace Rtsp

////////////组播配置///////////
namespace MultiCast {
#define MULTI_FIELD "multicast."
//组播分配起始地址
const string kAddrMin = MULTI_FIELD"addrMin";
//组播分配截止地址
const string kAddrMax = MULTI_FIELD"addrMax";
//组播TTL
const string kUdpTTL = MULTI_FIELD"udpTTL";

onceToken token([](){
    mINI::Instance()[kAddrMin] = "239.0.0.0";
    mINI::Instance()[kAddrMax] = "239.255.255.255";
    mINI::Instance()[kUdpTTL] = 64;
},nullptr);
} //namespace MultiCast

////////////录像配置///////////
namespace Record {
#define RECORD_FIELD "record."
//查看录像的应用名称
const string kAppName = RECORD_FIELD"appName";
//每次流化MP4文件的时长,单位毫秒
const string kSampleMS = RECORD_FIELD"sampleMS";
//MP4文件录制大小,默认一个小时
const string kFileSecond = RECORD_FIELD"fileSecond";
//录制文件路径
const string kFilePath = RECORD_FIELD"filePath";
//mp4文件写缓存大小
const string kFileBufSize = RECORD_FIELD"fileBufSize";
//mp4录制完成后是否进行二次关键帧索引写入头部
const string kFastStart = RECORD_FIELD"fastStart";
//mp4文件是否重头循环读取
const string kFileRepeat = RECORD_FIELD"fileRepeat";

onceToken token([](){
    mINI::Instance()[kAppName] = "record";
    mINI::Instance()[kSampleMS] = 500;
    mINI::Instance()[kFileSecond] = 60*60;
    mINI::Instance()[kFilePath] = "./www";
    mINI::Instance()[kFileBufSize] = 64 * 1024;
    mINI::Instance()[kFastStart] = false;
    mINI::Instance()[kFileRepeat] = false;
},nullptr);
} //namespace Record

////////////HLS相关配置///////////
namespace Hls {
#define HLS_FIELD "hls."
//HLS切片时长,单位秒
const string kSegmentDuration = HLS_FIELD"segDur";
//HLS切片个数
const string kSegmentNum = HLS_FIELD"segNum";
//HLS切片从m3u8文件中移除后，继续保留在磁盘上的个数
const string kSegmentRetain = HLS_FIELD"segRetain";
//HLS文件写缓存大小
const string kFileBufSize = HLS_FIELD"fileBufSize";
//录制文件路径
const string kFilePath = HLS_FIELD"filePath";
// 是否广播 ts 切片完成通知
const string kBroadcastRecordTs = HLS_FIELD"broadcastRecordTs";
//hls直播文件删除延时，单位秒
const string kDeleteDelaySec = HLS_FIELD"deleteDelaySec";

onceToken token([](){
    mINI::Instance()[kSegmentDuration] = 2;
    mINI::Instance()[kSegmentNum] = 3;
    mINI::Instance()[kSegmentRetain] = 5;
    mINI::Instance()[kFileBufSize] = 64 * 1024;
    mINI::Instance()[kFilePath] = "./www";
    mINI::Instance()[kBroadcastRecordTs] = false;
    mINI::Instance()[kDeleteDelaySec] = 0;
},nullptr);
} //namespace Hls


////////////Rtp代理相关配置///////////
namespace RtpProxy {
#define RTP_PROXY_FIELD "rtp_proxy."
//rtp调试数据保存目录
const string kDumpDir = RTP_PROXY_FIELD"dumpDir";
//rtp接收超时时间
const string kTimeoutSec = RTP_PROXY_FIELD"timeoutSec";

onceToken token([](){
    mINI::Instance()[kDumpDir] = "";
    mINI::Instance()[kTimeoutSec] = 15;
},nullptr);
} //namespace RtpProxy


namespace Client {
const string kNetAdapter = "net_adapter";
const string kRtpType = "rtp_type";
const string kRtspUser = "rtsp_user" ;
const string kRtspPwd = "rtsp_pwd";
const string kRtspPwdIsMD5 = "rtsp_pwd_md5";
const string kTimeoutMS = "protocol_timeout_ms";
const string kMediaTimeoutMS = "media_timeout_ms";
const string kBeatIntervalMS = "beat_interval_ms";
const string kBenchmarkMode = "benchmark_mode";
const string kWaitTrackReady = "wait_track_ready";
}

}  // namespace mediakit

#ifdef ENABLE_MEM_DEBUG

extern "C" {
    extern void *__real_malloc(size_t);
    extern void __real_free(void *);
    extern void *__real_realloc(void *ptr, size_t c);
    void *__wrap_malloc(size_t c);
    void __wrap_free(void *ptr);
    void *__wrap_calloc(size_t __nmemb, size_t __size);
    void *__wrap_realloc(void *ptr, size_t c);
}

#define BLOCK_TYPES 16
#define MIN_BLOCK_SIZE 128

static int get_mem_block_type(size_t c) {
    int ret = 0;
    while (c > MIN_BLOCK_SIZE && ret + 1 < BLOCK_TYPES) {
        c >>= 1;
        ++ret;
    }
    return ret;
}

std::vector<size_t> getBlockTypeSize() {
    std::vector<size_t> ret;
    ret.resize(BLOCK_TYPES);
    size_t block_size = MIN_BLOCK_SIZE;
    for (auto i = 0; i < BLOCK_TYPES; ++i) {
        ret[i] = block_size;
        block_size <<= 1;
    }
    return ret;
}

class MemThreadInfo {
public:
    using Ptr = std::shared_ptr<MemThreadInfo>;
    atomic<uint64_t> mem_usage{0};
    atomic<uint64_t> mem_block{0};
    atomic<uint64_t> mem_block_map[BLOCK_TYPES];

    static MemThreadInfo *Instance(bool is_thread_local) {
        if (!is_thread_local) {
            static auto instance = new MemThreadInfo(is_thread_local);
            return instance;
        }
        static auto thread_local instance = new MemThreadInfo(is_thread_local);
        return instance;
    }

    ~MemThreadInfo() {
        //printf("%s %d\r\n", __FUNCTION__, (int) _is_thread_local);
    }

    MemThreadInfo(bool is_thread_local) {
        _is_thread_local = is_thread_local;
        if (_is_thread_local) {
            //确保所有线程退出后才能释放全局内存统计器
            total_mem = Instance(false);
        }
        //printf("%s %d\r\n", __FUNCTION__, (int) _is_thread_local);
    }

    void *operator new(size_t sz) {
        return __real_malloc(sz);
    }

    void operator delete(void *ptr) {
        __real_free(ptr);
    }

    void addBlock(size_t c) {
        if (total_mem) {
            total_mem->addBlock(c);
        }
        mem_usage += c;
        ++mem_block_map[get_mem_block_type(c)];
        ++mem_block;
    }

    void delBlock(size_t c) {
        if (total_mem) {
            total_mem->delBlock(c);
        }
        mem_usage -= c;
        --mem_block_map[get_mem_block_type(c)];
        if (0 == --mem_block) {
            delete this;
        }
    }

private:
    bool _is_thread_local;
    MemThreadInfo *total_mem = nullptr;
};

class MemThreadInfoLocal {
public:
    MemThreadInfoLocal() {
        ptr = MemThreadInfo::Instance(true);
        ptr->addBlock(1);
    }

    ~MemThreadInfoLocal() {
        ptr->delBlock(1);
    }

    MemThreadInfo *get() const {
        return ptr;
    }

private:
    MemThreadInfo *ptr;
};

//该变量主要确保线程退出后才能释放MemThreadInfo变量
static thread_local MemThreadInfoLocal s_thread_mem_info;

uint64_t getTotalMemUsage() {
    return MemThreadInfo::Instance(false)->mem_usage.load();
}

uint64_t getTotalMemBlock() {
    return MemThreadInfo::Instance(false)->mem_block.load();
}

uint64_t getTotalMemBlockByType(int type) {
    assert(type < BLOCK_TYPES);
    return MemThreadInfo::Instance(false)->mem_block_map[type].load();
}

uint64_t getThisThreadMemUsage() {
    return MemThreadInfo::Instance(true)->mem_usage.load();
}

uint64_t getThisThreadMemBlock() {
    return MemThreadInfo::Instance(true)->mem_block.load();
}

uint64_t getThisThreadMemBlockByType(int type) {
    assert(type < BLOCK_TYPES);
    return MemThreadInfo::Instance(true)->mem_block_map[type].load();
}

class MemCookie {
public:
    static constexpr uint32_t kMagic = 0xFEFDFCFB;
    uint32_t magic;
    uint32_t size;
    MemThreadInfo* alloc_info;
    char ptr;
};

#define MEM_OFFSET offsetof(MemCookie, ptr)

#if (defined(__linux__) && !defined(ANDROID)) || defined(__MACH__)
#define MAX_STACK_FRAMES 128
#define MEM_WARING
#include <limits.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <execinfo.h>

static void print_mem_waring(size_t c) {
    void *array[MAX_STACK_FRAMES];
    int size = backtrace(array, MAX_STACK_FRAMES);
    char **strings = backtrace_symbols(array, size);
    printf("malloc big memory:%d, back trace:\r\n", (int)c);
    for (int i = 0; i < size; ++i) {
        printf("[%d]: %s\r\n", i, strings[i]);
    }
    __real_free(strings);
}
#endif

static void init_cookie(MemCookie *cookie, size_t c) {
    cookie->magic = MemCookie::kMagic;
    cookie->size = c;
    cookie->alloc_info = s_thread_mem_info.get();
    cookie->alloc_info->addBlock(c);

#if defined(MEM_WARING)
    static auto env = getenv("MEM_WARN_SIZE");
    static size_t s_mem_waring_size = atoll(env ? env : "0");
    if (s_mem_waring_size > 1024 && c >= s_mem_waring_size) {
        print_mem_waring(c);
    }
#endif
}

static void un_init_cookie(MemCookie *cookie) {
    cookie->alloc_info->delBlock(cookie->size);
}

void *__wrap_malloc(size_t c) {
    c += MEM_OFFSET;
    auto cookie = (MemCookie *) __real_malloc(c);
    if (cookie) {
        init_cookie(cookie, c);
        return &cookie->ptr;
    }
    return nullptr;
}

void __wrap_free(void *ptr) {
    if (!ptr) {
        return;
    }
    auto cookie = (MemCookie *) ((char *) ptr - MEM_OFFSET);
    if (cookie->magic != MemCookie::kMagic) {
        __real_free(ptr);
        return;
    }
    un_init_cookie(cookie);
    __real_free(cookie);
}

void *__wrap_calloc(size_t __nmemb, size_t __size) {
    auto size = __nmemb * __size;
    auto ret = malloc(size);
    if (ret) {
        memset(ret, 0, size);
    }
    return ret;
}

void *__wrap_realloc(void *ptr, size_t c) {
    if (!ptr) {
        return malloc(c);
    }

    auto cookie = (MemCookie *) ((char *) ptr - MEM_OFFSET);
    if (cookie->magic != MemCookie::kMagic) {
        return __real_realloc(ptr, c);
    }

    un_init_cookie(cookie);
    c += MEM_OFFSET;
    cookie = (MemCookie *) __real_realloc(cookie, c);
    if (cookie) {
        init_cookie(cookie, c);
        return &cookie->ptr;
    }
    return nullptr;
}

void *operator new(std::size_t size) {
    auto ret = malloc(size);
    if (ret) {
        return ret;
    }
    throw std::bad_alloc();
}

void operator delete(void *ptr) noexcept {
    free(ptr);
}

void operator delete(void *ptr, std::size_t) noexcept {
    free(ptr);
}

void *operator new[](std::size_t size) {
    auto ret = malloc(size);
    if (ret) {
        return ret;
    }
    throw std::bad_alloc();
}

void operator delete[](void *ptr) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, std::size_t) noexcept {
    free(ptr);
}
#endif
