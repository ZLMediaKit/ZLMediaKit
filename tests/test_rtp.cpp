/*
 * MIT License
 *
 * Copyright (c) 2016-2019 Gemfield <gemfield@civilnet.cn>
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
#include <iostream>
#include "Util/MD5.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/util.h"
#include "Network/TcpServer.h"
#include "Common/config.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
#include "Rtp/RtpFileLoader.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

int main(int argc,char *argv[]) {
    {
        //设置日志
        Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel"));
        //启动异步日志线程
        Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
        loadIniConfig((exeDir() + "config.ini").data());
        TcpServer::Ptr rtspSrv(new TcpServer());
        TcpServer::Ptr rtmpSrv(new TcpServer());
        TcpServer::Ptr httpSrv(new TcpServer());
        rtspSrv->start<RtspSession>(554);//默认554
        rtmpSrv->start<RtmpSession>(1935);//默认1935
        httpSrv->start<HttpSession>(80);//默认80
        RtpFileLoader::loadFile(argv[1]);
    }
    return 0;
}



