/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
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
#include "Poller/EventPoller.h"
#include "Common/config.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Shell/ShellSession.h"
#include "Http/WebSocketSession.h"
#include "Rtp/RtpServer.h"
#include "WebApi.h"
#include "WebHook.h"
#include "../webrtc/Sdp.h"

#if defined(ENABLE_VERSION)
#include "Version.h"
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
        _parser.reset(new OptionParser(nullptr));

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
                             to_string(LTrace).data(),/*该选项默认值*/
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
                             (exeDir() + "ssl.p12").data(),/*该选项默认值*/
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

#if defined(ENABLE_VERSION)
        (*_parser) << Option('v', "version", Option::ArgNone, nullptr, false, "显示版本号",
                             [](const std::shared_ptr<ostream> &stream, const string &arg) -> bool {
                                 //版本信息
                                 *stream << "编译日期: " << build_time << std::endl;
                                 *stream << "当前git分支: " << branch_name << std::endl;
                                 *stream << "当前git hash值: " << commit_hash << std::endl;
                                 throw ExitException();
                             });
#endif
    }

    ~CMD_main() override{}
    const char *description() const override{
        return "主程序命令参数";
    }
};

#if !defined(_WIN32)
static void inline listen_shell_input(){
    cout << "> 欢迎进入命令模式，你可以输入\"help\"命令获取帮助" << endl;
    cout << "> " << std::flush;

    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);

    SockUtil::setNoBlocked(STDIN_FILENO);
    auto oninput = [](int event) {
        if (event & Event_Read) {
            char buf[1024];
            int n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n > 0) {
                buf[n] = '\0';
                try {
                    CMDRegister::Instance()(buf);
                    cout << "> " << std::flush;
                } catch (ExitException &ex) {
                    InfoL << "ExitException";
                    kill(getpid(), SIGINT);
                } catch (std::exception &ex) {
                    cout << ex.what() << endl;
                }
            } else {
                DebugL << get_uv_errmsg();
                EventPollerPool::Instance().getFirstPoller()->delEvent(STDIN_FILENO);
            }
        }

        if (event & Event_Error) {
            WarnL << "Event_Error";
            EventPollerPool::Instance().getFirstPoller()->delEvent(STDIN_FILENO);
        }
    };
    EventPollerPool::Instance().getFirstPoller()->addEvent(STDIN_FILENO, Event_Read | Event_Error | Event_LT,oninput);
}
#endif//!defined(_WIN32)

//全局变量，在WebApi中用于保存配置文件用
string g_ini_file;


