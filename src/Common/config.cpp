﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Common/config.h"
#include "MediaSource.h"
#include "Util/NoticeCenter.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/util.h"
#include <assert.h>
#include <stdio.h>

using namespace std;
using namespace toolkit;

namespace mediakit {

bool loadIniConfig(const char *ini_path) {
    string ini;
    if (ini_path && ini_path[0] != '\0') {
        ini = ini_path;
    } else {
        ini = exePath() + ".ini";
    }
    try {
        mINI::Instance().parseFile(ini);
        NOTICE_EMIT(BroadcastReloadConfigArgs, Broadcast::kBroadcastReloadConfig);
        return true;
    } catch (std::exception &) {
        InfoL << "dump ini file to:" << ini;
        mINI::Instance().dumpFile(ini);
        return false;
    }
}
////////////广播名称///////////
namespace Broadcast {
const string kBroadcastMediaChanged = "kBroadcastMediaChanged";
const string kBroadcastRecordMP4 = "kBroadcastRecordMP4";
const string kBroadcastRecordTs = "kBroadcastRecordTs";
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
const string kBroadcastSendRtpStopped = "kBroadcastSendRtpStopped";
const string kBroadcastRtpServerTimeout = "kBroadcastRtpServerTimeout";
const string kBroadcastRtcSctpConnecting = "kBroadcastRtcSctpConnecting";
const string kBroadcastRtcSctpConnected = "kBroadcastRtcSctpConnected";
const string kBroadcastRtcSctpFailed = "kBroadcastRtcSctpFailed";
const string kBroadcastRtcSctpClosed = "kBroadcastRtcSctpClosed";
const string kBroadcastRtcSctpSend = "kBroadcastRtcSctpSend";
const string kBroadcastRtcSctpReceived = "kBroadcastRtcSctpReceived";
const string kBroadcastPlayerCountChanged = "kBroadcastPlayerCountChanged";

} // namespace Broadcast

// 通用配置项目
namespace General {
#define GENERAL_FIELD "general."
const string kMediaServerId = GENERAL_FIELD "mediaServerId";
const string kFlowThreshold = GENERAL_FIELD "flowThreshold";
const string kStreamNoneReaderDelayMS = GENERAL_FIELD "streamNoneReaderDelayMS";
const string kMaxStreamWaitTimeMS = GENERAL_FIELD "maxStreamWaitMS";
const string kEnableVhost = GENERAL_FIELD "enableVhost";
const string kResetWhenRePlay = GENERAL_FIELD "resetWhenRePlay";
const string kMergeWriteMS = GENERAL_FIELD "mergeWriteMS";
const string kCheckNvidiaDev = GENERAL_FIELD "check_nvidia_dev";
const string kEnableFFmpegLog = GENERAL_FIELD "enable_ffmpeg_log";
const string kWaitTrackReadyMS = GENERAL_FIELD "wait_track_ready_ms";
const string kWaitAddTrackMS = GENERAL_FIELD "wait_add_track_ms";
const string kUnreadyFrameCache = GENERAL_FIELD "unready_frame_cache";
const string kBroadcastPlayerCountChanged = GENERAL_FIELD "broadcast_player_count_changed";

static onceToken token([]() {
    mINI::Instance()[kFlowThreshold] = 1024;
    mINI::Instance()[kStreamNoneReaderDelayMS] = 20 * 1000;
    mINI::Instance()[kMaxStreamWaitTimeMS] = 15 * 1000;
    mINI::Instance()[kEnableVhost] = 0;
    mINI::Instance()[kResetWhenRePlay] = 1;
    mINI::Instance()[kMergeWriteMS] = 0;
    mINI::Instance()[kMediaServerId] = makeRandStr(16);
    mINI::Instance()[kCheckNvidiaDev] = 1;
    mINI::Instance()[kEnableFFmpegLog] = 0;
    mINI::Instance()[kWaitTrackReadyMS] = 10000;
    mINI::Instance()[kWaitAddTrackMS] = 3000;
    mINI::Instance()[kUnreadyFrameCache] = 100;
    mINI::Instance()[kBroadcastPlayerCountChanged] = 0;
});

} // namespace General

namespace Protocol {
const string kModifyStamp = string(kFieldName) + "modify_stamp";
const string kEnableAudio = string(kFieldName) + "enable_audio";
const string kAddMuteAudio = string(kFieldName) + "add_mute_audio";
const string kAutoClose = string(kFieldName) + "auto_close";
const string kContinuePushMS = string(kFieldName) + "continue_push_ms";
const string kPacedSenderMS = string(kFieldName) + "paced_sender_ms";

const string kEnableHls = string(kFieldName) + "enable_hls";
const string kEnableHlsFmp4 = string(kFieldName) + "enable_hls_fmp4";
const string kEnableMP4 = string(kFieldName) + "enable_mp4";
const string kEnableRtsp = string(kFieldName) + "enable_rtsp";
const string kEnableRtmp = string(kFieldName) + "enable_rtmp";
const string kEnableTS = string(kFieldName) + "enable_ts";
const string kEnableFMP4 = string(kFieldName) + "enable_fmp4";

const string kMP4AsPlayer = string(kFieldName) + "mp4_as_player";
const string kMP4MaxSecond = string(kFieldName) + "mp4_max_second";
const string kMP4SavePath = string(kFieldName) + "mp4_save_path";

const string kHlsSavePath = string(kFieldName) + "hls_save_path";

const string kHlsDemand = string(kFieldName) + "hls_demand";
const string kRtspDemand = string(kFieldName) + "rtsp_demand";
const string kRtmpDemand = string(kFieldName) + "rtmp_demand";
const string kTSDemand = string(kFieldName) + "ts_demand";
const string kFMP4Demand = string(kFieldName) + "fmp4_demand";

static onceToken token([]() {
    mINI::Instance()[kModifyStamp] = (int)ProtocolOption::kModifyStampRelative;
    mINI::Instance()[kEnableAudio] = 1;
    mINI::Instance()[kAddMuteAudio] = 1;
    mINI::Instance()[kContinuePushMS] = 15000;
    mINI::Instance()[kPacedSenderMS] = 0;
    mINI::Instance()[kAutoClose] = 0;

    mINI::Instance()[kEnableHls] = 1;
    mINI::Instance()[kEnableHlsFmp4] = 0;
    mINI::Instance()[kEnableMP4] = 0;
    mINI::Instance()[kEnableRtsp] = 1;
    mINI::Instance()[kEnableRtmp] = 1;
    mINI::Instance()[kEnableTS] = 1;
    mINI::Instance()[kEnableFMP4] = 1;

    mINI::Instance()[kMP4AsPlayer] = 0;
    mINI::Instance()[kMP4MaxSecond] = 3600;
    mINI::Instance()[kMP4SavePath] = "./www";

    mINI::Instance()[kHlsSavePath] = "./www";

    mINI::Instance()[kHlsDemand] = 0;
    mINI::Instance()[kRtspDemand] = 0;
    mINI::Instance()[kRtmpDemand] = 0;
    mINI::Instance()[kTSDemand] = 0;
    mINI::Instance()[kFMP4Demand] = 0;
});
} // !Protocol

////////////HTTP配置///////////
namespace Http {
#define HTTP_FIELD "http."
const string kSendBufSize = HTTP_FIELD "sendBufSize";
const string kMaxReqSize = HTTP_FIELD "maxReqSize";
const string kKeepAliveSecond = HTTP_FIELD "keepAliveSecond";
const string kCharSet = HTTP_FIELD "charSet";
const string kRootPath = HTTP_FIELD "rootPath";
const string kVirtualPath = HTTP_FIELD "virtualPath";
const string kNotFound = HTTP_FIELD "notFound";
const string kDirMenu = HTTP_FIELD "dirMenu";
const string kForbidCacheSuffix = HTTP_FIELD "forbidCacheSuffix";
const string kForwardedIpHeader = HTTP_FIELD "forwarded_ip_header";
const string kAllowCrossDomains = HTTP_FIELD "allow_cross_domains";
const string kAllowIPRange = HTTP_FIELD "allow_ip_range";

static onceToken token([]() {
    mINI::Instance()[kSendBufSize] = 64 * 1024;
    mINI::Instance()[kMaxReqSize] = 4 * 10240;
    mINI::Instance()[kKeepAliveSecond] = 15;
    mINI::Instance()[kDirMenu] = true;
    mINI::Instance()[kVirtualPath] = "";
    mINI::Instance()[kCharSet] = "utf-8";

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
    mINI::Instance()[kForbidCacheSuffix] = "";
    mINI::Instance()[kForwardedIpHeader] = "";
    mINI::Instance()[kAllowCrossDomains] = 1;
    mINI::Instance()[kAllowIPRange] = "::1,127.0.0.1,172.16.0.0-172.31.255.255,192.168.0.0-192.168.255.255,10.0.0.0-10.255.255.255";
});

} // namespace Http

////////////SHELL配置///////////
namespace Shell {
#define SHELL_FIELD "shell."
const string kMaxReqSize = SHELL_FIELD "maxReqSize";

static onceToken token([]() { mINI::Instance()[kMaxReqSize] = 1024; });
} // namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {
#define RTSP_FIELD "rtsp."
const string kAuthBasic = RTSP_FIELD "authBasic";
const string kHandshakeSecond = RTSP_FIELD "handshakeSecond";
const string kKeepAliveSecond = RTSP_FIELD "keepAliveSecond";
const string kDirectProxy = RTSP_FIELD "directProxy";
const string kLowLatency = RTSP_FIELD"lowLatency";
const string kRtpTransportType = RTSP_FIELD"rtpTransportType";

static onceToken token([]() {
    // 默认Md5方式认证
    mINI::Instance()[kAuthBasic] = 0;
    mINI::Instance()[kHandshakeSecond] = 15;
    mINI::Instance()[kKeepAliveSecond] = 15;
    mINI::Instance()[kDirectProxy] = 1;
    mINI::Instance()[kLowLatency] = 0;
    mINI::Instance()[kRtpTransportType] = -1;
});
} // namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
#define RTMP_FIELD "rtmp."
const string kHandshakeSecond = RTMP_FIELD "handshakeSecond";
const string kKeepAliveSecond = RTMP_FIELD "keepAliveSecond";
const string kDirectProxy = RTMP_FIELD "directProxy";
const string kEnhanced = RTMP_FIELD "enhanced";

