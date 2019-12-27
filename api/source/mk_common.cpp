/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
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

#include "mk_common.h"
#include <stdarg.h>
#include <unordered_map>
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Network/TcpServer.h"
#include "Thread/WorkThreadPool.h"

#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
#include "Shell/ShellSession.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

static TcpServer::Ptr rtsp_server[2];
static TcpServer::Ptr rtmp_server[2];
static TcpServer::Ptr http_server[2];
static TcpServer::Ptr shell_server;

#ifdef ENABLE_RTPPROXY
#include "Rtp/UdpRecver.h"
#include "Rtp/RtpSession.h"
static std::shared_ptr<UdpRecver> udpRtpServer;
static TcpServer::Ptr tcpRtpServer;
#endif

//////////////////////////environment init///////////////////////////
API_EXPORT void API_CALL mk_env_init(const mk_config *cfg) {
    assert(cfg);
    mk_env_init1(cfg->thread_num,
                 cfg->log_level,
                 cfg->ini_is_path,
                 cfg->ini,
                 cfg->ssl_is_path,
                 cfg->ssl,
                 cfg->ssl_pwd);
}

extern void stopAllTcpServer();

API_EXPORT void API_CALL mk_stop_all_server(){
    CLEAR_ARR(rtsp_server);
    CLEAR_ARR(rtmp_server);
    CLEAR_ARR(http_server);
    udpRtpServer = nullptr;
    tcpRtpServer = nullptr;
    stopAllTcpServer();
}

API_EXPORT void API_CALL mk_env_init1(  int thread_num,
                                        int log_level,
                                        int ini_is_path,
                                        const char *ini,
                                        int ssl_is_path,
                                        const char *ssl,
                                        const char *ssl_pwd) {

    static onceToken token([&]() {
        Logger::Instance().add(std::make_shared<ConsoleChannel>("console", (LogLevel) log_level));
        Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

        EventPollerPool::setPoolSize(thread_num);
        WorkThreadPool::setPoolSize(thread_num);

        if (ini && ini[0]) {
            //设置配置文件
            if (ini_is_path) {
                try{
                    mINI::Instance().parseFile(ini);
                }catch (std::exception &ex) {
                    InfoL << "dump ini file to:" << ini;
                    mINI::Instance().dumpFile(ini);
                }
            } else {
                mINI::Instance().parse(ini);
            }
        }

        if (ssl && ssl[0]) {
            //设置ssl证书
            SSL_Initor::Instance().loadCertificate(ssl, true, ssl_pwd ? ssl_pwd : "", ssl_is_path);
        }
    });
}

API_EXPORT void API_CALL mk_set_option(const char *key, const char *val) {
    assert(key && val);
    if (mINI::Instance().find(key) == mINI::Instance().end()) {
        WarnL << "key:" << key << " not existed!";
        return;
    }
    mINI::Instance()[key] = val;
}

API_EXPORT uint16_t API_CALL mk_http_server_start(uint16_t port, int ssl) {
    ssl = MAX(0,MIN(ssl,1));
    try {
        http_server[ssl] = std::make_shared<TcpServer>();
        if(ssl){
            http_server[ssl]->start<TcpSessionWithSSL<HttpSession> >(port);
        } else{
            http_server[ssl]->start<HttpSession>(port);
        }
        return http_server[ssl]->getPort();
    } catch (std::exception &ex) {
        http_server[ssl].reset();
        WarnL << ex.what();
        return 0;
    }
}

API_EXPORT uint16_t API_CALL mk_rtsp_server_start(uint16_t port, int ssl) {
    ssl = MAX(0,MIN(ssl,1));
    try {
        rtsp_server[ssl] = std::make_shared<TcpServer>();
        if(ssl){
            rtsp_server[ssl]->start<TcpSessionWithSSL<RtspSession> >(port);
        }else{
            rtsp_server[ssl]->start<RtspSession>(port);
        }
        return rtsp_server[ssl]->getPort();
    } catch (std::exception &ex) {
        rtsp_server[ssl].reset();
        WarnL << ex.what();
        return 0;
    }
}

API_EXPORT uint16_t API_CALL mk_rtmp_server_start(uint16_t port, int ssl) {
    ssl = MAX(0,MIN(ssl,1));
    try {
        rtmp_server[ssl] = std::make_shared<TcpServer>();
        if(ssl){
            rtmp_server[ssl]->start<TcpSessionWithSSL<RtmpSession> >(port);
        }else{
            rtmp_server[ssl]->start<RtmpSession>(port);
        }
        return rtmp_server[ssl]->getPort();
    } catch (std::exception &ex) {
        rtmp_server[ssl].reset();
        WarnL << ex.what();
        return 0;
    }
}

API_EXPORT uint16_t API_CALL mk_rtp_server_start(uint16_t port){
#ifdef ENABLE_RTPPROXY
    try {
        //创建rtp tcp服务器
        tcpRtpServer = std::make_shared<TcpServer>();
        tcpRtpServer->start<RtpSession>(port);

        //创建rtp udp服务器
        auto ret = tcpRtpServer->getPort();
        udpRtpServer = std::make_shared<UdpRecver>();
        udpRtpServer->initSock(port);
        return ret;
    } catch (std::exception &ex) {
        tcpRtpServer.reset();
        udpRtpServer.reset();
        WarnL << ex.what();
        return 0;
    }
#else
    WarnL << "未启用该功能!";
    return 0;
#endif
}

API_EXPORT uint16_t API_CALL mk_shell_server_start(uint16_t port){
    try {
        shell_server =  std::make_shared<TcpServer>();
        shell_server->start<ShellSession>(port);
        return shell_server->getPort();
    } catch (std::exception &ex) {
        shell_server.reset();
        WarnL << ex.what();
        return 0;
    }
}
