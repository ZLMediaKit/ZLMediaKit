/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Common/config.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "Network/sockutil.h"

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
    }catch (std::exception &ex) {
        InfoL << "dump ini file to:" << ini;
        mINI::Instance().dumpFile(ini);
        return false;
    }
}
////////////广播名称///////////
namespace Broadcast {
const string kBroadcastMediaChanged = "kBroadcastMediaChanged";
const string kBroadcastRecordMP4 = "kBroadcastRecordMP4";
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
const string kFlowThreshold = GENERAL_FIELD"flowThreshold";
const string kStreamNoneReaderDelayMS = GENERAL_FIELD"streamNoneReaderDelayMS";
const string kMaxStreamWaitTimeMS = GENERAL_FIELD"maxStreamWaitMS";
const string kEnableVhost = GENERAL_FIELD"enableVhost";
const string kAddMuteAudio = GENERAL_FIELD"addMuteAudio";
const string kResetWhenRePlay = GENERAL_FIELD"resetWhenRePlay";
const string kPublishToRtxp = GENERAL_FIELD"publishToRtxp";
const string kPublishToHls = GENERAL_FIELD"publishToHls";
const string kPublishToMP4 = GENERAL_FIELD"publishToMP4";
const string kMergeWriteMS = GENERAL_FIELD"mergeWriteMS";
const string kModifyStamp = GENERAL_FIELD"modifyStamp";

onceToken token([](){
    mINI::Instance()[kFlowThreshold] = 1024;
    mINI::Instance()[kStreamNoneReaderDelayMS] = 20 * 1000;
    mINI::Instance()[kMaxStreamWaitTimeMS] = 15 * 1000;
    mINI::Instance()[kEnableVhost] = 0;
    mINI::Instance()[kAddMuteAudio] = 1;
    mINI::Instance()[kResetWhenRePlay] = 1;
    mINI::Instance()[kPublishToRtxp] = 1;
    mINI::Instance()[kPublishToHls] = 1;
    mINI::Instance()[kPublishToMP4] = 0;
    mINI::Instance()[kMergeWriteMS] = 0;
    mINI::Instance()[kModifyStamp] = 0;
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
//http 404错误提示内容
const string kNotFound = HTTP_FIELD"notFound";
//是否显示文件夹菜单
const string kDirMenu = HTTP_FIELD"dirMenu";

onceToken token([](){
    mINI::Instance()[kSendBufSize] = 64 * 1024;
    mINI::Instance()[kMaxReqSize] = 4*1024;
    mINI::Instance()[kKeepAliveSecond] = 15;
    mINI::Instance()[kDirMenu] = true;

#if defined(_WIN32)
    mINI::Instance()[kCharSet] = "gb2312";
#else
    mINI::Instance()[kCharSet] ="utf-8";
#endif

    mINI::Instance()[kRootPath] = "./www";
    mINI::Instance()[kNotFound] =
                    "<html>"
                    "<head><title>404 Not Found</title></head>"
                    "<body bgcolor=\"white\">"
                    "<center><h1>您访问的资源不存在！</h1></center>"
                    "<hr><center>"
                    SERVER_NAME
                    "</center>"
                    "</body>"
                    "</html>";
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
//RTP排序缓存最大个数
const string kMaxRtpCount = RTP_FIELD"maxRtpCount";
//如果RTP序列正确次数累计达到该数字就启动清空排序缓存
const string kClearCount = RTP_FIELD"clearCount";
//最大RTP时间为13个小时，每13小时回环一次
const string kCycleMS = RTP_FIELD"cycleMS";

onceToken token([](){
    mINI::Instance()[kVideoMtuSize] = 1400;
    mINI::Instance()[kAudioMtuSize] = 600;
    mINI::Instance()[kMaxRtpCount] = 50;
    mINI::Instance()[kClearCount] = 10;
    mINI::Instance()[kCycleMS] = 13*60*60*1000;
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

onceToken token([](){
    mINI::Instance()[kSegmentDuration] = 2;
    mINI::Instance()[kSegmentNum] = 3;
    mINI::Instance()[kSegmentRetain] = 5;
    mINI::Instance()[kFileBufSize] = 64 * 1024;
    mINI::Instance()[kFilePath] = "./www";
},nullptr);
} //namespace Hls


////////////Rtp代理相关配置///////////
namespace RtpProxy {
#define RTP_PROXY_FIELD "rtp_proxy."
//rtp调试数据保存目录
const string kDumpDir = RTP_PROXY_FIELD"dumpDir";
//是否限制udp数据来源ip和端口
const string kCheckSource = RTP_PROXY_FIELD"checkSource";
//rtp接收超时时间
const string kTimeoutSec = RTP_PROXY_FIELD"timeoutSec";

onceToken token([](){
    mINI::Instance()[kDumpDir] = "";
    mINI::Instance()[kCheckSource] = 1;
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
const string kMaxAnalysisMS = "max_analysis_ms";
const string kBenchmarkMode = "benchmark_mode";

}

}  // namespace mediakit


void Assert_Throw(int failed, const char *exp, const char *func, const char *file, int line){
    if(failed) {
        _StrPrinter printer;
        printer << "Assertion failed: (" << exp << "), function " << func << ", file " << file << ", line " << line << ".";
        throw std::runtime_error(printer);
    }
}
