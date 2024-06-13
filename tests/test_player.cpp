
#include "Codec/Transcode.h"
#include "Common/Device.h"
#include "Common/config.h"
#include "Common/macros.h"
#include "Player/MediaPlayer.h"
#include "Record/Recorder.h"
#include "Rtsp/UDPServer.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/util.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <signal.h>
#include <unistd.h>

#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"
#include "Util/MD5.h"
#include "Util/SSLBox.h"
#include "Util/logger.h"
#include "Util/onceToken.h"

#include "Common/config.h"
#include "Http/WebSocketSession.h"
#include "Player/PlayerProxy.h"
#include "Rtmp/FlvMuxer.h"
#include "Rtmp/RtmpSession.h"
#include "Rtsp/RtspSession.h"
#include "Rtsp/UDPServer.h"
#include "Shell/ShellSession.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

static std::shared_ptr<DevChannel> g_dev;

void test_player(const std::string &url) {

    // 设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());

    g_dev = std::make_shared<mediakit::DevChannel>(mediakit::MediaTuple { DEFAULT_VHOST, "live", "test" });

    auto player = std::make_shared<MediaPlayer>();
    // sdl要求在main线程初始化
    weak_ptr<MediaPlayer> weakPlayer = player;
    player->setOnPlayResult([weakPlayer](const SockException &ex) {
        InfoL << "OnPlayResult:" << ex.what();
        auto strongPlayer = weakPlayer.lock();
        if (ex || !strongPlayer) {
            return;
        }

        auto videoTrack = dynamic_pointer_cast<VideoTrack>(strongPlayer->getTrack(TrackVideo, false));
        auto audioTrack = dynamic_pointer_cast<AudioTrack>(strongPlayer->getTrack(TrackAudio, false));

        if (videoTrack) {
            VideoInfo info;
            info.codecId = videoTrack->getCodecId();
            info.iWidth = videoTrack->getVideoWidth();
            info.iHeight = videoTrack->getVideoHeight();
            info.iFrameRate = videoTrack->getVideoFps();

            g_dev->initVideo(info);
            videoTrack->addDelegate([](const Frame::Ptr &frame) { return g_dev->inputFrame(frame); });
        }

        if (audioTrack) {
            AudioInfo info;
            info.codecId = audioTrack->getCodecId();
            info.iChannel = audioTrack->getAudioChannel();
            info.iSampleBit = audioTrack->getAudioSampleBit();
            info.iSampleRate = audioTrack->getAudioSampleRate();
            g_dev->initAudio(info);

            audioTrack->addDelegate([](const Frame::Ptr &frame) { return g_dev->inputFrame(frame); });
        }
    });

    player->setOnShutdown([](const SockException &ex) { WarnL << "play shutdown: " << ex.what(); });

    (*player)[Client::kRtpType] = mediakit::Rtsp::RTP_TCP;
    // 不等待track ready再回调播放成功事件，这样可以加快秒开速度
    (*player)[Client::kWaitTrackReady] = true;
    player->play(url);

    getchar();
}

namespace mediakit {
////////////HTTP配置///////////
namespace Http {
#define HTTP_FIELD "http."
#define HTTP_PORT 80
const string kPort = HTTP_FIELD "port";
#define HTTPS_PORT 443
const string kSSLPort = HTTP_FIELD "sslport";
onceToken token1(
    []() {
        mINI::Instance()[kPort] = HTTP_PORT;
        mINI::Instance()[kSSLPort] = HTTPS_PORT;
    },
    nullptr);
} // namespace Http

////////////SHELL配置///////////
namespace Shell {
#define SHELL_FIELD "shell."
#define SHELL_PORT 9000
const string kPort = SHELL_FIELD "port";
onceToken token1([]() { mINI::Instance()[kPort] = SHELL_PORT; }, nullptr);
} // namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {
#define RTSP_FIELD "rtsp."
#define RTSP_PORT 554
#define RTSPS_PORT 322
const string kPort = RTSP_FIELD "port";
const string kSSLPort = RTSP_FIELD "sslport";
onceToken token1(
    []() {
        mINI::Instance()[kPort] = RTSP_PORT;
        mINI::Instance()[kSSLPort] = RTSPS_PORT;
    },
    nullptr);

} // namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
#define RTMP_FIELD "rtmp."
#define RTMP_PORT 3935
const string kPort = RTMP_FIELD "port";
onceToken token1([]() { mINI::Instance()[kPort] = RTMP_PORT; }, nullptr);
} // namespace Rtmp
} // namespace mediakit

#define REALM "realm_zlmediakit"
static map<string, FlvRecorder::Ptr> s_mapFlvRecorder;
static mutex s_mtxFlvRecorder;

