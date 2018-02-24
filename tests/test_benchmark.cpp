/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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
#include <signal.h>
#include <atomic>
#include <iostream>
#include <list>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Rtsp/UDPServer.h"
#include "Network/sockutil.h"
#include "Poller/EventPoller.h"
#include "Device/PlayerProxy.h"
#include "Thread/WorkThreadPool.h"

using namespace std;
using namespace ZL::DEV;
using namespace ZL::Util;
using namespace ZL::Rtsp;
using namespace ZL::Thread;
using namespace ZL::Network;


int main(int argc, char *argv[]) {
    //设置退出信号处理函数
    signal(SIGINT, [](int) { EventPoller::Instance().shutdown(); });
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    if (argc != 5) {
        FatalL << "\r\n测试方法:./test_benchmark player_count play_interval rtxp_url rtp_type\r\n"
               << "例如你想每隔50毫秒启动共计100个播放器（tcp方式播放rtsp://127.0.0.1/live/0 ）可以输入以下命令:\r\n"
               << "./test_benchmark 100 50 rtsp://127.0.0.1/live/0 0\r\n"
               << endl;
        Logger::Destory();
        return 0;

    }
    {
        list<MediaPlayer::Ptr> playerList;
        auto playerCnt = atoi(argv[1]);//启动的播放器个数
        atomic_int alivePlayerCnt(0);
        //每隔若干毫秒启动一个播放器（如果一次性全部启动，服务器和客户端可能都承受不了）
        AsyncTaskThread::Instance().DoTaskDelay(0, atoi(argv[2]), [&]() {
            MediaPlayer::Ptr player(new MediaPlayer());
            player->setOnPlayResult([&](const SockException &ex) {
                if (!ex) {
                    ++alivePlayerCnt;
                }
            });
            player->setOnShutdown([&](const SockException &ex) {
                --alivePlayerCnt;
            });
            (*player)[RtspPlayer::kRtpType] = atoi(argv[4]);
            player->play(argv[3]);
            playerList.push_back(player);
            return playerCnt--;
        });

        AsyncTaskThread::Instance().DoTaskDelay(0, 1000, [&]() {
            InfoL << "存活播放器个数:" << alivePlayerCnt.load();
            return true;
        });
        EventPoller::Instance().runLoop();
        AsyncTaskThread::Instance().CancelTask(0);
    }

    static onceToken token(nullptr, []() {
        WorkThreadPool::Instance();
        UDPServer::Destory();
        EventPoller::Destory();
        AsyncTaskThread::Destory();
        Logger::Destory();
    });
    return 0;
}

