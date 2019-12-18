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

#include "common.h"
#include <stdarg.h>
#include <unordered_map>
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Network/TcpServer.h"
#include "Thread/WorkThreadPool.h"

#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

static TcpServer::Ptr rtsp_server[2];
static TcpServer::Ptr rtmp_server[2];
static TcpServer::Ptr http_server[2];

//////////////////////////environment init///////////////////////////
API_EXPORT void API_CALL mk_env_init(const config *cfg) {
    assert(cfg);
    static onceToken token([&]() {
        Logger::Instance().add(std::make_shared<ConsoleChannel>("console", (LogLevel) cfg->log_level));
        Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

        EventPollerPool::setPoolSize(cfg->thread_num);
        WorkThreadPool::setPoolSize(cfg->thread_num);

        if (cfg->ini) {
            //设置配置文件
            if (cfg->ini_is_path) {
                mINI::Instance().parseFile(cfg->ini);
            } else {
                mINI::Instance().parse(cfg->ini);
            }
        }

        if (cfg->ssl) {
            //设置ssl证书
            SSL_Initor::Instance().loadCertificate(cfg->ssl, true, cfg->ssl_pwd ? cfg->ssl_pwd : "", cfg->ssl_is_path);
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
        http_server[ssl].reset(new TcpServer());
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
        rtsp_server[ssl].reset(new TcpServer());
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
        rtmp_server[ssl].reset(new TcpServer());
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

API_EXPORT void API_CALL mk_log_printf(int level, const char *file, const char *function, int line, const char *fmt, ...) {
    assert(file && function && fmt);
    LogContextCapturer info(Logger::Instance(), (LogLevel) level, file, function, line);
    va_list pArg;
    va_start(pArg, fmt);
    char buf[4096];
    int n = vsprintf(buf, fmt, pArg);
    buf[n] = '\0';
    va_end(pArg);
    info << buf;
}

