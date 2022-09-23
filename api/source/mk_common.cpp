/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_common.h"
#include <stdarg.h>
#include <unordered_map>
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Network/TcpServer.h"
#include "Network/UdpServer.h"
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
#include "Rtp/RtpServer.h"
static std::shared_ptr<RtpServer> rtpServer;
#endif

#ifdef ENABLE_WEBRTC
#include "../webrtc/WebRtcSession.h"
static std::shared_ptr<UdpServer> rtcServer;
#endif

#if defined(ENABLE_SRT)
#include "../srt/SrtSession.hpp"
static std::shared_ptr<UdpServer> srtServer;
#endif

//////////////////////////environment init///////////////////////////

API_EXPORT void API_CALL mk_env_init(const mk_config *cfg) {
    assert(cfg);
    mk_env_init1(cfg->thread_num,
                 cfg->log_level,
                 cfg->log_mask,
                 cfg->log_file_path,
                 cfg->log_file_days,
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
    shell_server = nullptr;
#ifdef ENABLE_RTPPROXY
    rtpServer = nullptr;
#endif
#ifdef ENABLE_WEBRTC
    rtcServer = nullptr;
#endif
#ifdef ENABLE_SRT
    srtServer = nullptr;
#endif
    stopAllTcpServer();
}

API_EXPORT void API_CALL mk_env_init1(int thread_num,
                                      int log_level,
                                      int log_mask,
                                      const char *log_file_path,
                                      int log_file_days,
                                      int ini_is_path,
                                      const char *ini,
                                      int ssl_is_path,
                                      const char *ssl,
                                      const char *ssl_pwd) {
    //确保只初始化一次
    static onceToken token([&]() {
        if (log_mask & LOG_CONSOLE) {
            //控制台日志
            Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", (LogLevel) log_level));
        }

        if (log_mask & LOG_CALLBACK) {
            //广播日志
            Logger::Instance().add(std::make_shared<EventChannel>("EventChannel", (LogLevel) log_level));
        }

        if (log_mask & LOG_FILE) {
            //日志文件
            auto channel = std::make_shared<FileChannel>("FileChannel",
                                                         log_file_path ? File::absolutePath("", log_file_path) :
                                                         exeDir() + "log/", (LogLevel) log_level);
            channel->setMaxDay(log_file_days ? log_file_days : 1);
            Logger::Instance().add(channel);
        }

        //异步日志线程
        Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

        //设置线程数
        EventPollerPool::setPoolSize(thread_num);
        WorkThreadPool::setPoolSize(thread_num);

        if (ini && ini[0]) {
            //设置配置文件
            if (ini_is_path) {
                try {
                    mINI::Instance().parseFile(ini);
                } catch (std::exception &) {
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

API_EXPORT void API_CALL mk_set_log(int file_max_size, int file_max_count) {
    auto channel = dynamic_pointer_cast<FileChannel>(Logger::Instance().get("FileChannel"));
    if (channel) {
        channel->setFileMaxSize(file_max_size);
        channel->setFileMaxCount(file_max_count);
    }
}

API_EXPORT void API_CALL mk_set_option(const char *key, const char *val) {
    assert(key && val);
    if (mINI::Instance().find(key) == mINI::Instance().end()) {
        WarnL << "key:" << key << " not existed!";
        return;
    }
    mINI::Instance()[key] = val;
}

API_EXPORT const char * API_CALL mk_get_option(const char *key)
{
    assert(key);
    if (mINI::Instance().find(key) == mINI::Instance().end()) {
        WarnL << "key:" << key << " not existed!";
        return nullptr;
    }
    return mINI::Instance()[key].data();
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
        //创建rtp 服务器
        rtpServer = std::make_shared<RtpServer>();
        rtpServer->start(port);
        return rtpServer->getPort();
    } catch (std::exception &ex) {
        rtpServer.reset();
        WarnL << ex.what();
        return 0;
    }
#else
    WarnL << "未启用该功能!";
    return 0;
#endif
}

API_EXPORT uint16_t API_CALL mk_rtc_server_start(uint16_t port) {
#ifdef ENABLE_WEBRTC
    try {
        //创建rtc服务器
        rtcServer = std::make_shared<UdpServer>();
        rtcServer->setOnCreateSocket([](const EventPoller::Ptr &poller, const Buffer::Ptr &buf, struct sockaddr *, int) {
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
        rtcServer->start<WebRtcSession>(port);
        return rtcServer->getPort();

    } catch (std::exception &ex) {
        rtcServer.reset();
        WarnL << ex.what();
        return 0;
    }
#else
    WarnL << "未启用该功能!";
    return 0;
#endif
}

API_EXPORT uint16_t API_CALL mk_srt_server_start(uint16_t port) {
#ifdef ENABLE_SRT
    try {
        srtServer = std::make_shared<UdpServer>();
        srtServer->setOnCreateSocket([](const EventPoller::Ptr &poller, const Buffer::Ptr &buf, struct sockaddr *, int) {
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
        srtServer->start<SRT::SrtSession>(port);
        return srtServer->getPort();

    } catch (std::exception &ex) {
        srtServer.reset();
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
