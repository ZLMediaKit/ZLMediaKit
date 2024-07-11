﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <iostream>
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/onceToken.h"
#include "Util/CMD.h"
#include "Network/TcpServer.h"
#include "Network/UdpServer.h"
#include "Poller/EventPoller.h"
#include "Common/config.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Shell/ShellSession.h"
#include "Http/WebSocketSession.h"
#include "Rtp/RtpServer.h"
#include "WebApi.h"
#include "WebHook.h"

#if defined(ENABLE_WEBRTC)
#include "../webrtc/WebRtcTransport.h"
#include "../webrtc/WebRtcSession.h"
#endif

#if defined(ENABLE_SRT)
#include "../srt/SrtSession.hpp"
#include "../srt/SrtTransport.hpp"
#endif

#if defined(ENABLE_VERSION)
#include "ZLMVersion.h"
#endif

#if !defined(_WIN32)
#include "System.h"
#endif//!defined(_WIN32)

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace mediakit {
////////////HTTP配置///////////
namespace Http {
#define HTTP_FIELD "http."
const string kPort = HTTP_FIELD"port";
const string kSSLPort = HTTP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = 80;
    mINI::Instance()[kSSLPort] = 443;
},nullptr);
}//namespace Http

////////////SHELL配置///////////
namespace Shell {
#define SHELL_FIELD "shell."
const string kPort = SHELL_FIELD"port";
onceToken token1([](){
    mINI::Instance()[kPort] = 9000;
},nullptr);
} //namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {
#define RTSP_FIELD "rtsp."
const string kPort = RTSP_FIELD"port";
const string kSSLPort = RTSP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = 554;
    mINI::Instance()[kSSLPort] = 332;
},nullptr);

} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
#define RTMP_FIELD "rtmp."
const string kPort = RTMP_FIELD"port";
const string kSSLPort = RTMP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = 1935;
    mINI::Instance()[kSSLPort] = 19350;
},nullptr);
} //namespace RTMP

////////////Rtp代理相关配置///////////
namespace RtpProxy {
#define RTP_PROXY_FIELD "rtp_proxy."
const string kPort = RTP_PROXY_FIELD"port";
onceToken token1([](){
    mINI::Instance()[kPort] = 10000;
},nullptr);
} //namespace RtpProxy

}  // namespace mediakit


