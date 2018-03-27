/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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


#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

#include <functional>
#include "Util/mini.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"

using namespace std;
using namespace ZL::Util;

namespace Config {

//加载配置文件，如果配置文件不存在，那么会导出默认配置并生成配置文件
//加载配置文件成功后会触发kBroadcastUpdateConfig广播
//如果指定的文件名(ini_path)为空，那么会加载默认配置文件
//默认配置文件名为 /path/to/your/exe.ini
//加载配置文件成功后返回true，否则返回false
bool loadIniConfig(const char *ini_path = nullptr);
////////////其他宏定义///////////
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b) )
#endif //MAX

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b) )
#endif //MIN

#ifndef CLEAR_ARR
#define CLEAR_ARR(arr) for(auto &item : arr){ item = 0;}
#endif //CLEAR_ARR

#define SERVER_NAME "ZLMediaKit"
#define VHOST_KEY "vhost"
#define HTTP_SCHEMA "http"
#define RTSP_SCHEMA "rtsp"
#define RTMP_SCHEMA "rtmp"
#define DEFAULT_VHOST "__defaultVhost__"
#define RTSP_VERSION 1.30
#define RTSP_BUILDTIME __DATE__" CST"

////////////广播名称///////////
namespace Broadcast {

//注册或反注册MediaSource事件广播
extern const char kBroadcastMediaChanged[];
#define BroadcastMediaChangedArgs const bool &bRegist, const string &schema,const string &vhost,const string &app,const string &stream,MediaSource &sender

//录制mp4文件成功后广播
extern const char kBroadcastRecordMP4[];
#define BroadcastRecordMP4Args const Mp4Info &info,Mp4Maker &sender

//收到http api请求广播
extern const char kBroadcastHttpRequest[];
#define BroadcastHttpRequestArgs const Parser &parser,const HttpSession::HttpResponseInvoker &invoker,bool &consumed,TcpSession &sender

//该流是否需要认证？是的话调用invoker并传入realm,否则传入空的realm.如果该事件不监听则不认证
extern const char kBroadcastOnGetRtspRealm[];
#define BroadcastOnGetRtspRealmArgs const string &app,const string &stream,const RtspSession::onGetRealm &invoker,TcpSession &sender

//请求认证用户密码事件，user_name为用户名，must_no_encrypt如果为true，则必须提供明文密码(因为此时是base64认证方式),否则会导致认证失败
//获取到密码后请调用invoker并输入对应类型的密码和密码类型，invoker执行时会匹配密码
extern const char kBroadcastOnRtspAuth[];
#define BroadcastOnRtspAuthArgs const string &user_name,const bool &must_no_encrypt,const RtspSession::onAuth &invoker,TcpSession &sender

//鉴权结果回调对象
//如果errMessage为空则代表鉴权成功
typedef std::function<void(const string &errMessage)> AuthInvoker;

//收到rtmp推流事件广播，通过该事件控制推流鉴权
extern const char kBroadcastRtmpPublish[];
#define BroadcastRtmpPublishArgs const MediaInfo &args,const Broadcast::AuthInvoker &invoker,TcpSession &sender

//播放rtsp/rtmp/http-flv事件广播，通过该事件控制播放鉴权
extern const char kBroadcastMediaPlayed[];
#define BroadcastMediaPlayedArgs const MediaInfo &args,const Broadcast::AuthInvoker &invoker,TcpSession &sender

//shell登录鉴权
extern const char kBroadcastShellLogin[];
#define BroadcastShellLoginArgs const string &user_name,const string &passwd,const Broadcast::AuthInvoker &invoker,TcpSession &sender

//停止rtsp/rtmp/http-flv会话后流量汇报事件广播
extern const char kBroadcastFlowReport[];
#define BroadcastFlowReportArgs const MediaInfo &args,const uint64_t &totalBytes,TcpSession &sender

//流量汇报事件流量阈值,单位KB，默认1MB
extern const char kFlowThreshold[];

//更新配置文件事件广播,执行loadIniConfig函数加载配置文件成功后会触发该广播
extern const char kBroadcastReloadConfig[];
#define BroadcastReloadConfigArgs void
#define ReloadConfigTag  ((void *)(0xFF))

#define RELOAD_KEY(arg,key) \
    do{ \
        decltype(arg) arg##tmp = mINI::Instance()[key]; \
        if(arg != arg##tmp ) { \
            arg = arg##tmp; \
            InfoL << "reload config:" << key << "=" <<  arg; \
        } \
    }while(0);

//监听某个配置发送变更
#define RELOAD_KEY_REGISTER(arg,key) \
    do{ \
        static onceToken s_token([](){ \
            NoticeCenter::Instance().addListener(ReloadConfigTag,Config::Broadcast::kBroadcastReloadConfig,[](BroadcastReloadConfigArgs){ \
                RELOAD_KEY(arg,key); \
            }); \
        }); \
    }while(0);

#define GET_CONFIG_AND_REGISTER(type,arg,key) \
        static type arg = mINI::Instance()[key]; \
        RELOAD_KEY_REGISTER(arg,key);

} //namespace Broadcast

////////////HTTP配置///////////
namespace Http {
extern const char kPort[];
extern const char kSSLPort[];
//http 文件发送缓存大小
extern const char kSendBufSize[];
//http 最大请求字节数
extern const char kMaxReqSize[];
//http keep-alive秒数
extern const char kKeepAliveSecond[];
//http keep-alive最大请求数
extern const char kMaxReqCount[];
//http 字符编码
extern const char kCharSet[];
//http 服务器根目录
extern const char kRootPath[];
//http 404错误提示内容
extern const char kNotFound[];

}//namespace Http

////////////SHELL配置///////////
namespace Shell {
extern const char kMaxReqSize[];
extern const char kPort[];
} //namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {

extern const char kPort[];
//是否优先base64方式认证？默认Md5方式认证
extern const char kAuthBasic[];
} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
extern const char kPort[];
} //namespace RTMP


////////////RTP配置///////////
namespace Rtp {
//RTP打包最大MTU,公网情况下更小
extern const char kVideoMtuSize[];
//RTP打包最大MTU,公网情况下更小
extern const char kAudioMtuSize[];
//udp方式接受RTP包的最大缓存
extern const char kUdpBufSize[];
//RTP排序缓存最大个数
extern const char kMaxRtpCount[];
//如果RTP序列正确次数累计达到该数字就启动清空排序缓存
extern const char kClearCount[];
//最大RTP时间为13个小时，每13小时回环一次
extern const char kCycleMS[];
} //namespace Rtsp

////////////组播配置///////////
namespace MultiCast {
//组播分配起始地址
extern const char kAddrMin[];
//组播分配截止地址
extern const char kAddrMax[];
//组播TTL
extern const char kUdpTTL[];
} //namespace MultiCast

////////////录像配置///////////
namespace Record {
//查看录像的应用名称
extern const char kAppName[];
//每次流化MP4文件的时长,单位毫秒
extern const char kSampleMS[];
//MP4文件录制大小,不能太大,否则MP4Close函数执行事件太长
extern const char kFileSecond[];
//录制文件路径
extern const char kFilePath[];
} //namespace Record

////////////HLS相关配置///////////
namespace Hls {
//HLS切片时长,单位秒
extern const char kSegmentDuration[];
//HLS切片个数
extern const char kSegmentNum[];
//HLS文件写缓存大小
extern const char kFileBufSize[];
//录制文件路径
extern const char kFilePath[];
} //namespace Hls

}  // namespace Config

#endif /* COMMON_CONFIG_H */