void test_sdp(){
    char str1[] = "v=0\n"
                  "o=- 380154348540553537 2 IN IP4 127.0.0.1\n"
                  "s=-\n"
                  "b=CT:1900\n"
                  "t=0 0\n"
                  "a=group:BUNDLE video\n"
                  "a=msid-semantic: WMS\n"
                  "m=video 9 RTP/SAVPF 96\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=rtcp:9 IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:1ZFN\n"
                  "a=ice-pwd:70P3H0jPlGz1fiJl5XZfXMZH\n"
                  "a=ice-options:trickle\n"
                  "a=fingerprint:sha-256 3E:10:35:6B:9A:9E:B0:55:AC:2A:88:F5:74:C1:70:32:B5:8D:88:1D:37:B0:9C:69:A6:DD:07:10:73:27:1A:16\n"
                  "a=setup:active\n"
                  "a=mid:video\n"
                  "a=recvonly\n"
                  "a=rtcp-mux\n"
                  "a=rtpmap:96 H264/90000\n"
                  "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f";
    char str2[] = "v=0\n"
                  "o=- 2584450093346841581 2 IN IP4 127.0.0.1\n"
                  "s=-\n"
                  "t=0 0\n"
                  "a=group:BUNDLE audio video data\n"
                  "a=msid-semantic: WMS 616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=rtcp:9 IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:sXJ3\n"
                  "a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV\n"
                  "a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79\n"
                  "a=setup:actpass\n"
                  "a=mid:audio\n"
                  "a=extmap:1/sendonly urn:ietf:params:rtp-hdrext:ssrc-audio-level\n"
                  "a=sendrecv\n"
                  "a=rtcp-mux\n"
                  "a=rtpmap:111 opus/48000/2\n"
                  "a=rtcp-fb:111 transport-cc\n"
                  "a=fmtp:111 minptime=10;useinbandfec=1\n"
                  "a=rtpmap:103 ISAC/16000\n"
                  "a=rtpmap:104 ISAC/32000\n"
                  "a=rtpmap:9 G722/8000\n"
                  "a=rtpmap:0 PCMU/8000\n"
                  "a=rtpmap:8 PCMA/8000\n"
                  "a=rtpmap:106 CN/32000\n"
                  "a=rtpmap:105 CN/16000\n"
                  "a=rtpmap:13 CN/8000\n"
                  "a=rtpmap:110 telephone-event/48000\n"
                  "a=rtpmap:112 telephone-event/32000\n"
                  "a=rtpmap:113 telephone-event/16000\n"
                  "a=rtpmap:126 telephone-event/8000\n"
                  "a=ssrc:120276603 cname:iSkJ2vn5cYYubTve\n"
                  "a=ssrc:120276603 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 1da3d329-7399-4fe9-b20f-69606bebd363\n"
                  "a=ssrc:120276603 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "a=ssrc:120276603 label:1da3d329-7399-4fe9-b20f-69606bebd363\n"
                  "m=video 9 UDP/TLS/RTP/SAVPF 96 98 100 102 127 97 99 101 125\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=rtcp:9 IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:sXJ3\n"
                  "a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV\n"
                  "a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79\n"
                  "a=setup:actpass\n"
                  "a=mid:video\n"
                  "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\n"
                  "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\n"
                  "a=extmap:4 urn:3gpp:video-orientation\n"
                  "a=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\n"
                  "a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\n"
                  "a=sendrecv\n"
                  "a=rtcp-mux\n"
                  "a=rtcp-rsize\n"
                  "a=rtpmap:96 VP8/90000\n"
                  "a=rtcp-fb:96 ccm fir\n"
                  "a=rtcp-fb:96 nack\n"
                  "a=rtcp-fb:96 nack pli\n"
                  "a=rtcp-fb:96 goog-remb\n"
                  "a=rtcp-fb:96 transport-cc\n"
                  "a=rtpmap:98 VP9/90000\n"
                  "a=rtcp-fb:98 ccm fir\n"
                  "a=rtcp-fb:98 nack\n"
                  "a=rtcp-fb:98 nack pli\n"
                  "a=rtcp-fb:98 goog-remb\n"
                  "a=rtcp-fb:98 transport-cc\n"
                  "a=rtpmap:100 H264/90000\n"
                  "a=rtcp-fb:100 ccm fir\n"
                  "a=rtcp-fb:100 nack\n"
                  "a=rtcp-fb:100 nack pli\n"
                  "a=rtcp-fb:100 goog-remb\n"
                  "a=rtcp-fb:100 transport-cc\n"
                  "a=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\n"
                  "a=rtpmap:102 red/90000\n"
                  "a=rtpmap:127 ulpfec/90000\n"
                  "a=rtpmap:97 rtx/90000\n"
                  "a=fmtp:97 apt=96\n"
                  "a=rtpmap:99 rtx/90000\n"
                  "a=fmtp:99 apt=98\n"
                  "a=rtpmap:101 rtx/90000\n"
                  "a=fmtp:101 apt=100\n"
                  "a=rtpmap:125 rtx/90000\n"
                  "a=fmtp:125 apt=102\n"
                  "a=ssrc-group:FID 2580761338 611523443\n"
                  "a=ssrc:2580761338 cname:iSkJ2vn5cYYubTve\n"
                  "a=ssrc:2580761338 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=ssrc:2580761338 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "a=ssrc:2580761338 label:bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=ssrc:611523443 cname:iSkJ2vn5cYYubTve\n"
                  "a=ssrc:611523443 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=ssrc:611523443 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "a=ssrc:611523443 label:bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=candidate:3575467457 1 udp 2113937151 10.15.83.23 57857 typ host generation 0 ufrag 6R0z network-cost 999\n"
                  "m=application 9 DTLS/SCTP 5000\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:sXJ3\n"
                  "a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV\n"
                  "a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79\n"
                  "a=setup:actpass\n"
                  "a=mid:data\n"
                  "a=sctpmap:5000 webrtc-datachannel 1024\n"
                  "a=sctp-port:5000";

    RtcSessionSdp sdp1;
    sdp1.parse(str1);

    RtcSessionSdp sdp2;
    sdp2.parse(str2);

    for (auto media : sdp1.medias) {
        InfoL << getRtpDirectionString(media.getDirection());
    }
    for (auto media : sdp2.medias) {
        InfoL << getRtpDirectionString(media.getDirection());
    }
    InfoL << sdp1.toString();
    InfoL << sdp2.toString();

    RtcSession session1;
    session1.loadFrom(str1);

    RtcSession session2;
    session2.loadFrom(str2);
    DebugL << session1.toString();
    DebugL << session2.toString();
}

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

        //设置日志
        Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", logLevel));
#ifndef ANDROID
        auto fileChannel = std::make_shared<FileChannel>("FileChannel", exeDir() + "log/", logLevel);
        //日志最多保存天数
        fileChannel->setMaxDay(cmd_main["max_day"]);
        Logger::Instance().add(fileChannel);
#endif//

#if !defined(_WIN32)
        pid_t pid = getpid();
        if (bDaemon) {
            //启动守护进程
            System::startDaemon();
        }
        //开启崩溃捕获等
        System::systemSetup();