class CMD_main : public CMD {
public:
    CMD_main() {
        _parser = std::make_shared<OptionParser>(nullptr);

#if !defined(_WIN32)
        (*_parser) << Option('d',/*该选项简称，如果是\x00则说明无简称*/
                             "daemon",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgNone,/*该选项后面必须跟值*/
                             nullptr,/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "是否以Daemon方式启动",/*该选项说明文字*/
                             nullptr);
#endif//!defined(_WIN32)

        (*_parser) << Option('l',/*该选项简称，如果是\x00则说明无简称*/
                             "level",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(LDebug).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "日志等级,LTrace~LError(0~4)",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('m',/*该选项简称，如果是\x00则说明无简称*/
                             "max_day",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "7",/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "日志最多保存天数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('c',/*该选项简称，如果是\x00则说明无简称*/
                             "config",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             (exeDir() + "config.ini").data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "配置文件路径",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('s',/*该选项简称，如果是\x00则说明无简称*/
                             "ssl",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             (exeDir() + "default.pem").data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "ssl证书文件或文件夹,支持p12/pem类型",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('t',/*该选项简称，如果是\x00则说明无简称*/
                             "threads",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(thread::hardware_concurrency()).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动事件触发线程数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option(0,/*该选项简称，如果是\x00则说明无简称*/
                             "affinity",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(1).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "是否启动cpu亲和性设置",/*该选项说明文字*/
                             nullptr);

#if defined(ENABLE_VERSION)
        (*_parser) << Option('v', "version", Option::ArgNone, nullptr, false, "显示版本号",
                             [](const std::shared_ptr<ostream> &stream, const string &arg) -> bool {
                                 //版本信息
                                 *stream << "编译日期: " << BUILD_TIME << std::endl;
                                 *stream << "代码日期: " << COMMIT_TIME << std::endl;
                                 *stream << "当前git分支: " << BRANCH_NAME << std::endl;
                                 *stream << "当前git hash值: " << COMMIT_HASH << std::endl;
                                 throw ExitException();
                             });
#endif
        (*_parser) << Option(0,/*该选项简称，如果是\x00则说明无简称*/
                             "log-slice",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "100",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "最大保存日志切片个数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option(0,/*该选项简称，如果是\x00则说明无简称*/
                             "log-size",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "256",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "单个日志切片最大容量,单位MB",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option(0,/*该选项简称，如果是\x00则说明无简称*/
                             "log-dir",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             (exeDir() + "log/").data(),/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "日志保存文件夹路径",/*该选项说明文字*/
                             nullptr);
    }
};

//全局变量，在WebApi中用于保存配置文件用
string g_ini_file;

int start_main(int argc,char *argv[]) {
    {
        CMD_main cmd_main;
        try {
            cmd_main.operator()(argc, argv);
        } catch (ExitException &) {
            return 0;
        } catch (std::exception &ex) {
            cout << ex.what() << endl;
            return -1;
        }

        bool bDaemon = cmd_main.hasKey("daemon");
        LogLevel logLevel = (LogLevel) cmd_main["level"].as<int>();
        logLevel = MIN(MAX(logLevel, LTrace), LError);
        g_ini_file = cmd_main["config"];
        string ssl_file = cmd_main["ssl"];
        int threads = cmd_main["threads"];
        bool affinity = cmd_main["affinity"];

        //设置日志
        Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", logLevel));
#if !defined(ANDROID)
        auto fileChannel = std::make_shared<FileChannel>("FileChannel", cmd_main["log-dir"], logLevel);
        // 日志最多保存天数
        fileChannel->setMaxDay(cmd_main["max_day"]);
        fileChannel->setFileMaxCount(cmd_main["log-slice"]);
        fileChannel->setFileMaxSize(cmd_main["log-size"]);
        Logger::Instance().add(fileChannel);
#endif // !defined(ANDROID)

#if !defined(_WIN32)
        pid_t pid = getpid();
        bool kill_parent_if_failed = true;
        if (bDaemon) {
            //启动守护进程
            System::startDaemon(kill_parent_if_failed);
        }
        //开启崩溃捕获等
        System::systemSetup();
#endif//!defined(_WIN32)

        //启动异步日志线程
        Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

        InfoL << kServerName;

        //加载配置文件，如果配置文件不存在就创建一个
        loadIniConfig(g_ini_file.data());

        auto &secret = mINI::Instance()[API::kSecret];
        if (secret == "035c73f7-bb6b-4889-a715-d9eb2d1925cc" || secret.empty()) {
            // 使用默认secret被禁止启动
            secret = makeRandStr(32, true);
            mINI::Instance().dumpFile(g_ini_file);
            WarnL << "The " << API::kSecret << " is invalid, modified it to: " << secret
                  << ", saved config file: " << g_ini_file;
        }

        if (!File::is_dir(ssl_file)) {
            // 不是文件夹，加载证书，证书包含公钥和私钥
            SSL_Initor::Instance().loadCertificate(ssl_file.data());
        } else {
            //加载文件夹下的所有证书
            File::scanDir(ssl_file,[](const string &path, bool isDir){
                if (!isDir) {
                    // 最后的一个证书会当做默认证书(客户端ssl握手时未指定主机)
                    SSL_Initor::Instance().loadCertificate(path.data());
                }
                return true;
            });
        }

        uint16_t shellPort = mINI::Instance()[Shell::kPort];
        uint16_t rtspPort = mINI::Instance()[Rtsp::kPort];
        uint16_t rtspsPort = mINI::Instance()[Rtsp::kSSLPort];
        uint16_t rtmpPort = mINI::Instance()[Rtmp::kPort];
        uint16_t rtmpsPort = mINI::Instance()[Rtmp::kSSLPort];
        uint16_t httpPort = mINI::Instance()[Http::kPort];
        uint16_t httpsPort = mINI::Instance()[Http::kSSLPort];
        uint16_t rtpPort = mINI::Instance()[RtpProxy::kPort];

        //设置poller线程数和cpu亲和性,该函数必须在使用ZLToolKit网络相关对象之前调用才能生效
        //如果需要调用getSnap和addFFmpegSource接口，可以关闭cpu亲和性

        EventPollerPool::setPoolSize(threads);
        WorkThreadPool::setPoolSize(threads);
        EventPollerPool::enableCpuAffinity(affinity);

        //简单的telnet服务器，可用于服务器调试，但是不能使用23端口，否则telnet上了莫名其妙的现象
        //测试方法:telnet 127.0.0.1 9000
        auto shellSrv = std::make_shared<TcpServer>();

        //rtsp[s]服务器, 可用于诸如亚马逊echo show这样的设备访问
        auto rtspSrv = std::make_shared<TcpServer>();
        auto rtspSSLSrv = std::make_shared<TcpServer>();

        //rtmp[s]服务器
        auto rtmpSrv = std::make_shared<TcpServer>();
        auto rtmpsSrv = std::make_shared<TcpServer>();

        //http[s]服务器
        auto httpSrv = std::make_shared<TcpServer>();
        auto httpsSrv = std::make_shared<TcpServer>();

#if defined(ENABLE_RTPPROXY)
        //GB28181 rtp推流端口，支持UDP/TCP
        auto rtpServer = std::make_shared<RtpServer>();
#endif//defined(ENABLE_RTPPROXY)

#if defined(ENABLE_WEBRTC)
        auto rtcSrv_tcp = std::make_shared<TcpServer>();
        //webrtc udp服务器
        auto rtcSrv_udp = std::make_shared<UdpServer>();
        rtcSrv_udp->setOnCreateSocket([](const EventPoller::Ptr &poller, const Buffer::Ptr &buf, struct sockaddr *, int) {
            if (!buf) {
                return Socket::createSocket(poller, false);
            }
            auto new_poller = WebRtcSession::queryPoller(buf);
            if (!new_poller) {
                //该数据对应的webrtc对象未找到，丢弃之
                return Socket::Ptr();
            }
            return Socket::createSocket(new_poller, false);
        });
        uint16_t rtcPort = mINI::Instance()[Rtc::kPort];
        uint16_t rtcTcpPort = mINI::Instance()[Rtc::kTcpPort];
#endif//defined(ENABLE_WEBRTC)


#if defined(ENABLE_SRT)
        auto srtSrv = std::make_shared<UdpServer>();
        srtSrv->setOnCreateSocket([](const EventPoller::Ptr &poller, const Buffer::Ptr &buf, struct sockaddr *, int) {
            if (!buf) {
                return Socket::createSocket(poller, false);
            }
            auto new_poller = SRT::SrtSession::queryPoller(buf);
            if (!new_poller) {
                //握手第一阶段
                return Socket::createSocket(poller, false);
            }
            return Socket::createSocket(new_poller, false);
        });

        uint16_t srtPort = mINI::Instance()[SRT::kPort];
#endif //defined(ENABLE_SRT)

        installWebApi();
        InfoL << "已启动http api 接口";
        installWebHook();
        InfoL << "已启动http hook 接口";

        try {
            //rtsp服务器，端口默认554
            if (rtspPort) { rtspSrv->start<RtspSession>(rtspPort); }
            //rtsps服务器，端口默认322
            if (rtspsPort) { rtspSSLSrv->start<RtspSessionWithSSL>(rtspsPort); }

            //rtmp服务器，端口默认1935
            if (rtmpPort) { rtmpSrv->start<RtmpSession>(rtmpPort); }
            //rtmps服务器，端口默认19350
            if (rtmpsPort) { rtmpsSrv->start<RtmpSessionWithSSL>(rtmpsPort); }

            //http服务器，端口默认80
            if (httpPort) { httpSrv->start<HttpSession>(httpPort); }
            //https服务器，端口默认443
            if (httpsPort) { httpsSrv->start<HttpsSession>(httpsPort); }

            //telnet远程调试服务器
            if (shellPort) { shellSrv->start<ShellSession>(shellPort); }

#if defined(ENABLE_RTPPROXY)
            //创建rtp服务器
            if (rtpPort) { rtpServer->start(rtpPort); }
#endif//defined(ENABLE_RTPPROXY)

#if defined(ENABLE_WEBRTC)
            //webrtc udp服务器
            if (rtcPort) { rtcSrv_udp->start<WebRtcSession>(rtcPort);}

            if (rtcTcpPort) { rtcSrv_tcp->start<WebRtcSession>(rtcTcpPort);}
             
#endif//defined(ENABLE_WEBRTC)

#if defined(ENABLE_SRT)
            // srt udp服务器
            if (srtPort) { srtSrv->start<SRT::SrtSession>(srtPort); }
#endif//defined(ENABLE_SRT)

        } catch (std::exception &ex) {
            ErrorL << "Start server failed: " << ex.what();
            sleep(1);
#if !defined(_WIN32)
            if (pid != getpid() && kill_parent_if_failed) {
                //杀掉守护进程
                kill(pid, SIGINT);
            }
#endif
            return -1;
        }

        //设置退出信号处理函数
        static semaphore sem;
        signal(SIGINT, [](int) {
            InfoL << "SIGINT:exit";
            signal(SIGINT, SIG_IGN); // 设置退出信号
            sem.post();
        }); // 设置退出信号

        signal(SIGTERM,[](int) {
            WarnL << "SIGTERM:exit";
            signal(SIGTERM, SIG_IGN);
            sem.post();
        });

#if !defined(_WIN32)
        signal(SIGHUP, [](int) { mediakit::loadIniConfig(g_ini_file.data()); });
#endif
        sem.wait();
    }
    unInstallWebApi();
    unInstallWebHook();
    onProcessExited();

    //休眠1秒再退出，防止资源释放顺序错误
    InfoL << "程序退出中,请等待...";
    sleep(1);
    InfoL << "程序退出完毕!";
    return 0;
}

#ifndef DISABLE_MAIN
int main(int argc,char *argv[]) {
    return start_main(argc,argv);
}
#endif //DISABLE_MAIN


