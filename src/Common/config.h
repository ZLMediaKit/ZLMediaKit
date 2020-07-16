/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */


#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

#include <functional>
#include "Util/mini.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

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

#define SERVER_NAME "ZLMediaKit-5.0(build in " __DATE__ " " __TIME__ ")"
#define VHOST_KEY "vhost"
#define HTTP_SCHEMA "http"
#define RTSP_SCHEMA "rtsp"
#define RTMP_SCHEMA "rtmp"
#define HLS_SCHEMA "hls"
#define DEFAULT_VHOST "__defaultVhost__"

////////////广播名称///////////
namespace Broadcast {

//注册或反注册MediaSource事件广播
extern const string kBroadcastMediaChanged;
#define BroadcastMediaChangedArgs const bool &bRegist, MediaSource &sender

//录制mp4文件成功后广播
extern const string kBroadcastRecordMP4;
#define BroadcastRecordMP4Args const MP4Info &info

//收到http api请求广播
extern const string kBroadcastHttpRequest;
#define BroadcastHttpRequestArgs const Parser &parser,const HttpSession::HttpResponseInvoker &invoker,bool &consumed,SockInfo &sender

//在http文件服务器中,收到http访问文件或目录的广播,通过该事件控制访问http目录的权限
extern const string kBroadcastHttpAccess;
#define BroadcastHttpAccessArgs const Parser &parser,const string &path,const bool &is_dir,const HttpSession::HttpAccessPathInvoker &invoker,SockInfo &sender

//在http文件服务器中,收到http访问文件或目录前的广播,通过该事件可以控制http url到文件路径的映射
//在该事件中通过自行覆盖path参数，可以做到譬如根据虚拟主机或者app选择不同http根目录的目的
extern const string kBroadcastHttpBeforeAccess;
#define BroadcastHttpBeforeAccessArgs const Parser &parser,string &path,SockInfo &sender

//该流是否需要认证？是的话调用invoker并传入realm,否则传入空的realm.如果该事件不监听则不认证
extern const string kBroadcastOnGetRtspRealm;
#define BroadcastOnGetRtspRealmArgs const MediaInfo &args,const RtspSession::onGetRealm &invoker,SockInfo &sender

//请求认证用户密码事件，user_name为用户名，must_no_encrypt如果为true，则必须提供明文密码(因为此时是base64认证方式),否则会导致认证失败
//获取到密码后请调用invoker并输入对应类型的密码和密码类型，invoker执行时会匹配密码
extern const string kBroadcastOnRtspAuth;
#define BroadcastOnRtspAuthArgs const MediaInfo &args,const string &realm,const string &user_name,const bool &must_no_encrypt,const RtspSession::onAuth &invoker,SockInfo &sender

//推流鉴权结果回调对象
//如果errMessage为空则代表鉴权成功
//enableHls: 是否允许转换hls
//enableMP4: 是否运行MP4录制
//enableRtxp: rtmp推流时是否运行转rtsp；rtsp推流时，是否允许转rtmp
typedef std::function<void(const string &errMessage,bool enableRtxp,bool enableHls,bool enableMP4)> PublishAuthInvoker;

//收到rtsp/rtmp推流事件广播，通过该事件控制推流鉴权
extern const string kBroadcastMediaPublish;
#define BroadcastMediaPublishArgs const MediaInfo &args,const Broadcast::PublishAuthInvoker &invoker,SockInfo &sender

//播放鉴权结果回调对象
//如果errMessage为空则代表鉴权成功
typedef std::function<void(const string &errMessage)> AuthInvoker;

//播放rtsp/rtmp/http-flv事件广播，通过该事件控制播放鉴权
extern const string kBroadcastMediaPlayed;
#define BroadcastMediaPlayedArgs const MediaInfo &args,const Broadcast::AuthInvoker &invoker,SockInfo &sender

//shell登录鉴权
extern const string kBroadcastShellLogin;
#define BroadcastShellLoginArgs const string &user_name,const string &passwd,const Broadcast::AuthInvoker &invoker,SockInfo &sender

//停止rtsp/rtmp/http-flv会话后流量汇报事件广播
extern const string kBroadcastFlowReport;
#define BroadcastFlowReportArgs const MediaInfo &args,const uint64_t &totalBytes,const uint64_t &totalDuration,const bool &isPlayer, SockInfo &sender

//未找到流后会广播该事件，请在监听该事件后去拉流或其他方式产生流，这样就能按需拉流了
extern const string kBroadcastNotFoundStream;
#define BroadcastNotFoundStreamArgs const MediaInfo &args,SockInfo &sender, const function<void()> &closePlayer

//某个流无人消费时触发，目的为了实现无人观看时主动断开拉流等业务逻辑
extern const string kBroadcastStreamNoneReader;
#define BroadcastStreamNoneReaderArgs MediaSource &sender

//更新配置文件事件广播,执行loadIniConfig函数加载配置文件成功后会触发该广播
extern const string kBroadcastReloadConfig;
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
#define LISTEN_RELOAD_KEY(arg,key) \
    do{ \
        static onceToken s_token([](){ \
            NoticeCenter::Instance().addListener(ReloadConfigTag,Broadcast::kBroadcastReloadConfig,[](BroadcastReloadConfigArgs){ \
                RELOAD_KEY(arg,key); \
            }); \
        }); \
    }while(0);

#define GET_CONFIG(type,arg,key) \
        static type arg = mINI::Instance()[key]; \
        LISTEN_RELOAD_KEY(arg,key);

} //namespace Broadcast

