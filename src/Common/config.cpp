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
} //namespace Broadcast

//通用配置项目
namespace General{
#define GENERAL_FIELD "general."
const string kFlowThreshold = GENERAL_FIELD"flowThreshold";
const string kStreamNoneReaderDelayMS = GENERAL_FIELD"streamNoneReaderDelayMS";
const string kMaxStreamWaitTimeMS = GENERAL_FIELD"maxStreamWaitMS";
const string kEnableVhost = GENERAL_FIELD"enableVhost";
const string kUltraLowDelay = GENERAL_FIELD"ultraLowDelay";
const string kAddMuteAudio = GENERAL_FIELD"addMuteAudio";
const string kResetWhenRePlay = GENERAL_FIELD"resetWhenRePlay";

onceToken token([](){
    mINI::Instance()[kFlowThreshold] = 1024;
    mINI::Instance()[kStreamNoneReaderDelayMS] = 5 * 1000;
    mINI::Instance()[kMaxStreamWaitTimeMS] = 5 * 1000;
    mINI::Instance()[kEnableVhost] = 1;
	mINI::Instance()[kUltraLowDelay] = 1;
	mINI::Instance()[kAddMuteAudio] = 1;
	mINI::Instance()[kResetWhenRePlay] = 1;
},nullptr);

}//namespace General

////////////HTTP配置///////////
namespace Http {
#define HTTP_FIELD "http."

//http 文件发送缓存大小
#define HTTP_SEND_BUF_SIZE (64 * 1024)
const string kSendBufSize = HTTP_FIELD"sendBufSize";

//http 最大请求字节数
#define HTTP_MAX_REQ_SIZE (4*1024)
const string kMaxReqSize = HTTP_FIELD"maxReqSize";

//http keep-alive秒数
#define HTTP_KEEP_ALIVE_SECOND 10
const string kKeepAliveSecond = HTTP_FIELD"keepAliveSecond";

//http keep-alive最大请求数
#define HTTP_MAX_REQ_CNT 100
const string kMaxReqCount = HTTP_FIELD"maxReqCount";


//http 字符编码
#if defined(_WIN32)
#define HTTP_CHAR_SET "gb2312"
#else
#define HTTP_CHAR_SET "utf-8"
#endif
const string kCharSet = HTTP_FIELD"charSet";

//http 服务器根目录
#define HTTP_ROOT_PATH "./httpRoot"
const string kRootPath = HTTP_FIELD"rootPath";

//http 404错误提示内容
#define HTTP_NOT_FOUND "<html>"\
		"<head><title>404 Not Found</title></head>"\
		"<body bgcolor=\"white\">"\
		"<center><h1>您访问的资源不存在！</h1></center>"\
		"<hr><center>"\
		 SERVER_NAME\
		"</center>"\
		"</body>"\
		"</html>"
const string kNotFound = HTTP_FIELD"notFound";


onceToken token([](){
	mINI::Instance()[kSendBufSize] = HTTP_SEND_BUF_SIZE;
	mINI::Instance()[kMaxReqSize] = HTTP_MAX_REQ_SIZE;
	mINI::Instance()[kKeepAliveSecond] = HTTP_KEEP_ALIVE_SECOND;
	mINI::Instance()[kMaxReqCount] = HTTP_MAX_REQ_CNT;
	mINI::Instance()[kCharSet] = HTTP_CHAR_SET;
	mINI::Instance()[kRootPath] = HTTP_ROOT_PATH;
	mINI::Instance()[kNotFound] = HTTP_NOT_FOUND;
},nullptr);

}//namespace Http

////////////SHELL配置///////////
namespace Shell {
#define SHELL_FIELD "shell."

#define SHELL_MAX_REQ_SIZE 1024
const string kMaxReqSize = SHELL_FIELD"maxReqSize";

onceToken token([](){
	mINI::Instance()[kMaxReqSize] = SHELL_MAX_REQ_SIZE;
},nullptr);
} //namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {
#define RTSP_FIELD "rtsp."
const string kAuthBasic = RTSP_FIELD"authBasic";
const string kHandshakeSecond = RTSP_FIELD"handshakeSecond";
const string kKeepAliveSecond = RTSP_FIELD"keepAliveSecond";
const string kDirectProxy = RTSP_FIELD"directProxy";
const string kModifyStamp = RTSP_FIELD"modifyStamp";

onceToken token([](){
	//默认Md5方式认证
	mINI::Instance()[kAuthBasic] = 0;
    mINI::Instance()[kHandshakeSecond] = 15;
    mINI::Instance()[kKeepAliveSecond] = 15;
	mINI::Instance()[kDirectProxy] = 1;
	mINI::Instance()[kModifyStamp] = false;
},nullptr);

} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
#define RTMP_FIELD "rtmp."
const string kModifyStamp = RTMP_FIELD"modifyStamp";
const string kHandshakeSecond = RTMP_FIELD"handshakeSecond";
const string kKeepAliveSecond = RTMP_FIELD"keepAliveSecond";