static onceToken token([]() {
    mINI::Instance()[kHandshakeSecond] = 15;
    mINI::Instance()[kKeepAliveSecond] = 15;
    mINI::Instance()[kDirectProxy] = 1;
    mINI::Instance()[kEnhanced] = 0;
});
} // namespace Rtmp

////////////RTP配置///////////
namespace Rtp {
#define RTP_FIELD "rtp."
// RTP打包最大MTU,公网情况下更小
const string kVideoMtuSize = RTP_FIELD "videoMtuSize";
const string kAudioMtuSize = RTP_FIELD "audioMtuSize";
// rtp包最大长度限制，单位是KB
const string kRtpMaxSize = RTP_FIELD "rtpMaxSize";
const string kLowLatency = RTP_FIELD "lowLatency";
const string kH264StapA = RTP_FIELD "h264_stap_a";

static onceToken token([]() {
    mINI::Instance()[kVideoMtuSize] = 1400;
    mINI::Instance()[kAudioMtuSize] = 600;
    mINI::Instance()[kRtpMaxSize] = 10;
    mINI::Instance()[kLowLatency] = 0;
    mINI::Instance()[kH264StapA] = 1;
});
} // namespace Rtp

////////////组播配置///////////
namespace MultiCast {
#define MULTI_FIELD "multicast."
// 组播分配起始地址
const string kAddrMin = MULTI_FIELD "addrMin";
// 组播分配截止地址
const string kAddrMax = MULTI_FIELD "addrMax";
// 组播TTL
const string kUdpTTL = MULTI_FIELD "udpTTL";

static onceToken token([]() {
    mINI::Instance()[kAddrMin] = "239.0.0.0";
    mINI::Instance()[kAddrMax] = "239.255.255.255";
    mINI::Instance()[kUdpTTL] = 64;
});
} // namespace MultiCast

