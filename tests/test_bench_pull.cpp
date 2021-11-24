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
#include <atomic>
#include <iostream>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/CMD.h"
#include "Rtsp/UDPServer.h"
#include "Thread/WorkThreadPool.h"
#include "Player/PlayerProxy.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

class CMD_main : public CMD {
public:
    CMD_main() {
        _parser.reset(new OptionParser(nullptr));

        (*_parser) << Option('l',/*该选项简称，如果是\x00则说明无简称*/
                             "level",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(LTrace).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "日志等级,LTrace~LError(0~4)",/*该选项说明文字*/
                             nullptr);


        (*_parser) << Option('t',/*该选项简称，如果是\x00则说明无简称*/
                             "threads",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(thread::hardware_concurrency()).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动事件触发线程数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('i',/*该选项简称，如果是\x00则说明无简称*/
                             "in",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             nullptr,/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "拉流url,支持rtsp/rtmp/hls",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('c',/*该选项简称，如果是\x00则说明无简称*/
                             "count",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "1000",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "拉流播放器个数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('d',/*该选项简称，如果是\x00则说明无简称*/
                             "delay",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "10",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动拉流客户端间隔,单位毫秒",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('T',/*该选项简称，如果是\x00则说明无简称*/
                             "rtp",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string((int) (Rtsp::RTP_TCP)).data(),/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "rtsp拉流方式,支持tcp/udp/multicast:0/1/2",/*该选项说明文字*/
                             nullptr);
    }

    ~CMD_main() override {}

    const char *description() const override {
        return "主程序命令参数";
    }
};

//此程序用于拉流播放性能测试
int main(int argc, char *argv[]) {
    CMD_main cmd_main;
    try {
        cmd_main.operator()(argc, argv);
    } catch (ExitException &) {
        return 0;
    } catch (std::exception &ex) {
        cout << ex.what() << endl;
        return -1;
    }

    int threads = cmd_main["threads"];
    LogLevel logLevel = (LogLevel) cmd_main["level"].as<int>();
    logLevel = MIN(MAX(logLevel, LTrace), LError);
    auto in_url = cmd_main["in"];
    auto rtp_type = cmd_main["rtp"].as<int>();
    auto delay_ms = cmd_main["delay"].as<int>();
    auto player_count = cmd_main["count"].as<int>();

    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", logLevel));
    //启动异步日志线程
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    //设置线程数
    EventPollerPool::setPoolSize(threads);
    WorkThreadPool::setPoolSize(threads);

    //播放器map
    recursive_mutex mtx;
    unordered_map<void *, MediaPlayer::Ptr> player_map;

    auto add_player = [&]() {
        auto player = std::make_shared<MediaPlayer>();
        auto tag = player.get();
        player->setOnCreateSocket([](const EventPoller::Ptr &poller) {
            //socket关闭互斥锁，提高性能
            return std::make_shared<Socket>(poller, false);
        });
        //设置播放失败监听
        player->setOnPlayResult([&mtx, &player_map, tag](const SockException &ex) {
            if (ex) {
                //播放失败，移除之
                lock_guard<recursive_mutex> lck(mtx);
                player_map.erase(tag);
            }
        });
        //设置播放中途断开监听
        player->setOnShutdown([&mtx, &player_map, tag](const SockException &ex) {
            //播放中途失败，移除之
            lock_guard<recursive_mutex> lck(mtx);
            player_map.erase(tag);
        });
        //设置为性能测试模式
        (*player)[kBenchmarkMode] = true;
        //设置rtsp拉流方式(在rtsp拉流时有效)
        (*player)[kRtpType] = rtp_type;
        //提高压测性能与正确性
        (*player)[Client::kWaitTrackReady] = false;
        //发起播放请求
        player->play(in_url);

        //保持对象不销毁
        lock_guard<recursive_mutex> lck(mtx);
        player_map.emplace(tag, std::move(player));

        //休眠后再启动下一个播放，防止短时间海量链接
        if (delay_ms > 0) {
            usleep(1000 * delay_ms);
        }
    };

    //添加这么多播放器
    for (auto i = 0; i < player_count; ++i) {
        add_player();
    }

    // 设置退出信号
    static bool exit_flag = false;
    signal(SIGINT, [](int) { exit_flag = true; });
    while (!exit_flag) {
        //休眠一秒打印
        sleep(1);

        size_t alive_player = 0;
        {
            lock_guard<recursive_mutex> lck(mtx);
            alive_player = player_map.size();
        }
        InfoL << "在线播放器个数:" << alive_player;
        size_t re_try = player_count - alive_player;
        while (!exit_flag && re_try--) {
            //有些播放器播放失败了，那么我们重试添加
            add_player();
        }
    }

    return 0;
}

