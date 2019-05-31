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

#include <map>
#include <signal.h>
#include <iostream>
#include "Util/MD5.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/onceToken.h"
#include "Util/CMD.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"
#include "Common/config.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Shell/ShellSession.h"
#include "RtmpMuxer/FlvMuxer.h"
#include "Player/PlayerProxy.h"
#include "Http/WebSocketSession.h"
#include "System.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace mediakit {
////////////HTTP配置///////////
namespace Http {
#define HTTP_FIELD "http."
#define HTTP_PORT 80
const char kPort[] = HTTP_FIELD"port";
#define HTTPS_PORT 443
const char kSSLPort[] = HTTP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = HTTP_PORT;
    mINI::Instance()[kSSLPort] = HTTPS_PORT;
},nullptr);
}//namespace Http

////////////SHELL配置///////////
namespace Shell {
#define SHELL_FIELD "shell."
#define SHELL_PORT 9000
const char kPort[] = SHELL_FIELD"port";
onceToken token1([](){
    mINI::Instance()[kPort] = SHELL_PORT;
},nullptr);
} //namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {
#define RTSP_FIELD "rtsp."
#define RTSP_PORT 554
#define RTSPS_PORT 322
const char kPort[] = RTSP_FIELD"port";
const char kSSLPort[] = RTSP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = RTSP_PORT;
    mINI::Instance()[kSSLPort] = RTSPS_PORT;
},nullptr);

} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
#define RTMP_FIELD "rtmp."
#define RTMP_PORT 1935
const char kPort[] = RTMP_FIELD"port";
onceToken token1([](){
    mINI::Instance()[kPort] = RTMP_PORT;
},nullptr);
} //namespace RTMP
}  // namespace mediakit


class CMD_main : public CMD {
public:
    CMD_main() {
        _parser.reset(new OptionParser(nullptr));

        (*_parser) << Option('d',/*该选项简称，如果是\x00则说明无简称*/
                             "daemon",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgNone,/*该选项后面必须跟值*/
                             nullptr,/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "是否以Daemon方式启动",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('l',/*该选项简称，如果是\x00则说明无简称*/
                             "level",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(LTrace).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "日志等级,LTrace~LError(0~4)",/*该选项说明文字*/
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
                             "ssl证书路径,支持p12/pem类型",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('t',/*该选项简称，如果是\x00则说明无简称*/
                             "threads",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(thread::hardware_concurrency()).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动事件触发线程数",/*该选项说明文字*/
                             nullptr);
    }

    virtual ~CMD_main() {}
    virtual const char *description() const {
        return "主程序命令参数";
    }
};


extern void installWebApi();
extern void unInstallWebApi();
extern void installWebHook();
extern void unInstallWebHook();

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
int main(int argc,char *argv[]) {
    {
        CMD_main cmd_main;
        try {
            cmd_main.operator()(argc, argv);
        } catch (std::exception &ex) {
            cout << ex.what() << endl;
            return -1;
        }

        bool bDaemon = cmd_main.hasKey("daemon");
        LogLevel logLevel = (LogLevel) cmd_main["level"].as<int>();
        logLevel = MIN(MAX(logLevel, LTrace), LError);
        static string ini_file = cmd_main["config"];
        string ssl_file = cmd_main["ssl"];
        int threads = cmd_main["threads"];

        //设置日志
        Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", logLevel));
#if defined(__linux__) || defined(__linux)
        Logger::Instance().add(std::make_shared<SysLogChannel>("SysLogChannel",logLevel));
#else
        Logger::Instance().add(std::make_shared<FileChannel>("FileChannel", exePath() + ".log", logLevel));
#endif

        if (bDaemon) {
            //启动守护进程
            System::startDaemon();
        }

        //开启崩溃捕获等
        System::systemSetup();

        //启动异步日志线程
        Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
        //加载配置文件，如果配置文件不存在就创建一个
        loadIniConfig(ini_file.data());

        //加载证书，证书包含公钥和私钥
        SSL_Initor::Instance().loadCertificate(ssl_file.data());
        //信任某个自签名证书
        SSL_Initor::Instance().trustCertificate(ssl_file.data());
        //不忽略无效证书证书(例如自签名或过期证书)
        SSL_Initor::Instance().ignoreInvalidCertificate(true);

        uint16_t shellPort = mINI::Instance()[Shell::kPort];
        uint16_t rtspPort = mINI::Instance()[Rtsp::kPort];
        uint16_t rtspsPort = mINI::Instance()[Rtsp::kSSLPort];
        uint16_t rtmpPort = mINI::Instance()[Rtmp::kPort];
        uint16_t httpPort = mINI::Instance()[Http::kPort];
        uint16_t httpsPort = mINI::Instance()[Http::kSSLPort];

        //设置poller线程数,该函数必须在使用ZLToolKit网络相关对象之前调用才能生效
        EventPollerPool::setPoolSize(threads);

        //简单的telnet服务器，可用于服务器调试，但是不能使用23端口，否则telnet上了莫名其妙的现象
        //测试方法:telnet 127.0.0.1 9000
        TcpServer::Ptr shellSrv(new TcpServer());
        TcpServer::Ptr rtspSrv(new TcpServer());
        TcpServer::Ptr rtmpSrv(new TcpServer());
        TcpServer::Ptr httpSrv(new TcpServer());

        shellSrv->start<ShellSession>(shellPort);
        rtspSrv->start<RtspSession>(rtspPort);//默认554
        rtmpSrv->start<RtmpSession>(rtmpPort);//默认1935
        //http服务器,支持websocket
        httpSrv->start<EchoWebSocketSession>(httpPort);//默认80

        //如果支持ssl，还可以开启https服务器
        TcpServer::Ptr httpsSrv(new TcpServer());
        //https服务器,支持websocket
        httpsSrv->start<SSLEchoWebSocketSession>(httpsPort);//默认443

        //支持ssl加密的rtsp服务器，可用于诸如亚马逊echo show这样的设备访问
        TcpServer::Ptr rtspSSLSrv(new TcpServer());
        rtspSSLSrv->start<RtspSessionWithSSL>(rtspsPort);//默认322

        installWebApi();
        InfoL << "已启动http api 接口";
        installWebHook();
        InfoL << "已启动http hook 接口";

        if (!bDaemon) {
            //交互式shell输入
            listen_shell_input();
        }

        //设置退出信号处理函数
        static semaphore sem;
        signal(SIGINT, [](int) {
            InfoL << "SIGINT:exit";
            signal(SIGINT, SIG_IGN);// 设置退出信号
            sem.post();
        });// 设置退出信号
        signal(SIGHUP, [](int) { mediakit::loadIniConfig(ini_file.data()); });
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

