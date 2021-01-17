/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <signal.h>
#include <iostream>

#include "Util/MD5.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/onceToken.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"

#include "Common/config.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Shell/ShellSession.h"
#include "Rtmp/FlvMuxer.h"
#include "Player/PlayerProxy.h"
#include "Http/WebSocketSession.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace mediakit {
////////////HTTP配置///////////
namespace Http {
#define HTTP_FIELD "http."
#define HTTP_PORT 80
const string kPort = HTTP_FIELD"port";
#define HTTPS_PORT 443
const string kSSLPort = HTTP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = HTTP_PORT;
    mINI::Instance()[kSSLPort] = HTTPS_PORT;
},nullptr);
}//namespace Http

////////////SHELL配置///////////
namespace Shell {
#define SHELL_FIELD "shell."
#define SHELL_PORT 9000
const string kPort = SHELL_FIELD"port";
onceToken token1([](){
    mINI::Instance()[kPort] = SHELL_PORT;
},nullptr);
} //namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {
#define RTSP_FIELD "rtsp."
#define RTSP_PORT 554
#define RTSPS_PORT 322
const string kPort = RTSP_FIELD"port";
const string kSSLPort = RTSP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = RTSP_PORT;
    mINI::Instance()[kSSLPort] = RTSPS_PORT;
},nullptr);

} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
#define RTMP_FIELD "rtmp."
#define RTMP_PORT 1935
const string kPort = RTMP_FIELD"port";
onceToken token1([](){
    mINI::Instance()[kPort] = RTMP_PORT;
},nullptr);
} //namespace RTMP
}  // namespace mediakit


#define REALM "realm_zlmedaikit"
static map<string,FlvRecorder::Ptr> s_mapFlvRecorder;
static mutex s_mtxFlvRecorder;

void initEventListener() {
    static onceToken s_token([]() {
        //监听kBroadcastOnGetRtspRealm事件决定rtsp链接是否需要鉴权(传统的rtsp鉴权方案)才能访问
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastOnGetRtspRealm,
                                             [](BroadcastOnGetRtspRealmArgs) {
                                                 DebugL << "RTSP是否需要鉴权事件：" << args._schema << " " << args._vhost << " "
                                                        << args._app << " " << args._streamid << " "
                                                        << args._param_strs;
                                                 if (string("1") == args._streamid) {
                                                     // live/1需要认证
                                                     //该流需要认证，并且设置realm
                                                     invoker(REALM);
                                                 } else {
                                                     //有时我们要查询redis或数据库来判断该流是否需要认证，通过invoker的方式可以做到完全异步
                                                     //该流我们不需要认证
                                                     invoker("");
                                                 }
                                             });

        //监听kBroadcastOnRtspAuth事件返回正确的rtsp鉴权用户密码
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastOnRtspAuth, [](BroadcastOnRtspAuthArgs) {
            DebugL << "RTSP播放鉴权:" << args._schema << " " << args._vhost << " " << args._app << " " << args._streamid
                   << " " << args._param_strs;
            DebugL << "RTSP用户：" << user_name << (must_no_encrypt ? " Base64" : " MD5") << " 方式登录";
            string user = user_name;
            //假设我们异步读取数据库
            if (user == "test0") {
                //假设数据库保存的是明文
                invoker(false, "pwd0");
                return;
            }

            if (user == "test1") {
                //假设数据库保存的是密文
                auto encrypted_pwd = MD5(user + ":" + REALM + ":" + "pwd1").hexdigest();
                invoker(true, encrypted_pwd);
                return;
            }
            if (user == "test2" && must_no_encrypt) {
                //假设登录的是test2,并且以base64方式登录，此时我们提供加密密码，那么会导致认证失败
                //可以通过这个方式屏蔽base64这种不安全的加密方式
                invoker(true, "pwd2");
                return;
            }

            //其他用户密码跟用户名一致
            invoker(false, user);
        });


        //监听rtsp/rtmp推流事件，返回结果告知是否有推流权限
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaPublish, [](BroadcastMediaPublishArgs) {
            DebugL << "推流鉴权：" << args._schema << " " << args._vhost << " " << args._app << " " << args._streamid << " "
                   << args._param_strs;
            invoker("", true, false);//鉴权成功
            //invoker("this is auth failed message");//鉴权失败
        });

        //监听rtsp/rtsps/rtmp/http-flv播放事件，返回结果告知是否有播放权限(rtsp通过kBroadcastOnRtspAuth或此事件都可以实现鉴权)
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaPlayed, [](BroadcastMediaPlayedArgs) {
            DebugL << "播放鉴权:" << args._schema << " " << args._vhost << " " << args._app << " " << args._streamid << " "
                   << args._param_strs;
            invoker("");//鉴权成功
            //invoker("this is auth failed message");//鉴权失败
        });

        //shell登录事件，通过shell可以登录进服务器执行一些命令
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastShellLogin, [](BroadcastShellLoginArgs) {
            DebugL << "shell login:" << user_name << " " << passwd;
            invoker("");//鉴权成功
            //invoker("this is auth failed message");//鉴权失败
        });

        //监听rtsp、rtmp源注册或注销事件；此处用于测试rtmp保存为flv录像，保存在http根目录下
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaChanged, [](BroadcastMediaChangedArgs) {
            if (sender.getSchema() == RTMP_SCHEMA && sender.getApp() == "live") {
                lock_guard<mutex> lck(s_mtxFlvRecorder);
                if (bRegist) {
                    DebugL << "开始录制RTMP：" << sender.getSchema() << " " << sender.getVhost() << " " << sender.getApp() << " " << sender.getId();
                    GET_CONFIG(string, http_root, Http::kRootPath);
                    auto path =
                            http_root + "/" + sender.getVhost() + "/" + sender.getApp() + "/" + sender.getId() + "_" + to_string(time(NULL)) + ".flv";
                    FlvRecorder::Ptr recorder(new FlvRecorder);
                    try {
                        recorder->startRecord(EventPollerPool::Instance().getPoller(),
                                              dynamic_pointer_cast<RtmpMediaSource>(sender.shared_from_this()), path);
                        s_mapFlvRecorder[sender.getVhost() + "/" + sender.getApp() + "/" + sender.getId()] = recorder;
                    } catch (std::exception &ex) {
                        WarnL << ex.what();
                    }
                } else {
                    s_mapFlvRecorder.erase(sender.getVhost() + "/" + sender.getApp() + "/" + sender.getId());
                }
            }
        });

        //监听播放失败(未找到特定的流)事件
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastNotFoundStream,
                                             [](BroadcastNotFoundStreamArgs) {
                                                 /**
                                                  * 你可以在这个事件触发时再去拉流，这样就可以实现按需拉流
                                                  * 拉流成功后，ZLMediaKit会把其立即转发给播放器(最大等待时间约为5秒，如果5秒都未拉流成功，播放器会播放失败)
                                                  */
                                                 DebugL << "未找到流事件:" << args._schema << " " << args._vhost << " "
                                                        << args._app << " " << args._streamid << " "
                                                        << args._param_strs;
                                             });


        //监听播放或推流结束时消耗流量事件
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastFlowReport, [](BroadcastFlowReportArgs) {
            DebugL << "播放器(推流器)断开连接事件:" << args._schema << " " << args._vhost << " " << args._app << " "
                   << args._streamid << " " << args._param_strs
                   << "\r\n使用流量:" << totalBytes << " bytes,连接时长:" << totalDuration << "秒";

        });


    }, nullptr);
}