////////////通用配置///////////
namespace General{
//流量汇报事件流量阈值,单位KB，默认1MB
extern const string kFlowThreshold;
//流无人观看并且超过若干时间后才触发kBroadcastStreamNoneReader事件
//默认连续5秒无人观看然后触发kBroadcastStreamNoneReader事件
extern const string kStreamNoneReaderDelayMS;
//等待流注册超时时间，收到播放器后请求后，如果未找到相关流，服务器会等待一定时间，
//如果在这个时间内，相关流注册上了，那么服务器会立即响应播放器播放成功，
//否则会最多等待kMaxStreamWaitTimeMS毫秒，然后响应播放器播放失败
extern const string kMaxStreamWaitTimeMS;
//是否启动虚拟主机
extern const string kEnableVhost;
//拉流代理时是否添加静音音频
extern const string kAddMuteAudio;
//拉流代理时如果断流再重连成功是否删除前一次的媒体流数据，如果删除将重新开始，
//如果不删除将会接着上一次的数据继续写(录制hls/mp4时会继续在前一个文件后面写)
extern const string kResetWhenRePlay;
//是否默认推流时转换成rtsp或rtmp，hook接口(on_publish)中可以覆盖该设置
extern const string kPublishToRtxp ;
//是否默认推流时转换成hls，hook接口(on_publish)中可以覆盖该设置
extern const string kPublishToHls ;
//是否默认推流时mp4录像，hook接口(on_publish)中可以覆盖该设置
extern const string kPublishToMP4 ;
//合并写缓存大小(单位毫秒)，合并写指服务器缓存一定的数据后才会一次性写入socket，这样能提高性能，但是会提高延时
//开启后会同时关闭TCP_NODELAY并开启MSG_MORE
extern const string kMergeWriteMS ;
//全局的时间戳覆盖开关，在转协议时，对frame进行时间戳覆盖
extern const string kModifyStamp;
}//namespace General


////////////HTTP配置///////////
namespace Http {
//http 文件发送缓存大小
extern const string kSendBufSize;
//http 最大请求字节数
extern const string kMaxReqSize;
//http keep-alive秒数
extern const string kKeepAliveSecond;
//http 字符编码
extern const string kCharSet;
//http 服务器根目录
extern const string kRootPath;
//http 404错误提示内容
extern const string kNotFound;
//是否显示文件夹菜单
extern const string kDirMenu;
}//namespace Http

////////////SHELL配置///////////
namespace Shell {
extern const string kMaxReqSize;
} //namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {
//是否优先base64方式认证？默认Md5方式认证
extern const string kAuthBasic;
//握手超时时间，默认15秒
extern const string kHandshakeSecond;
//维持链接超时时间，默认15秒
extern const string kKeepAliveSecond;