void initEventListener() {
    static onceToken s_token(
        []() {
            // 监听kBroadcastOnGetRtspRealm事件决定rtsp链接是否需要鉴权(传统的rtsp鉴权方案)才能访问
            NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastOnGetRtspRealm, [](BroadcastOnGetRtspRealmArgs) {
                DebugL << "RTSP是否需要鉴权事件：" << args.getUrl() << " " << args.params;
                if (string("1") == args.stream) {
                    // live/1需要认证
                    // 该流需要认证，并且设置realm
                    invoker(REALM);
                } else {
                    // 有时我们要查询redis或数据库来判断该流是否需要认证，通过invoker的方式可以做到完全异步
                    // 该流我们不需要认证
                    invoker("");
                }
            });

            // 监听kBroadcastOnRtspAuth事件返回正确的rtsp鉴权用户密码
            NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastOnRtspAuth, [](BroadcastOnRtspAuthArgs) {
                DebugL << "RTSP播放鉴权:" << args.getUrl() << " " << args.params;
                DebugL << "RTSP用户：" << user_name << (must_no_encrypt ? " Base64" : " MD5") << " 方式登录";
                string user = user_name;
                // 假设我们异步读取数据库
                if (user == "test0") {
                    // 假设数据库保存的是明文
                    invoker(false, "pwd0");
                    return;
                }

                if (user == "test1") {
                    // 假设数据库保存的是密文
                    auto encrypted_pwd = MD5(user + ":" + REALM + ":" + "pwd1").hexdigest();
                    invoker(true, encrypted_pwd);
                    return;
                }
                if (user == "test2" && must_no_encrypt) {
                    // 假设登录的是test2,并且以base64方式登录，此时我们提供加密密码，那么会导致认证失败
                    // 可以通过这个方式屏蔽base64这种不安全的加密方式
                    invoker(true, "pwd2");
                    return;
                }

                // 其他用户密码跟用户名一致
                invoker(false, user);
            });

            // 监听rtsp/rtmp推流事件，返回结果告知是否有推流权限
            NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaPublish, [](BroadcastMediaPublishArgs) {
                DebugL << "推流鉴权：" << args.getUrl() << " " << args.params;
                invoker("", ProtocolOption()); // 鉴权成功
                                               // invoker("this is auth failed message");//鉴权失败
            });

            // 监听rtsp/rtsps/rtmp/http-flv播放事件，返回结果告知是否有播放权限(rtsp通过kBroadcastOnRtspAuth或此事件都可以实现鉴权)
            NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaPlayed, [](BroadcastMediaPlayedArgs) {
                DebugL << "播放鉴权:" << args.getUrl() << " " << args.params;
                invoker(""); // 鉴权成功
                             // invoker("this is auth failed message");//鉴权失败
            });

            // shell登录事件，通过shell可以登录进服务器执行一些命令
            NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastShellLogin, [](BroadcastShellLoginArgs) {
                DebugL << "shell login:" << user_name << " " << passwd;
                invoker(""); // 鉴权成功
                             // invoker("this is auth failed message");//鉴权失败
            });

            // 监听rtsp、rtmp源注册或注销事件；此处用于测试rtmp保存为flv录像，保存在http根目录下
            NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaChanged, [](BroadcastMediaChangedArgs) {
                auto tuple = sender.getMediaTuple();
                if (sender.getSchema() == RTMP_SCHEMA && tuple.app == "live") {
                    lock_guard<mutex> lck(s_mtxFlvRecorder);
                    auto key = tuple.shortUrl();
                    if (bRegist) {
                        DebugL << "开始录制RTMP：" << sender.getUrl();
                        GET_CONFIG(string, http_root, Http::kRootPath);
                        auto path = http_root + "/" + key + "_" + to_string(time(NULL)) + ".flv";
                        FlvRecorder::Ptr recorder(new FlvRecorder);
                        try {
                            recorder->startRecord(
                                EventPollerPool::Instance().getPoller(), dynamic_pointer_cast<RtmpMediaSource>(sender.shared_from_this()), path);
                            s_mapFlvRecorder[key] = recorder;
                        } catch (std::exception &ex) {
                            WarnL << ex.what();
                        }
                    } else {
                        s_mapFlvRecorder.erase(key);
                    }
                }
            });

            // 监听播放失败(未找到特定的流)事件
            NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastNotFoundStream, [](BroadcastNotFoundStreamArgs) {
                /**
                 * 你可以在这个事件触发时再去拉流，这样就可以实现按需拉流
                 * 拉流成功后，ZLMediaKit会把其立即转发给播放器(最大等待时间约为5秒，如果5秒都未拉流成功，播放器会播放失败)
                 */
                DebugL << "未找到流事件:" << args.getUrl() << " " << args.params;
            });

            // 监听播放或推流结束时消耗流量事件
            NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastFlowReport, [](BroadcastFlowReportArgs) {
                DebugL << "播放器(推流器)断开连接事件:" << args.getUrl() << " " << args.params << "\r\n使用流量:" << totalBytes
                       << " bytes,连接时长:" << totalDuration << "秒";
            });
        },
        nullptr);
}