onceToken token([](){
	mINI::Instance()[kModifyStamp] = true;
    mINI::Instance()[kHandshakeSecond] = 15;
    mINI::Instance()[kKeepAliveSecond] = 15;
},nullptr);

} //namespace RTMP


////////////RTP配置///////////
namespace Rtp {
#define RTP_FIELD "rtp."

//RTP打包最大MTU,公网情况下更小
#define RTP_VIDOE_MTU_SIZE 1400
const string kVideoMtuSize = RTP_FIELD"videoMtuSize";

#define RTP_Audio_MTU_SIZE 600
const string kAudioMtuSize = RTP_FIELD"audioMtuSize";

//RTP排序缓存最大个数
#define RTP_MAX_RTP_COUNT 50
const string kMaxRtpCount = RTP_FIELD"maxRtpCount";

//如果RTP序列正确次数累计达到该数字就启动清空排序缓存
#define RTP_CLEAR_COUNT 10
const string kClearCount = RTP_FIELD"clearCount";

//最大RTP时间为13个小时，每13小时回环一次
#define RTP_CYCLE_MS (13*60*60*1000)
const string kCycleMS = RTP_FIELD"cycleMS";


onceToken token([](){
	mINI::Instance()[kVideoMtuSize] = RTP_VIDOE_MTU_SIZE;
	mINI::Instance()[kAudioMtuSize] = RTP_Audio_MTU_SIZE;
	mINI::Instance()[kMaxRtpCount] = RTP_MAX_RTP_COUNT;
	mINI::Instance()[kClearCount] = RTP_CLEAR_COUNT;
	mINI::Instance()[kCycleMS] = RTP_CYCLE_MS;
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
#define MULTI_UDP_TTL 64
const string kUdpTTL = MULTI_FIELD"udpTTL";

onceToken token([](){
	mINI::Instance()[kAddrMin] = "239.0.0.0";
	mINI::Instance()[kAddrMax] = "239.255.255.255";
	mINI::Instance()[kUdpTTL] = MULTI_UDP_TTL;
},nullptr);

} //namespace MultiCast

////////////录像配置///////////
namespace Record {
#define RECORD_FIELD "record."

//查看录像的应用名称
#define RECORD_APP_NAME "record"
const string kAppName = RECORD_FIELD"appName";

//每次流化MP4文件的时长,单位毫秒
#define RECORD_SAMPLE_MS 500
const string kSampleMS = RECORD_FIELD"sampleMS";

//MP4文件录制大小,默认一个小时
#define RECORD_FILE_SECOND (60*60)
const string kFileSecond = RECORD_FIELD"fileSecond";

//录制文件路径
#define RECORD_FILE_PATH HTTP_ROOT_PATH
const string kFilePath = RECORD_FIELD"filePath";

//mp4文件写缓存大小
const string kFileBufSize = RECORD_FIELD"fileBufSize";

//mp4录制完成后是否进行二次关键帧索引写入头部
const string kFastStart = RECORD_FIELD"fastStart";

//mp4文件是否重头循环读取
const string kFileRepeat = RECORD_FIELD"fileRepeat";

onceToken token([](){
	mINI::Instance()[kAppName] = RECORD_APP_NAME;
	mINI::Instance()[kSampleMS] = RECORD_SAMPLE_MS;
	mINI::Instance()[kFileSecond] = RECORD_FILE_SECOND;
	mINI::Instance()[kFilePath] = RECORD_FILE_PATH;
	mINI::Instance()[kFileBufSize] = 64 * 1024;
	mINI::Instance()[kFastStart] = false;
	mINI::Instance()[kFileRepeat] = false;
},nullptr);

} //namespace Record

////////////HLS相关配置///////////
namespace Hls {
#define HLS_FIELD "hls."

//HLS切片时长,单位秒
#define HLS_SEGMENT_DURATION 3
const string kSegmentDuration = HLS_FIELD"segDur";

//HLS切片个数
#define HLS_SEGMENT_NUM 3
const string kSegmentNum = HLS_FIELD"segNum";

//HLS文件写缓存大小
#define HLS_FILE_BUF_SIZE (64 * 1024)
const string kFileBufSize = HLS_FIELD"fileBufSize";

//录制文件路径
#define HLS_FILE_PATH (HTTP_ROOT_PATH)
const string kFilePath = HLS_FIELD"filePath";

onceToken token([](){
	mINI::Instance()[kSegmentDuration] = HLS_SEGMENT_DURATION;
	mINI::Instance()[kSegmentNum] = HLS_SEGMENT_NUM;
	mINI::Instance()[kFileBufSize] = HLS_FILE_BUF_SIZE;
	mINI::Instance()[kFilePath] = HLS_FILE_PATH;
},nullptr);

} //namespace Hls


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

}

}  // namespace mediakit


