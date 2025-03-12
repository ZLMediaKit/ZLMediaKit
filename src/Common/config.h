/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

#include "Util/NoticeCenter.h"
#include "Util/mini.h"
#include "Util/onceToken.h"
#include "macros.h"
#include <functional>

namespace mediakit {

class ProtocolOption;

// 加载配置文件，如果配置文件不存在，那么会导出默认配置并生成配置文件  [AUTO-TRANSLATED:16d0b898]
// Load the configuration file. If the configuration file does not exist, the default configuration will be exported and the configuration file will be generated.
// 加载配置文件成功后会触发kBroadcastUpdateConfig广播  [AUTO-TRANSLATED:327e5be2]
// After the configuration file is loaded successfully, the kBroadcastUpdateConfig broadcast will be triggered.
// 如果指定的文件名(ini_path)为空，那么会加载默认配置文件  [AUTO-TRANSLATED:e241a2b7]
// If the specified file name (ini_path) is empty, the default configuration file will be loaded.
// 默认配置文件名为 /path/to/your/exe.ini  [AUTO-TRANSLATED:2d1acfcb]
// The default configuration file name is /path/to/your/exe.ini
// 加载配置文件成功后返回true，否则返回false  [AUTO-TRANSLATED:cba43e43]
// Returns true if the configuration file is loaded successfully, otherwise returns false.
bool loadIniConfig(const char *ini_path = nullptr);

// //////////广播名称///////////  [AUTO-TRANSLATED:439b2d74]
// //////////Broadcast Name///////////
namespace Broadcast {

// 注册或反注册MediaSource事件广播  [AUTO-TRANSLATED:ec55c1cf]
// Register or unregister MediaSource event broadcast
extern const std::string kBroadcastMediaChanged;
#define BroadcastMediaChangedArgs const bool &bRegist, MediaSource &sender

// 录制mp4文件成功后广播  [AUTO-TRANSLATED:479ec954]
// Broadcast after recording mp4 file successfully
extern const std::string kBroadcastRecordMP4;
#define BroadcastRecordMP4Args const RecordInfo &info

// 录制 ts 文件后广播  [AUTO-TRANSLATED:63a8868c]
// Broadcast after recording ts file
extern const std::string kBroadcastRecordTs;
#define BroadcastRecordTsArgs const RecordInfo &info

// 收到http api请求广播  [AUTO-TRANSLATED:c72e7c3f]
// Broadcast for receiving http api request
extern const std::string kBroadcastHttpRequest;
#define BroadcastHttpRequestArgs const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, bool &consumed, SockInfo &sender

// 在http文件服务器中,收到http访问文件或目录的广播,通过该事件控制访问http目录的权限  [AUTO-TRANSLATED:2de426b4]
// In the http file server, broadcast for receiving http access to files or directories. Control access permissions to the http directory through this event.
extern const std::string kBroadcastHttpAccess;
#define BroadcastHttpAccessArgs const Parser &parser, const std::string &path, const bool &is_dir, const HttpSession::HttpAccessPathInvoker &invoker, SockInfo &sender

// 在http文件服务器中,收到http访问文件或目录前的广播,通过该事件可以控制http url到文件路径的映射  [AUTO-TRANSLATED:0294d0c5]
// In the http file server, broadcast before receiving http access to files or directories. Control the mapping from http url to file path through this event.
// 在该事件中通过自行覆盖path参数，可以做到譬如根据虚拟主机或者app选择不同http根目录的目的  [AUTO-TRANSLATED:1bea3efb]
// By overriding the path parameter in this event, you can achieve the purpose of selecting different http root directories based on virtual hosts or apps.
extern const std::string kBroadcastHttpBeforeAccess;
#define BroadcastHttpBeforeAccessArgs const Parser &parser, std::string &path, SockInfo &sender

// 该流是否需要认证？是的话调用invoker并传入realm,否则传入空的realm.如果该事件不监听则不认证  [AUTO-TRANSLATED:5f436d8f]
// Does this stream need authentication? If yes, call invoker and pass in realm, otherwise pass in an empty realm. If this event is not listened to, no authentication will be performed.
extern const std::string kBroadcastOnGetRtspRealm;
#define BroadcastOnGetRtspRealmArgs const MediaInfo &args, const RtspSession::onGetRealm &invoker, SockInfo &sender

// 请求认证用户密码事件，user_name为用户名，must_no_encrypt如果为true，则必须提供明文密码(因为此时是base64认证方式),否则会导致认证失败  [AUTO-TRANSLATED:22b6dfcc]
// Request authentication user password event, user_name is the username, must_no_encrypt if true, then the plaintext password must be provided (because it is base64 authentication method at this time), otherwise it will lead to authentication failure.
// 获取到密码后请调用invoker并输入对应类型的密码和密码类型，invoker执行时会匹配密码  [AUTO-TRANSLATED:8c57fd43]
// After getting the password, please call invoker and input the corresponding type of password and password type. The invoker will match the password when executing.
extern const std::string kBroadcastOnRtspAuth;
#define BroadcastOnRtspAuthArgs const MediaInfo &args, const std::string &realm, const std::string &user_name, const bool &must_no_encrypt, const RtspSession::onAuth &invoker, SockInfo &sender

// 推流鉴权结果回调对象  [AUTO-TRANSLATED:7e508ed1]
// Push stream authentication result callback object
// 如果err为空则代表鉴权成功  [AUTO-TRANSLATED:d49b0544]
// If err is empty, it means authentication is successful.
using PublishAuthInvoker = std::function<void(const std::string &err, const ProtocolOption &option)>;

// 收到rtsp/rtmp推流事件广播，通过该事件控制推流鉴权  [AUTO-TRANSLATED:72417373]
// Broadcast for receiving rtsp/rtmp push stream event. Control push stream authentication through this event.
extern const std::string kBroadcastMediaPublish;
#define BroadcastMediaPublishArgs const MediaOriginType &type, const MediaInfo &args, const Broadcast::PublishAuthInvoker &invoker, SockInfo &sender

// 播放鉴权结果回调对象  [AUTO-TRANSLATED:c980162b]
// Playback authentication result callback object
// 如果err为空则代表鉴权成功  [AUTO-TRANSLATED:d49b0544]
// If err is empty, it means authentication is successful.
using AuthInvoker = std::function<void(const std::string &err)>;

// 播放rtsp/rtmp/http-flv事件广播，通过该事件控制播放鉴权  [AUTO-TRANSLATED:eddd7014]
// Broadcast for playing rtsp/rtmp/http-flv events. Control playback authentication through this event.
extern const std::string kBroadcastMediaPlayed;
#define BroadcastMediaPlayedArgs const MediaInfo &args, const Broadcast::AuthInvoker &invoker, SockInfo &sender

// shell登录鉴权  [AUTO-TRANSLATED:26b135d4]
// Shell login authentication
extern const std::string kBroadcastShellLogin;
#define BroadcastShellLoginArgs const std::string &user_name, const std::string &passwd, const Broadcast::AuthInvoker &invoker, SockInfo &sender

// 停止rtsp/rtmp/http-flv会话后流量汇报事件广播  [AUTO-TRANSLATED:69df61d8]
// Broadcast for traffic reporting event after stopping rtsp/rtmp/http-flv session
extern const std::string kBroadcastFlowReport;
#define BroadcastFlowReportArgs const MediaInfo &args, const uint64_t &totalBytes, const uint64_t &totalDuration, const bool &isPlayer, SockInfo &sender

// 未找到流后会广播该事件，请在监听该事件后去拉流或其他方式产生流，这样就能按需拉流了  [AUTO-TRANSLATED:0c00171d]
// This event will be broadcast after the stream is not found. Please pull the stream or other methods to generate the stream after listening to this event, so that you can pull the stream on demand.
extern const std::string kBroadcastNotFoundStream;
#define BroadcastNotFoundStreamArgs const MediaInfo &args, SockInfo &sender, const std::function<void()> &closePlayer

// 某个流无人消费时触发，目的为了实现无人观看时主动断开拉流等业务逻辑  [AUTO-TRANSLATED:3c45f002]
// Triggered when a stream is not consumed by anyone. The purpose is to achieve business logic such as actively disconnecting the pull stream when no one is watching.
extern const std::string kBroadcastStreamNoneReader;
#define BroadcastStreamNoneReaderArgs MediaSource &sender

// rtp推流被动停止时触发  [AUTO-TRANSLATED:43881965]
// Triggered when rtp push stream is passively stopped.
extern const std::string kBroadcastSendRtpStopped;
#define BroadcastSendRtpStoppedArgs MultiMediaSourceMuxer &sender, const std::string &ssrc, const SockException &ex

// 更新配置文件事件广播,执行loadIniConfig函数加载配置文件成功后会触发该广播  [AUTO-TRANSLATED:ad4e167d]
// Update configuration file event broadcast. This broadcast will be triggered after the loadIniConfig function loads the configuration file successfully.
extern const std::string kBroadcastReloadConfig;
#define BroadcastReloadConfigArgs void

// rtp server 超时  [AUTO-TRANSLATED:a65573fd]
// Rtp server timeout
extern const std::string kBroadcastRtpServerTimeout;
#define BroadcastRtpServerTimeoutArgs uint16_t &local_port, const MediaTuple &tuple, int &tcp_mode, bool &re_use_port, uint32_t &ssrc

// rtc transport sctp 连接状态  [AUTO-TRANSLATED:f00284da]
// Rtc transport sctp connection status
extern const std::string kBroadcastRtcSctpConnecting;
extern const std::string kBroadcastRtcSctpConnected;
extern const std::string kBroadcastRtcSctpFailed;
extern const std::string kBroadcastRtcSctpClosed;
#define BroadcastRtcSctpConnectArgs WebRtcTransport& sender

// rtc transport sctp 发送数据  [AUTO-TRANSLATED:258f1ba8]
// rtc transport sctp send data
extern const std::string kBroadcastRtcSctpSend;
#define BroadcastRtcSctpSendArgs WebRtcTransport& sender, const uint8_t *&data, size_t& len

// rtc transport sctp 接收数据  [AUTO-TRANSLATED:ce4cb57e]
// rtc transport sctp receive data
extern const std::string kBroadcastRtcSctpReceived;
#define BroadcastRtcSctpReceivedArgs WebRtcTransport& sender, uint16_t &streamId, uint32_t &ppid, const uint8_t *&msg, size_t &len

// 观看人数变化广播  [AUTO-TRANSLATED:5b246b54]
// broadcast viewer count changes
extern const std::string kBroadcastPlayerCountChanged;
#define BroadcastPlayerCountChangedArgs const MediaTuple& args, const int& count

#define ReloadConfigTag ((void *)(0xFF))
#define RELOAD_KEY(arg, key)                                                                                           \
    do {                                                                                                               \
        decltype(arg) arg##_tmp = ::toolkit::mINI::Instance()[key];                                                    \
        if (arg == arg##_tmp) {                                                                                        \
            return;                                                                                                    \
        }                                                                                                              \
        arg = arg##_tmp;                                                                                               \
        InfoL << "reload config:" << key << "=" << arg;                                                                \
    } while (0)

// 监听某个配置发送变更  [AUTO-TRANSLATED:7f46b5b1]
// listen for configuration changes
#define LISTEN_RELOAD_KEY(arg, key, ...)                                                                               \
    do {                                                                                                               \
        static ::toolkit::onceToken s_token_listen([]() {                                                              \
            ::toolkit::NoticeCenter::Instance().addListener(                                                           \
                ReloadConfigTag, Broadcast::kBroadcastReloadConfig, [](BroadcastReloadConfigArgs) { __VA_ARGS__; });   \
        });                                                                                                            \
    } while (0)

#define GET_CONFIG(type, arg, key)                                                                                     \
    static type arg = ::toolkit::mINI::Instance()[key];                                                                \
    LISTEN_RELOAD_KEY(arg, key, { RELOAD_KEY(arg, key); });

#define GET_CONFIG_FUNC(type, arg, key, ...)                                                                           \
    static type arg;                                                                                                   \
    do {                                                                                                               \
        static ::toolkit::onceToken s_token_set([]() {                                                                 \
            static auto lam = __VA_ARGS__;                                                                             \
            static auto arg##_str = ::toolkit::mINI::Instance()[key];                                                  \
            arg = lam(arg##_str);                                                                                      \
            LISTEN_RELOAD_KEY(arg, key, {                                                                              \
                RELOAD_KEY(arg##_str, key);                                                                            \
                arg = lam(arg##_str);                                                                                  \
            });                                                                                                        \
        });                                                                                                            \
    } while (0)

} // namespace Broadcast

// //////////通用配置///////////  [AUTO-TRANSLATED:b09b9640]
// //////////General Configuration///////////
namespace General {
// 每个流媒体服务器的ID（GUID）  [AUTO-TRANSLATED:c6ac6e56]
// ID (GUID) of each media server
extern const std::string kMediaServerId;
// 流量汇报事件流量阈值,单位KB，默认1MB  [AUTO-TRANSLATED:fd036326]
// Traffic reporting event traffic threshold, unit KB, default 1MB
extern const std::string kFlowThreshold;
// 流无人观看并且超过若干时间后才触发kBroadcastStreamNoneReader事件  [AUTO-TRANSLATED:baeea387]
// Trigger kBroadcastStreamNoneReader event only after the stream has been unwatched for a certain period of time
// 默认连续5秒无人观看然后触发kBroadcastStreamNoneReader事件  [AUTO-TRANSLATED:477bf488]
// Default to trigger kBroadcastStreamNoneReader event after 5 seconds of no viewers
extern const std::string kStreamNoneReaderDelayMS;
// 等待流注册超时时间，收到播放器后请求后，如果未找到相关流，服务器会等待一定时间，  [AUTO-TRANSLATED:7ccd518d]
// Stream registration timeout, after receiving the player's request, if the related stream is not found, the server will wait for a certain period of time,
// 如果在这个时间内，相关流注册上了，那么服务器会立即响应播放器播放成功，  [AUTO-TRANSLATED:93ef0249]
// If the related stream is registered within this time, the server will immediately respond to the player that the playback is successful,
// 否则会最多等待kMaxStreamWaitTimeMS毫秒，然后响应播放器播放失败  [AUTO-TRANSLATED:f8c9f19e]
// Otherwise, it will wait for a maximum of kMaxStreamWaitTimeMS milliseconds and then respond to the player that the playback failed
extern const std::string kMaxStreamWaitTimeMS;
// 是否启动虚拟主机  [AUTO-TRANSLATED:e1cea728]
// Whether to enable virtual host
extern const std::string kEnableVhost;
// 拉流代理时如果断流再重连成功是否删除前一次的媒体流数据，如果删除将重新开始，  [AUTO-TRANSLATED:d150ffaa]
// When pulling stream proxy, whether to delete the previous media stream data if the stream is disconnected and reconnected successfully, if deleted, it will start again,
// 如果不删除将会接着上一次的数据继续写(录制hls/mp4时会继续在前一个文件后面写)  [AUTO-TRANSLATED:21a5be7e]
// If not deleted, it will continue to write from the previous data (when recording hls/mp4, it will continue to write after the previous file)
extern const std::string kResetWhenRePlay;
// 合并写缓存大小(单位毫秒)，合并写指服务器缓存一定的数据后才会一次性写入socket，这样能提高性能，但是会提高延时  [AUTO-TRANSLATED:6cc6fcf7]
// Merge write cache size (unit milliseconds), merge write refers to the server caching a certain amount of data before writing to the socket at once, which can improve performance but increase latency
// 开启后会同时关闭TCP_NODELAY并开启MSG_MORE  [AUTO-TRANSLATED:953b82cf]
// When enabled, TCP_NODELAY will be closed and MSG_MORE will be enabled at the same time
extern const std::string kMergeWriteMS;
// 在docker环境下，不能通过英伟达驱动是否存在来判断是否支持硬件转码  [AUTO-TRANSLATED:de678431]
// In the docker environment, the existence of the NVIDIA driver cannot be used to determine whether hardware transcoding is supported
extern const std::string kCheckNvidiaDev;
// 是否开启ffmpeg日志  [AUTO-TRANSLATED:038b471e]
// Whether to enable ffmpeg log
extern const std::string kEnableFFmpegLog;
// 最多等待未初始化的Track 10秒，超时之后会忽略未初始化的Track  [AUTO-TRANSLATED:826cd533]
// Maximum wait time for uninitialized Track is 10 seconds, after timeout, uninitialized Track will be ignored
extern const std::string kWaitTrackReadyMS;
//最多等待音频Track收到数据时间，单位毫秒，超时且完全没收到音频数据，忽略音频Track
//加快某些带封装的流metadata说明有音频，但是实际上没有的流ready时间（比如很多厂商的GB28181 PS）
extern const std::string kWaitAudioTrackDataMS;
// 如果直播流只有单Track，最多等待3秒，超时后未收到其他Track的数据，则认为是单Track  [AUTO-TRANSLATED:0e7a364d]
// If the live stream has only one Track, wait for a maximum of 3 seconds, if no data from other Tracks is received after timeout, it is considered a single Track
// 如果协议元数据有声明特定track数，那么无此等待时间  [AUTO-TRANSLATED:76606846]
// If the protocol metadata declares a specific number of tracks, there is no such waiting time
extern const std::string kWaitAddTrackMS;
// 如果track未就绪，我们先缓存帧数据，但是有最大个数限制(100帧时大约4秒)，防止内存溢出  [AUTO-TRANSLATED:c520054f]
// If the track is not ready, we will cache the frame data first, but there is a maximum number limit (100 frames is about 4 seconds) to prevent memory overflow
extern const std::string kUnreadyFrameCache;
// 是否启用观看人数变化事件广播，置1则启用，置0则关闭  [AUTO-TRANSLATED:3b7f0748]
// Whether to enable viewer count change event broadcast, set to 1 to enable, set to 0 to disable
extern const std::string kBroadcastPlayerCountChanged;
// 绑定的本地网卡ip  [AUTO-TRANSLATED:daa90832]
// Bound local network card ip
extern const std::string kListenIP;
} // namespace General

namespace Protocol {
static constexpr char kFieldName[] = "protocol.";
// 时间戳修复这一路流标志位  [AUTO-TRANSLATED:cc208f41]
// Timestamp repair flag for this stream
extern const std::string kModifyStamp;
// 转协议是否开启音频  [AUTO-TRANSLATED:220dddfa]
// Whether to enable audio for protocol conversion
extern const std::string kEnableAudio;
// 添加静音音频，在关闭音频时，此开关无效  [AUTO-TRANSLATED:47c0ec8e]
// Add silent audio, this switch is invalid when audio is closed
extern const std::string kAddMuteAudio;
// 无人观看时，是否直接关闭(而不是通过on_none_reader hook返回close)  [AUTO-TRANSLATED:dba7ab70]
// When there are no viewers, whether to close directly (instead of returning close through the on_none_reader hook)
// 此配置置1时，此流如果无人观看，将不触发on_none_reader hook回调，  [AUTO-TRANSLATED:a5ead314]
// When this configuration is set to 1, if this stream has no viewers, it will not trigger the on_none_reader hook callback,
// 而是将直接关闭流  [AUTO-TRANSLATED:06887d49]
// Instead, it will directly close the stream
extern const std::string kAutoClose;
// 断连续推延时，单位毫秒，默认采用配置文件  [AUTO-TRANSLATED:7a15b12f]
// When the continuous delay is interrupted, the unit is milliseconds, and the configuration file is used by default
extern const std::string kContinuePushMS;
// 平滑发送定时器间隔，单位毫秒，置0则关闭；开启后影响cpu性能同时增加内存  [AUTO-TRANSLATED:ad4e306a]
// Smooth sending timer interval, unit is milliseconds, set to 0 to close; enabling it will affect CPU performance and increase memory
// 该配置开启后可以解决一些流发送不平滑导致zlmediakit转发也不平滑的问题  [AUTO-TRANSLATED:0f2b1657]
// Enabling this configuration can solve some problems where the stream is not sent smoothly, resulting in ZLMediaKit forwarding not being smooth
extern const std::string kPacedSenderMS;

// 是否开启转换为hls(mpegts)  [AUTO-TRANSLATED:bfc1167a]
// Whether to enable conversion to HLS (MPEGTS)
extern const std::string kEnableHls;
// 是否开启转换为hls(fmp4)  [AUTO-TRANSLATED:20548673]
// Whether to enable conversion to HLS (FMP4)
extern const std::string kEnableHlsFmp4;
// 是否开启MP4录制  [AUTO-TRANSLATED:0157b014]
// Whether to enable MP4 recording
extern const std::string kEnableMP4;
// 是否开启转换为rtsp/webrtc  [AUTO-TRANSLATED:0711cb18]
// Whether to enable conversion to RTSP/WebRTC
extern const std::string kEnableRtsp;
// 是否开启转换为rtmp/flv  [AUTO-TRANSLATED:d4774119]
// Whether to enable conversion to RTMP/FLV
extern const std::string kEnableRtmp;
// 是否开启转换为http-ts/ws-ts  [AUTO-TRANSLATED:51acc798]
// Whether to enable conversion to HTTP-TS/WS-TS
extern const std::string kEnableTS;
// 是否开启转换为http-fmp4/ws-fmp4  [AUTO-TRANSLATED:8c96e1e4]
// Whether to enable conversion to HTTP-FMP4/WS-FMP4
extern const std::string kEnableFMP4;

// 是否将mp4录制当做观看者  [AUTO-TRANSLATED:ba351230]
// Whether to treat MP4 recording as a viewer
extern const std::string kMP4AsPlayer;
// mp4切片大小，单位秒  [AUTO-TRANSLATED:c3fb8ec1]
// MP4 fragment size, unit is seconds
extern const std::string kMP4MaxSecond;
// mp4录制保存路径  [AUTO-TRANSLATED:6d860f27]
// MP4 recording save path
extern const std::string kMP4SavePath;

// hls录制保存路径  [AUTO-TRANSLATED:cfa90719]
// HLS recording save path
extern const std::string kHlsSavePath;

// 按需转协议的开关  [AUTO-TRANSLATED:9f350899]
// On-demand protocol conversion switch
extern const std::string kHlsDemand;
extern const std::string kRtspDemand;
extern const std::string kRtmpDemand;
extern const std::string kTSDemand;
extern const std::string kFMP4Demand;
} // !Protocol

// //////////HTTP配置///////////  [AUTO-TRANSLATED:a281d694]
// //////////HTTP configuration///////////
namespace Http {
// http 文件发送缓存大小  [AUTO-TRANSLATED:51fb08c0]
// HTTP file sending cache size
extern const std::string kSendBufSize;
// http 最大请求字节数  [AUTO-TRANSLATED:8239eb9c]
// HTTP maximum request byte size
extern const std::string kMaxReqSize;
// http keep-alive秒数  [AUTO-TRANSLATED:d4930c66]
// HTTP keep-alive seconds
extern const std::string kKeepAliveSecond;
// http 字符编码  [AUTO-TRANSLATED:f7e55c83]
// HTTP character encoding
extern const std::string kCharSet;
// http 服务器根目录  [AUTO-TRANSLATED:f8f55daf]
// HTTP server root directory
extern const std::string kRootPath;
// http 服务器虚拟目录 虚拟目录名和文件路径使用","隔开，多个配置路径间用";"隔开，例如  path_d,d:/record;path_e,e:/record  [AUTO-TRANSLATED:fa4ee929]
// HTTP server virtual directory. Virtual directory name and file path are separated by ",", and multiple configuration paths are separated by ";", for example, path_d,d:/record;path_e,e:/record
extern const std::string kVirtualPath;
// http 404错误提示内容  [AUTO-TRANSLATED:91adb026]
// HTTP 404 error prompt content
extern const std::string kNotFound;
// 是否显示文件夹菜单  [AUTO-TRANSLATED:77301b85]
// Whether to display the folder menu
extern const std::string kDirMenu;
// 禁止缓存文件的后缀  [AUTO-TRANSLATED:92bcb7f7]
// Forbidden cache file suffixes
extern const std::string kForbidCacheSuffix;
// 可以把http代理前真实客户端ip放在http头中：https://github.com/ZLMediaKit/ZLMediaKit/issues/1388  [AUTO-TRANSLATED:afcd9556]
// You can put the real client IP address before the HTTP proxy in the HTTP header: https://github.com/ZLMediaKit/ZLMediaKit/issues/1388
extern const std::string kForwardedIpHeader;
// 是否允许所有跨域请求  [AUTO-TRANSLATED:2551c096]
// Whether to allow all cross-domain requests
extern const std::string kAllowCrossDomains;
// 允许访问http api和http文件索引的ip地址范围白名单，置空情况下不做限制  [AUTO-TRANSLATED:ab939863]
// Whitelist of IP address ranges allowed to access HTTP API and HTTP file index. No restrictions are imposed when empty
extern const std::string kAllowIPRange;
} // namespace Http

// //////////SHELL配置///////////  [AUTO-TRANSLATED:f023ec45]
// //////////SHELL configuration///////////
namespace Shell {
extern const std::string kMaxReqSize;
} // namespace Shell

// //////////RTSP服务器配置///////////  [AUTO-TRANSLATED:950e1981]
// //////////RTSP Server Configuration///////////
namespace Rtsp {
// 是否优先base64方式认证？默认Md5方式认证  [AUTO-TRANSLATED:0ea332b5]
// Is base64 authentication prioritized? Default is Md5 authentication
extern const std::string kAuthBasic;
// 握手超时时间，默认15秒  [AUTO-TRANSLATED:6f69a65b]
// Handshake timeout, default 15 seconds
extern const std::string kHandshakeSecond;
// 维持链接超时时间，默认15秒  [AUTO-TRANSLATED:b6339c90]
// Keep-alive timeout, default 15 seconds
extern const std::string kKeepAliveSecond;

// rtsp拉流代理是否直接代理  [AUTO-TRANSLATED:9cd82709]
// Whether RTSP pull stream proxy is direct proxy
// 直接代理后支持任意编码格式，但是会导致GOP缓存无法定位到I帧，可能会导致开播花屏  [AUTO-TRANSLATED:36525a92]
// Direct proxy supports any encoding format, but it will cause GOP cache unable to locate I-frame, which may lead to screen flickering
// 并且如果是tcp方式拉流，如果rtp大于mtu会导致无法使用udp方式代理  [AUTO-TRANSLATED:a1ab467e]
// And if it is TCP pull stream, if RTP is larger than MTU, it will not be able to use UDP proxy
// 假定您的拉流源地址不是264或265或AAC，那么你可以使用直接代理的方式来支持rtsp代理  [AUTO-TRANSLATED:9efaedcd]
// Assuming your pull stream source address is not 264 or 265 or AAC, then you can use direct proxy to support RTSP proxy
// 默认开启rtsp直接代理，rtmp由于没有这些问题，是强制开启直接代理的  [AUTO-TRANSLATED:0e55d051]
// Default to enable RTSP direct proxy, RTMP does not have these problems, it is forced to enable direct proxy
extern const std::string kDirectProxy;

// rtsp 转发是否使用低延迟模式，当开启时，不会缓存rtp包，来提高并发，可以降低一帧的延迟  [AUTO-TRANSLATED:f6fe8c6c]
// Whether RTSP forwarding uses low latency mode, when enabled, it will not cache RTP packets to improve concurrency and reduce one frame delay
extern const std::string kLowLatency;

// 强制协商rtp传输方式 (0:TCP,1:UDP,2:MULTICAST,-1:不限制)  [AUTO-TRANSLATED:38574ed5]
// Force negotiation of RTP transport method (0: TCP, 1: UDP, 2: MULTICAST, -1: no restriction)
// 当客户端发起RTSP SETUP的时候如果传输类型和此配置不一致则返回461 Unsupport Transport  [AUTO-TRANSLATED:b0fd0336]
// When the client initiates RTSP SETUP, if the transport type is inconsistent with this configuration, it will return 461 Unsupport Transport
// 迫使客户端重新SETUP并切换到对应协议。目前支持FFMPEG和VLC  [AUTO-TRANSLATED:45f9cddb]
// Force the client to re-SETUP and switch to the corresponding protocol. Currently supports FFMPEG and VLC
extern const std::string kRtpTransportType;
} // namespace Rtsp

// //////////RTMP服务器配置///////////  [AUTO-TRANSLATED:8de6f41f]
// //////////RTMP Server Configuration///////////
namespace Rtmp {
// 握手超时时间，默认15秒  [AUTO-TRANSLATED:6f69a65b]
// Handshake timeout, default 15 seconds
extern const std::string kHandshakeSecond;
// 维持链接超时时间，默认15秒  [AUTO-TRANSLATED:b6339c90]
// Keep-alive timeout, default 15 seconds
extern const std::string kKeepAliveSecond;
// 是否直接代理  [AUTO-TRANSLATED:25268b70]
// Whether direct proxy
extern const std::string kDirectProxy;
// h265-rtmp是否采用增强型(或者国内扩展)  [AUTO-TRANSLATED:4a52d042]
// Whether h265-rtmp uses enhanced (or domestic extension)
extern const std::string kEnhanced;
} // namespace Rtmp

// //////////RTP配置///////////  [AUTO-TRANSLATED:23cbcb86]
// //////////RTP Configuration///////////
namespace Rtp {
// RTP打包最大MTU,公网情况下更小  [AUTO-TRANSLATED:869f5c4b]
// Maximum RTP packet MTU, smaller in public network
extern const std::string kVideoMtuSize;
// RTP打包最大MTU,公网情况下更小  [AUTO-TRANSLATED:869f5c4b]
// Maximum RTP packet MTU, smaller in public network
extern const std::string kAudioMtuSize;
// rtp包最大长度限制, 单位KB  [AUTO-TRANSLATED:1da42584]
// Maximum RTP packet length limit, unit KB
extern const std::string kRtpMaxSize;
// rtp 打包时，低延迟开关，默认关闭（为0），h264存在一帧多个slice（NAL）的情况，在这种情况下，如果开启可能会导致画面花屏  [AUTO-TRANSLATED:4cf0cb8d]
// When RTP is packaged, low latency switch, default off (0), H264 has multiple slices (NAL) in one frame, in this case, if enabled, it may cause screen flickering
extern const std::string kLowLatency;
// H264 rtp打包模式是否采用stap-a模式(为了在老版本浏览器上兼容webrtc)还是采用Single NAL unit packet per H.264 模式  [AUTO-TRANSLATED:30632378]
// Whether H264 RTP packaging mode uses stap-a mode (for compatibility with webrtc on older browsers) or Single NAL unit packet per H.264 mode
extern const std::string kH264StapA;
} // namespace Rtp

// //////////组播配置///////////  [AUTO-TRANSLATED:dc39b9d6]
// //////////Multicast Configuration///////////
namespace MultiCast {
// 组播分配起始地址  [AUTO-TRANSLATED:069db91d]
// Multicast allocation start address
extern const std::string kAddrMin;
// 组播分配截止地址  [AUTO-TRANSLATED:6d3fc54c]
// Multicast allocation end address
extern const std::string kAddrMax;
// 组播TTL  [AUTO-TRANSLATED:c7c5339c]
// Multicast TTL
extern const std::string kUdpTTL;
} // namespace MultiCast

// //////////录像配置///////////  [AUTO-TRANSLATED:19de3e96]
// //////////Recording Configuration///////////
namespace Record {
// 查看录像的应用名称  [AUTO-TRANSLATED:a71b5daf]
// Application name for viewing recordings
extern const std::string kAppName;
// 每次流化MP4文件的时长,单位毫秒  [AUTO-TRANSLATED:0add878d]
// Duration of each MP4 file streaming, in milliseconds
extern const std::string kSampleMS;
// mp4文件写缓存大小  [AUTO-TRANSLATED:9904413d]
// MP4 file write cache size
extern const std::string kFileBufSize;
// mp4录制完成后是否进行二次关键帧索引写入头部  [AUTO-TRANSLATED:53cfdcb5]
// Whether to perform secondary keyframe index writing to the header after MP4 recording is completed
extern const std::string kFastStart;
// mp4文件是否重头循环读取  [AUTO-TRANSLATED:69ac72de]
// Whether to loop read the MP4 file from the beginning
extern const std::string kFileRepeat;
// mp4录制文件是否采用fmp4格式  [AUTO-TRANSLATED:12559ae0]
// Whether to use fmp4 format for MP4 recording files
extern const std::string kEnableFmp4;
} // namespace Record

// //////////HLS相关配置///////////  [AUTO-TRANSLATED:873cc84c]
// //////////HLS related configuration///////////
namespace Hls {
// HLS切片时长,单位秒  [AUTO-TRANSLATED:ed6a4219]
// HLS slice duration, in seconds
extern const std::string kSegmentDuration;
// m3u8文件中HLS切片个数，如果设置为0，则不删除切片，而是保存为点播  [AUTO-TRANSLATED:92388a5d]
// Number of HLS slices in the m3u8 file. If set to 0, the slices will not be deleted and will be saved as on-demand
extern const std::string kSegmentNum;
// 如果设置为0，则不保留切片，设置为1则一直保留切片  [AUTO-TRANSLATED:0933fd7b]
// If set to 0, the slices will not be retained, if set to 1, the slices will be retained all the time
extern const std::string kSegmentKeep;
// HLS切片延迟个数，大于0将生成hls_delay.m3u8文件，0则不生成  [AUTO-TRANSLATED:b1751b00]
// Number of HLS slice delays. Greater than 0 will generate hls_delay.m3u8 file, 0 will not generate
extern const std::string kSegmentDelay;
// HLS切片从m3u8文件中移除后，继续保留在磁盘上的个数  [AUTO-TRANSLATED:b7a23e1a]
// Number of HLS slices that continue to be retained on disk after being removed from the m3u8 file
extern const std::string kSegmentRetain;
// HLS文件写缓存大小  [AUTO-TRANSLATED:81832c8b]
// HLS file write cache size
extern const std::string kFileBufSize;
// 是否广播 ts 切片完成通知  [AUTO-TRANSLATED:a53644a2]
// Whether to broadcast ts slice completion notification
extern const std::string kBroadcastRecordTs;
// hls直播文件删除延时，单位秒  [AUTO-TRANSLATED:5643cab7]
// HLS live file deletion delay, in seconds
extern const std::string kDeleteDelaySec;
// 如果设置为1，则第一个切片长度强制设置为1个GOP  [AUTO-TRANSLATED:fbbb651d]
// If set to 1, the length of the first slice is forced to be 1 GOP
extern const std::string kFastRegister;
} // namespace Hls

// //////////Rtp代理相关配置///////////  [AUTO-TRANSLATED:7b285587]
// //////////Rtp proxy related configuration///////////
namespace RtpProxy {
// rtp调试数据保存目录,置空则不生成  [AUTO-TRANSLATED:aa004af0]
// Rtp debug data save directory, empty if not generated
extern const std::string kDumpDir;
// rtp接收超时时间  [AUTO-TRANSLATED:9e918489]
// Rtp receive timeout
extern const std::string kTimeoutSec;
// 随机端口范围，最少确保36个端口  [AUTO-TRANSLATED:2f2b6b17]
// Random port range, at least 36 ports are guaranteed
// 该范围同时限制rtsp服务器udp端口范围  [AUTO-TRANSLATED:1ff8fd75]
// This range also limits the rtsp server udp port range
extern const std::string kPortRange;
// rtp server h264的pt  [AUTO-TRANSLATED:b8cf877b]
// Rtp server h264 pt
extern const std::string kH264PT;
// rtp server h265的pt  [AUTO-TRANSLATED:2bdb1dfb]
// Rtp server h265 pt
extern const std::string kH265PT;
// rtp server ps 的pt  [AUTO-TRANSLATED:6feaf5f9]
// Rtp server ps pt
extern const std::string kPSPT;
// rtp server opus 的pt  [AUTO-TRANSLATED:9f91f85a]
// Rtp server opus pt
extern const std::string kOpusPT;
// startSendRtp、startRecord相关功能是否提前开启gop缓存优化级联秒开体验，默认开启, 并缓存1个GOP  [AUTO-TRANSLATED:40c37c77]
// Whether to enable gop cache optimization cascade second-open experience for startSendRtp/startRecord related functions, enabled by default, and cached 1 GOP
extern const std::string kGopCache;
// 国标发送g711 rtp 打包时，每个包的语音时长是多少，默认是100 ms，范围为20~180ms (gb28181-2016，c.2.4规定)，  [AUTO-TRANSLATED:3b3916a3]
// When sending g711 rtp packets in national standard, what is the duration of each packet, the default is 100 ms, the range is 20~180ms (gb28181-2016, c.2.4),
// 最好为20 的倍数，程序自动向20的倍数取整  [AUTO-TRANSLATED:7bc6e0ec]
// It is best to be a multiple of 20, the program automatically rounds to the nearest multiple of 20
extern const std::string kRtpG711DurMs;
// udp recv socket buffer size
extern const std::string kUdpRecvSocketBuffer;
} // namespace RtpProxy

/**
 * rtsp/rtmp播放器、推流器相关设置名，
 * 这些设置项都不是配置文件用
 * 只用于设置某个播放器或推流器实例用
 * Rtsp/rtmp player, pusher related settings name,
 * These settings are not used in the configuration file
 * Only used to set a specific player or pusher instance
 
 * [AUTO-TRANSLATED:59086953]
 */
namespace Client {
// 指定网卡ip  [AUTO-TRANSLATED:679fdccb]
// Specify network card ip
extern const std::string kNetAdapter;
// 设置rtp传输类型，可选项有0(tcp，默认)、1(udp)、2(组播)  [AUTO-TRANSLATED:bf73f779]
// Set rtp transport type, options are 0 (tcp, default), 1 (udp), 2 (multicast)
// 设置方法:player[PlayerBase::kRtpType] = 0/1/2;  [AUTO-TRANSLATED:30eb2936]
// Set method: player[PlayerBase::kRtpType] = 0/1/2;
extern const std::string kRtpType;
// rtsp播放器发送信令心跳还是rtcp心跳，可选项有0(同时发)、1(rtcp心跳)、2(信令心跳)  [AUTO-TRANSLATED:56d9ac7c]
// Whether the RTSP player sends signaling heartbeat or RTCP heartbeat, options are 0 (both), 1 (RTCP heartbeat), 2 (signaling heartbeat)
// 设置方法:player[PlayerBase::kRtspBeatType] = 0/1/2;  [AUTO-TRANSLATED:ccc0726b]
// Set method: player[PlayerBase::kRtspBeatType] = 0/1/2;
extern const std::string kRtspBeatType;
// rtsp认证用户名  [AUTO-TRANSLATED:5ab80e57]
// RTSP authentication username
extern const std::string kRtspUser;
// rtsp认证用用户密码，可以是明文也可以是md5,md5密码生成方式 md5(username:realm:password)  [AUTO-TRANSLATED:1228f997]
// RTSP authentication user password, can be plain text or MD5, MD5 password generation method md5(username:realm:password)
extern const std::string kRtspPwd;
// rtsp认证用用户密码是否为md5类型  [AUTO-TRANSLATED:208696d1]
// Whether the RTSP authentication user password is MD5 type
extern const std::string kRtspPwdIsMD5;
// 握手超时时间，默认10,000 毫秒  [AUTO-TRANSLATED:44b3f73f]
// Handshake timeout, default 10,000 milliseconds
extern const std::string kTimeoutMS;
// rtp/rtmp包接收超时时间，默认5000秒  [AUTO-TRANSLATED:e450d4cc]
// RTP/RTMP packet receive timeout, default 5000 seconds
extern const std::string kMediaTimeoutMS;
// rtsp/rtmp心跳时间,默认5000毫秒  [AUTO-TRANSLATED:4d64f27f]
// RTSP/RTMP heartbeat time, default 5000 milliseconds
extern const std::string kBeatIntervalMS;
// 是否为性能测试模式，性能测试模式开启后不会解析rtp或rtmp包  [AUTO-TRANSLATED:be9a797d]
// Whether it is performance test mode, performance test mode will not parse RTP or RTMP packets after being turned on
extern const std::string kBenchmarkMode;
// 播放器在触发播放成功事件时，是否等待所有track ready时再回调  [AUTO-TRANSLATED:73523e6d]
// Whether the player waits for all tracks to be ready before calling back when triggering the playback success event
extern const std::string kWaitTrackReady;
// rtsp播放指定track，可选项有0(不指定，默认)、1(视频)、2(音频)  [AUTO-TRANSLATED:e4f481f9]
// RTSP playback specified track, options are 0 (not specified, default), 1 (video), 2 (audio)
// 设置方法:player[Client::kPlayTrack] = 0/1/2;  [AUTO-TRANSLATED:0a2705c8]
// Set method: player[Client::kPlayTrack] = 0/1/2;
extern const std::string kPlayTrack;
// 设置代理url，目前只支持http协议  [AUTO-TRANSLATED:c84918cc]
// Set proxy url, currently only supports http protocol
extern const std::string kProxyUrl;
// 设置开始rtsp倍速播放  [AUTO-TRANSLATED:5db03cad]
// Set the start RTSP playback speed
extern const std::string kRtspSpeed;
// Set SRT delay
extern const std::string kLatency;
// Set SRT PassPhrase
extern const std::string kPassPhrase;
} // namespace Client
} // namespace mediakit

#endif /* COMMON_CONFIG_H */
