#include <map>
#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Network/TcpServer.h"
#include "Common/config.h"
#include "Player/PlayerProxy.h"
#include "Http/WebSocketSession.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

//此程序为zlm的拉流代理性能测试工具，用于测试拉流代理性能
int main(int argc, char *argv[]) {
    {
        Logger::Instance().add(std::make_shared<ConsoleChannel>());
        Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

        EventPollerPool::Instance().preferCurrentThread(false);
        mINI::Instance()[General::kRtspDemand] = atoi(argv[3]);
        mINI::Instance()[General::kRtmpDemand] = atoi(argv[3]);
        mINI::Instance()[General::kHlsDemand] = 1;
        mINI::Instance()[General::kTSDemand] = 1;
        mINI::Instance()[General::kFMP4Demand] = 1;

        string url = argv[1];
        int count = atoi(argv[2]);
        map<string, PlayerProxy::Ptr> proxyMap;
        for (auto i = 0; i < count; ++i) {
            auto stream = to_string(i);
            PlayerProxy::Ptr player(new PlayerProxy(DEFAULT_VHOST, "live", stream, false, false));
            (*player)[kRtpType] = Rtsp::RTP_TCP;
            player->play(url);
            proxyMap.emplace(stream, player);
        }

        static semaphore sem;
        signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
        sem.wait();
    }
    return 0;
}