////////////录像配置///////////
namespace Record {
#define RECORD_FIELD "record."
const string kAppName = RECORD_FIELD "appName";
const string kSampleMS = RECORD_FIELD "sampleMS";
const string kFileBufSize = RECORD_FIELD "fileBufSize";
const string kFastStart = RECORD_FIELD "fastStart";
const string kFileRepeat = RECORD_FIELD "fileRepeat";
const string kEnableFmp4 = RECORD_FIELD "enableFmp4";

static onceToken token([]() {
    mINI::Instance()[kAppName] = "record";
    mINI::Instance()[kSampleMS] = 500;
    mINI::Instance()[kFileBufSize] = 64 * 1024;
    mINI::Instance()[kFastStart] = false;
    mINI::Instance()[kFileRepeat] = false;
    mINI::Instance()[kEnableFmp4] = false;
});
} // namespace Record

////////////HLS相关配置///////////
namespace Hls {
#define HLS_FIELD "hls."
const string kSegmentDuration = HLS_FIELD "segDur";
const string kSegmentNum = HLS_FIELD "segNum";
const string kSegmentKeep = HLS_FIELD "segKeep";
const string kSegmentDelay = HLS_FIELD "segDelay";
const string kSegmentRetain = HLS_FIELD "segRetain";
const string kFileBufSize = HLS_FIELD "fileBufSize";
const string kBroadcastRecordTs = HLS_FIELD "broadcastRecordTs";
const string kDeleteDelaySec = HLS_FIELD "deleteDelaySec";
const string kFastRegister = HLS_FIELD "fastRegister";

static onceToken token([]() {
    mINI::Instance()[kSegmentDuration] = 2;
    mINI::Instance()[kSegmentNum] = 3;
    mINI::Instance()[kSegmentKeep] = false;
    mINI::Instance()[kSegmentDelay] = 0;
    mINI::Instance()[kSegmentRetain] = 5;
    mINI::Instance()[kFileBufSize] = 64 * 1024;
    mINI::Instance()[kBroadcastRecordTs] = false;
    mINI::Instance()[kDeleteDelaySec] = 10;
    mINI::Instance()[kFastRegister] = false;
});
} // namespace Hls