#if !defined(SIGHUP)
#define SIGHUP 1
#endif

int main(int argc,char *argv[]) {
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().add(std::make_shared<FileChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    //加载配置文件，如果配置文件不存在就创建一个
    loadIniConfig();
    initEventListener();

    //这里是拉流地址，支持rtmp/rtsp协议，负载必须是H264+AAC
    //如果是其他不识别的音视频将会被忽略(譬如说h264+adpcm转发后会去除音频)
    auto urlList = {"rtsp://admin:admin123@192.168.1.64:554/cam/realmonitor?channel=1&subtype=1"
            //rtsp链接支持输入用户名密码
            /*"rtsp://admin:jzan123456@192.168.0.122/"*/};
    map<string, PlayerProxy::Ptr> proxyMap;
    int i = 0;
    for (auto &url : urlList) {
        //PlayerProxy构造函数前两个参数分别为应用名（app）,流id（streamId）
        //比如说应用为live，流id为0，那么直播地址为:

        //hls地址 : http://127.0.0.1/live/0/hls.m3u8
        //http-flv地址 : http://127.0.0.1/live/0.flv
        //rtsp地址 : rtsp://127.0.0.1/live/0
        //rtmp地址 : rtmp://127.0.0.1/live/0

        //录像地址为(当然vlc不支持这么多级的rtmp url，可以用test_player测试rtmp点播):
        //http://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
        //rtsp://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4
        //rtmp://127.0.0.1/record/live/0/2017-04-11/11-09-38.mp4

        PlayerProxy::Ptr player(new PlayerProxy(DEFAULT_VHOST, "live", std::string("chn") + to_string(i).data()));
        //指定RTP over TCP(播放rtsp时有效)
        (*player)[kRtpType] = Rtsp::RTP_TCP;
        //开始播放，如果播放失败或者播放中止，将会自动重试若干次，重试次数在配置文件中配置，默认一直重试
        player->play(url);
        //需要保存PlayerProxy，否则作用域结束就会销毁该对象
        proxyMap.emplace(to_string(i), player);
        ++i;
    }

    DebugL << "\r\n"
              " PlayerProxy构造函数前两个参数分别为应用名（app）,流id（streamId）\n"
              " 比如说应用为live，流id为0，那么直播地址为:\n"
              " hls地址 : http://127.0.0.1/live/0/hls.m3u8\n"
              " http-flv地址 : http://127.0.0.1/live/0.flv\n"
              " rtsp地址 : rtsp://127.0.0.1/live/0\n"
              " rtmp地址 : rtmp://127.0.0.1/live/0";

    //加载证书，证书包含公钥和私钥
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    //信任某个自签名证书
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    //不忽略无效证书证书(例如自签名或过期证书)
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    uint16_t shellPort = mINI::Instance()[Shell::kPort];
    uint16_t rtspPort = mINI::Instance()[Rtsp::kPort];
    uint16_t rtspsPort = mINI::Instance()[Rtsp::kSSLPort];
    uint16_t rtmpPort = mINI::Instance()[Rtmp::kPort];
    uint16_t httpPort = mINI::Instance()[Http::kPort];
    uint16_t httpsPort = mINI::Instance()[Http::kSSLPort];

    //简单的telnet服务器，可用于服务器调试，但是不能使用23端口，否则telnet上了莫名其妙的现象
    //测试方法:telnet 127.0.0.1 9000
    TcpServer::Ptr shellSrv(new TcpServer());
    TcpServer::Ptr rtspSrv(new TcpServer());
    TcpServer::Ptr rtmpSrv(new TcpServer());
    TcpServer::Ptr httpSrv(new TcpServer());

    shellSrv->start<ShellSession>(shellPort);
    rtspSrv->start<RtspSession>(rtspPort);//默认554
    rtmpSrv->start<RtmpSession>(rtmpPort);//默认1935
    //http服务器
    httpSrv->start<HttpSession>(httpPort);//默认80

    //如果支持ssl，还可以开启https服务器
    TcpServer::Ptr httpsSrv(new TcpServer());
    //https服务器
    httpsSrv->start<HttpsSession>(httpsPort);//默认443

    //支持ssl加密的rtsp服务器，可用于诸如亚马逊echo show这样的设备访问
    TcpServer::Ptr rtspSSLSrv(new TcpServer());
    rtspSSLSrv->start<RtspSessionWithSSL>(rtspsPort);//默认322

    //服务器支持动态切换端口(不影响现有连接)
    NoticeCenter::Instance().addListener(ReloadConfigTag,Broadcast::kBroadcastReloadConfig,[&](BroadcastReloadConfigArgs){
        //重新创建服务器
        if(shellPort != mINI::Instance()[Shell::kPort].as<uint16_t>()){
            shellPort = mINI::Instance()[Shell::kPort];
            shellSrv->start<ShellSession>(shellPort);
            InfoL << "重启shell服务器:" << shellPort;
        }
        if(rtspPort != mINI::Instance()[Rtsp::kPort].as<uint16_t>()){
            rtspPort = mINI::Instance()[Rtsp::kPort];
            rtspSrv->start<RtspSession>(rtspPort);
            InfoL << "重启rtsp服务器" << rtspPort;
        }
        if(rtmpPort != mINI::Instance()[Rtmp::kPort].as<uint16_t>()){
            rtmpPort = mINI::Instance()[Rtmp::kPort];
            rtmpSrv->start<RtmpSession>(rtmpPort);
            InfoL << "重启rtmp服务器" << rtmpPort;
        }
        if(httpPort != mINI::Instance()[Http::kPort].as<uint16_t>()){
            httpPort = mINI::Instance()[Http::kPort];
            httpSrv->start<HttpSession>(httpPort);
            InfoL << "重启http服务器" << httpPort;
        }
        if(httpsPort != mINI::Instance()[Http::kSSLPort].as<uint16_t>()){
            httpsPort = mINI::Instance()[Http::kSSLPort];
            httpsSrv->start<HttpsSession>(httpsPort);
            InfoL << "重启https服务器" << httpsPort;
        }

        if(rtspsPort != mINI::Instance()[Rtsp::kSSLPort].as<uint16_t>()){
            rtspsPort = mINI::Instance()[Rtsp::kSSLPort];
            rtspSSLSrv->start<RtspSessionWithSSL>(rtspsPort);
            InfoL << "重启rtsps服务器" << rtspsPort;
        }
    });

    //设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    signal(SIGHUP, [](int) { loadIniConfig(); });
    sem.wait();

    lock_guard<mutex> lck(s_mtxFlvRecorder);
    s_mapFlvRecorder.clear();
    return 0;
}

