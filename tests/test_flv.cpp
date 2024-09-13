/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <iostream>
#include "Util/logger.h"
#include "Util/util.h"
#include "Network/TcpServer.h"
#include "Common/config.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
#include "Rtmp/FlvSplitter.h"
#include "Rtmp/RtmpMediaSourceImp.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

class FlvSplitterImp : public FlvSplitter {
public:
    FlvSplitterImp() {
        _src = std::make_shared<RtmpMediaSourceImp>(MediaTuple{DEFAULT_VHOST, "live", "test", ""});
    }
    ~FlvSplitterImp()  override = default;

    void inputData(const char *data, size_t len, uint32_t &stamp) {
        FlvSplitter::input(data, len);
        stamp = _stamp;
    }

protected:
    void onRecvFlvHeader(const FLVHeader &header) override {
    }

    bool onRecvMetadata(const AMFValue &metadata) override {
        _src->setMetaData(metadata);
        _src->setProtocolOption(ProtocolOption());
        return true;
    }

    void onRecvRtmpPacket(RtmpPacket::Ptr packet) override {
        _stamp = packet->time_stamp;
        _src->onWrite(std::move(packet));
    }

private:
    uint32_t _stamp;
    RtmpMediaSourceImp::Ptr _src;
};

static bool loadFile(const char *path){
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        WarnL << "open file failed:" << path;
        return false;
    }

    char buffer[64 * 1024];
    uint32_t timeStamp_last = 0;
    size_t len;
    size_t total_size = 0;
    FlvSplitterImp flv;
    while (true) {
        len = fread(buffer, 1, sizeof(buffer), fp);
        if (len <= 0) {
            break;
        }
        total_size += len;
        uint32_t timeStamp;
        flv.inputData(buffer, len, timeStamp);
        auto diff = timeStamp - timeStamp_last;
        if (diff > 0) {
            usleep(diff * 1000);
        } else {
            usleep(1 * 1000);
        }
        timeStamp_last = timeStamp;
    }
    WarnL << total_size / 1024 << "KB";
    fclose(fp);
    return true;
}

int main(int argc,char *argv[]) {
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

    if (argc == 2)
      loadFile(argv[1]);
    else
      ErrorL << "parameter error.";
    return 0;
}