#if !defined(SIGHUP)
#define SIGHUP 1
#endif

int main(int argc, char *argv[]) {
    // 设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().add(std::make_shared<FileChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    if (argc < 2) {
        ErrorL << "\r\n测试方法：./test_player rtxp_url\r\n";

        return 0;
    }

    std::string url = argv[1];
    // 加载配置文件，如果配置文件不存在就创建一个
    loadIniConfig();
    initEventListener();

    // 加载证书，证书包含公钥和私钥
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    // 信任某个自签名证书
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    // 不忽略无效证书证书(例如自签名或过期证书)
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    uint16_t shellPort = mINI::Instance()[Shell::kPort];
    uint16_t rtspPort = mINI::Instance()[Rtsp::kPort];
    uint16_t rtspsPort = mINI::Instance()[Rtsp::kSSLPort];
    uint16_t rtmpPort = 2935;
    uint16_t httpPort = 8088;
    uint16_t httpsPort = mINI::Instance()[Http::kSSLPort];

    // 简单的telnet服务器，可用于服务器调试，但是不能使用23端口，否则telnet上了莫名其妙的现象
    // 测试方法:telnet 127.0.0.1 9000
    TcpServer::Ptr shellSrv(new TcpServer());
    TcpServer::Ptr rtspSrv(new TcpServer());
    TcpServer::Ptr rtmpSrv(new TcpServer());
    TcpServer::Ptr httpSrv(new TcpServer());

    shellSrv->start<ShellSession>(shellPort);
    rtspSrv->start<RtspSession>(rtspPort); // 默认554
    rtmpSrv->start<RtmpSession>(rtmpPort); // 默认1935
    // http服务器
    httpSrv->start<HttpSession>(httpPort); // 默认80

    // 如果支持ssl，还可以开启https服务器
    TcpServer::Ptr httpsSrv(new TcpServer());
    // https服务器
    httpsSrv->start<HttpsSession>(httpsPort); // 默认443

    // 支持ssl加密的rtsp服务器，可用于诸如亚马逊echo show这样的设备访问
    TcpServer::Ptr rtspSSLSrv(new TcpServer());
    rtspSSLSrv->start<RtspSessionWithSSL>(rtspsPort); // 默认322

    // 服务器支持动态切换端口(不影响现有连接)
    NoticeCenter::Instance().addListener(ReloadConfigTag, Broadcast::kBroadcastReloadConfig, [&](BroadcastReloadConfigArgs) {
        // 重新创建服务器
        if (shellPort != mINI::Instance()[Shell::kPort].as<uint16_t>()) {
            shellPort = mINI::Instance()[Shell::kPort];
            shellSrv->start<ShellSession>(shellPort);
            InfoL << "重启shell服务器:" << shellPort;
        }
        if (rtspPort != mINI::Instance()[Rtsp::kPort].as<uint16_t>()) {
            rtspPort = mINI::Instance()[Rtsp::kPort];
            rtspSrv->start<RtspSession>(rtspPort);
            InfoL << "重启rtsp服务器" << rtspPort;
        }
        if (rtmpPort != mINI::Instance()[Rtmp::kPort].as<uint16_t>()) {
            rtmpPort = mINI::Instance()[Rtmp::kPort];
            rtmpSrv->start<RtmpSession>(rtmpPort);
            InfoL << "重启rtmp服务器" << rtmpPort;
        }
        if (httpPort != mINI::Instance()[Http::kPort].as<uint16_t>()) {
            httpPort = mINI::Instance()[Http::kPort];
            httpSrv->start<HttpSession>(httpPort);
            InfoL << "重启http服务器" << httpPort;
        }
        if (httpsPort != mINI::Instance()[Http::kSSLPort].as<uint16_t>()) {
            httpsPort = mINI::Instance()[Http::kSSLPort];
            httpsSrv->start<HttpsSession>(httpsPort);
            InfoL << "重启https服务器" << httpsPort;
        }

        if (rtspsPort != mINI::Instance()[Rtsp::kSSLPort].as<uint16_t>()) {
            rtspsPort = mINI::Instance()[Rtsp::kSSLPort];
            rtspSSLSrv->start<RtspSessionWithSSL>(rtspsPort);
            InfoL << "重启rtsps服务器" << rtspsPort;
        }
    });

    test_player(url);
    // 设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); }); // 设置退出信号
    signal(SIGHUP, [](int) { loadIniConfig(); });
    sem.wait();

    lock_guard<mutex> lck(s_mtxFlvRecorder);
    s_mapFlvRecorder.clear();
    return 0;
}