//rtsp拉流代理是否直接代理
//直接代理后支持任意编码格式，但是会导致GOP缓存无法定位到I帧，可能会导致开播花屏
//并且如果是tcp方式拉流，如果rtp大于mtu会导致无法使用udp方式代理
//假定您的拉流源地址不是264或265或AAC，那么你可以使用直接代理的方式来支持rtsp代理
//默认开启rtsp直接代理，rtmp由于没有这些问题，是强制开启直接代理的
extern const string kDirectProxy;
} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
//rtmp推流时间戳覆盖开关
extern const string kModifyStamp;
//握手超时时间，默认15秒
extern const string kHandshakeSecond;
//维持链接超时时间，默认15秒
extern const string kKeepAliveSecond;
} //namespace RTMP


////////////RTP配置///////////
namespace Rtp {
//RTP打包最大MTU,公网情况下更小
extern const string kVideoMtuSize;
//RTP打包最大MTU,公网情况下更小
extern const string kAudioMtuSize;
//RTP排序缓存最大个数
extern const string kMaxRtpCount;
//如果RTP序列正确次数累计达到该数字就启动清空排序缓存
extern const string kClearCount;
//最大RTP时间为13个小时，每13小时回环一次
extern const string kCycleMS;
} //namespace Rtsp

////////////组播配置///////////
namespace MultiCast {
//组播分配起始地址
extern const string kAddrMin;
//组播分配截止地址
extern const string kAddrMax;
//组播TTL
extern const string kUdpTTL;
} //namespace MultiCast

////////////录像配置///////////
namespace Record {
//查看录像的应用名称
extern const string kAppName;
//每次流化MP4文件的时长,单位毫秒
extern const string kSampleMS;
//MP4文件录制大小,默认一个小时
extern const string kFileSecond;
//录制文件路径
extern const string kFilePath;
//mp4文件写缓存大小
extern const string kFileBufSize;
//mp4录制完成后是否进行二次关键帧索引写入头部
extern const string kFastStart;
//mp4文件是否重头循环读取
extern const string kFileRepeat;
} //namespace Record

////////////HLS相关配置///////////
namespace Hls {
//HLS切片时长,单位秒
extern const string kSegmentDuration;
//m3u8文件中HLS切片个数，如果设置为0，则不删除切片，而是保存为点播
extern const string kSegmentNum;
//HLS切片从m3u8文件中移除后，继续保留在磁盘上的个数
extern const string kSegmentRetain;
//HLS文件写缓存大小
extern const string kFileBufSize;
//录制文件路径
extern const string kFilePath;
} //namespace Hls

////////////Rtp代理相关配置///////////
namespace RtpProxy {
//rtp调试数据保存目录,置空则不生成
extern const string kDumpDir;
//是否限制udp数据来源ip和端口
extern const string kCheckSource;
//rtp接收超时时间
extern const string kTimeoutSec;
} //namespace RtpProxy

/**
 * rtsp/rtmp播放器、推流器相关设置名，
 * 这些设置项都不是配置文件用
 * 只用于设置某个播放器或推流器实例用
 */
namespace Client {
//指定网卡ip
extern const string kNetAdapter;
//设置rtp传输类型，可选项有0(tcp，默认)、1(udp)、2(组播)
//设置方法:player[PlayerBase::kRtpType] = 0/1/2;
extern const string kRtpType;
//rtsp认证用户名
extern const string kRtspUser;
//rtsp认证用用户密码，可以是明文也可以是md5,md5密码生成方式 md5(username:realm:password)
extern const string kRtspPwd;
//rtsp认证用用户密码是否为md5类型
extern const string kRtspPwdIsMD5;
//握手超时时间，默认10,000 毫秒
extern const string kTimeoutMS;
//rtp/rtmp包接收超时时间，默认5000秒
extern const string kMediaTimeoutMS;
//rtsp/rtmp心跳时间,默认5000毫秒
extern const string kBeatIntervalMS;
//Track编码格式探测最大时间，单位毫秒，默认2000
extern const string kMaxAnalysisMS;
//是否为性能测试模式，性能测试模式开启后不会解析rtp或rtmp包
extern const string kBenchmarkMode;
}
}  // namespace mediakit

#endif /* COMMON_CONFIG_H */