////////////Rtp代理相关配置///////////
namespace RtpProxy {
#define RTP_PROXY_FIELD "rtp_proxy."
const string kDumpDir = RTP_PROXY_FIELD "dumpDir";
const string kTimeoutSec = RTP_PROXY_FIELD "timeoutSec";
const string kPortRange = RTP_PROXY_FIELD "port_range";
const string kH264PT = RTP_PROXY_FIELD "h264_pt";
const string kH265PT = RTP_PROXY_FIELD "h265_pt";
const string kPSPT = RTP_PROXY_FIELD "ps_pt";
const string kOpusPT = RTP_PROXY_FIELD "opus_pt";
const string kGopCache = RTP_PROXY_FIELD "gop_cache";
const string kRtpG711DurMs = RTP_PROXY_FIELD "rtp_g711_dur_ms";
const string kUdpRecvSocketBuffer = RTP_PROXY_FIELD "udp_recv_socket_buffer";

static onceToken token([]() {
    mINI::Instance()[kDumpDir] = "";
    mINI::Instance()[kTimeoutSec] = 15;
    mINI::Instance()[kPortRange] = "30000-35000";
    mINI::Instance()[kH264PT] = 98;
    mINI::Instance()[kH265PT] = 99;
    mINI::Instance()[kPSPT] = 96;
    mINI::Instance()[kOpusPT] = 100;
    mINI::Instance()[kGopCache] = 1;
    mINI::Instance()[kRtpG711DurMs] = 100;
    mINI::Instance()[kUdpRecvSocketBuffer] = 4 * 1024 * 1024;
});
} // namespace RtpProxy

namespace Client {
const string kNetAdapter = "net_adapter";
const string kRtpType = "rtp_type";
const string kRtspBeatType = "rtsp_beat_type";
const string kRtspUser = "rtsp_user";
const string kRtspPwd = "rtsp_pwd";
const string kRtspPwdIsMD5 = "rtsp_pwd_md5";
const string kTimeoutMS = "protocol_timeout_ms";
const string kMediaTimeoutMS = "media_timeout_ms";
const string kBeatIntervalMS = "beat_interval_ms";
const string kBenchmarkMode = "benchmark_mode";
const string kWaitTrackReady = "wait_track_ready";
const string kPlayTrack = "play_track";
const string kProxyUrl = "proxy_url";
const string kRtspSpeed = "rtsp_speed";
} // namespace Client

} // namespace mediakit

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
    atomic<uint64_t> mem_usage { 0 };
    atomic<uint64_t> mem_block { 0 };
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
        // printf("%s %d\r\n", __FUNCTION__, (int) _is_thread_local);
    }

    MemThreadInfo(bool is_thread_local) {
        _is_thread_local = is_thread_local;
        if (_is_thread_local) {
            // 确保所有线程退出后才能释放全局内存统计器
            total_mem = Instance(false);
        }
        // printf("%s %d\r\n", __FUNCTION__, (int) _is_thread_local);
    }

    void *operator new(size_t sz) { return __real_malloc(sz); }

    void operator delete(void *ptr) { __real_free(ptr); }

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

    ~MemThreadInfoLocal() { ptr->delBlock(1); }

    MemThreadInfo *get() const { return ptr; }

private:
    MemThreadInfo *ptr;
};

// 该变量主要确保线程退出后才能释放MemThreadInfo变量
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
    MemThreadInfo *alloc_info;
    char ptr;
};

#define MEM_OFFSET offsetof(MemCookie, ptr)

#if (defined(__linux__) && !defined(ANDROID)) || defined(__MACH__)
#define MAX_STACK_FRAMES 128
#define MEM_WARING
#include <execinfo.h>
#include <limits.h>
#include <sys/resource.h>
#include <sys/wait.h>

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
    auto cookie = (MemCookie *)__real_malloc(c);
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
    auto cookie = (MemCookie *)((char *)ptr - MEM_OFFSET);
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

    auto cookie = (MemCookie *)((char *)ptr - MEM_OFFSET);
    if (cookie->magic != MemCookie::kMagic) {
        return __real_realloc(ptr, c);
    }

    un_init_cookie(cookie);
    c += MEM_OFFSET;
    cookie = (MemCookie *)__real_realloc(cookie, c);
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