#endif//!defined(_WIN32)

        //启动异步日志线程
        Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
        //加载配置文件，如果配置文件不存在就创建一个
        loadIniConfig(g_ini_file.data());

        if(!File::is_dir(ssl_file.data())){
            //不是文件夹，加载证书，证书包含公钥和私钥
            SSL_Initor::Instance().loadCertificate(ssl_file.data());
        }else{
            //加载文件夹下的所有证书
            File::scanDir(ssl_file,[](const string &path, bool isDir){
                if(!isDir){
                    //最后的一个证书会当做默认证书(客户端ssl握手时未指定主机)
                    SSL_Initor::Instance().loadCertificate(path.data());
                }
                return true;
            });
        }

//        test_sdp();
        uint16_t shellPort = mINI::Instance()[Shell::kPort];
        uint16_t rtspPort = mINI::Instance()[Rtsp::kPort];
        uint16_t rtspsPort = mINI::Instance()[Rtsp::kSSLPort];
        uint16_t rtmpPort = mINI::Instance()[Rtmp::kPort];
        uint16_t rtmpsPort = mINI::Instance()[Rtmp::kSSLPort];
        uint16_t httpPort = mINI::Instance()[Http::kPort];
        uint16_t httpsPort = mINI::Instance()[Http::kSSLPort];
        uint16_t rtpPort = mINI::Instance()[RtpProxy::kPort];

        //设置poller线程数,该函数必须在使用ZLToolKit网络相关对象之前调用才能生效
        EventPollerPool::setPoolSize(threads);

        //简单的telnet服务器，可用于服务器调试，但是不能使用23端口，否则telnet上了莫名其妙的现象
        //测试方法:telnet 127.0.0.1 9000
        TcpServer::Ptr shellSrv(new TcpServer());

        //rtsp[s]服务器, 可用于诸如亚马逊echo show这样的设备访问
        TcpServer::Ptr rtspSrv(new TcpServer());
        TcpServer::Ptr rtspSSLSrv(new TcpServer());

        //rtmp[s]服务器
        TcpServer::Ptr rtmpSrv(new TcpServer());
        TcpServer::Ptr rtmpsSrv(new TcpServer());

        //http[s]服务器
        TcpServer::Ptr httpSrv(new TcpServer());
        TcpServer::Ptr httpsSrv(new TcpServer());

#if defined(ENABLE_RTPPROXY)
        //GB28181 rtp推流端口，支持UDP/TCP
        RtpServer::Ptr rtpServer = std::make_shared<RtpServer>();
#endif//defined(ENABLE_RTPPROXY)

        try {
            //rtsp服务器，端口默认554
            if(rtspPort) { rtspSrv->start<RtspSession>(rtspPort); }
            //rtsps服务器，端口默认322
            if(rtspsPort) { rtspSSLSrv->start<RtspSessionWithSSL>(rtspsPort); }

            //rtmp服务器，端口默认1935
            if(rtmpPort) { rtmpSrv->start<RtmpSession>(rtmpPort); }
            //rtmps服务器，端口默认19350
            if(rtmpsPort) { rtmpsSrv->start<RtmpSessionWithSSL>(rtmpsPort); }

            //http服务器，端口默认80
            if(httpPort) { httpSrv->start<HttpSession>(httpPort); }
            //https服务器，端口默认443
            if(httpsPort) { httpsSrv->start<HttpsSession>(httpsPort); }

            //telnet远程调试服务器
            if(shellPort) { shellSrv->start<ShellSession>(shellPort); }

#if defined(ENABLE_RTPPROXY)
            //创建rtp服务器
            if(rtpPort){ rtpServer->start(rtpPort); }
#endif//defined(ENABLE_RTPPROXY)

        }catch (std::exception &ex){
            WarnL << "端口占用或无权限:" << ex.what() << endl;
            ErrorL << "程序启动失败，请修改配置文件中端口号后重试!" << endl;
            sleep(1);
#if !defined(_WIN32)
            if(pid != getpid()){
                kill(pid,SIGINT);
            }
#endif
            return -1;
        }

        installWebApi();
        InfoL << "已启动http api 接口";
        installWebHook();
        InfoL << "已启动http hook 接口";

#if !defined(_WIN32) && !defined(ANDROID)
        if (!bDaemon) {
            //交互式shell输入
            listen_shell_input();
        }
#endif

        //设置退出信号处理函数
        static semaphore sem;
        signal(SIGINT, [](int) {
            InfoL << "SIGINT:exit";
            signal(SIGINT, SIG_IGN);// 设置退出信号
            sem.post();
        });// 设置退出信号

#if !defined(_WIN32)
        signal(SIGHUP, [](int) { mediakit::loadIniConfig(g_ini_file.data()); });
#endif
        sem.wait();
    }
    unInstallWebApi();
    unInstallWebHook();
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


