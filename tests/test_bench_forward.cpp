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
#include <vector>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/CMD.h"
#include "Util/File.h"
#include "Common/config.h"
#include "Common/Parser.h"
#include "Rtsp/Rtsp.h"
#include "Thread/WorkThreadPool.h"
#include "Pusher/MediaPusher.h"
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
                             "inputs",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "/tmp/inputs.txt",/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "拉流地址配置文件，支持rtmp、rtsp, hls，多个地址以 \"换行符\" 分割",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('o',/*该选项简称，如果是\x00则说明无简称*/
                             "outputs",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "/tmp/outputs.txt",/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "推流地址配置文件，支持rtmp、rtsp，多个地址以 \"换行符\" 分割",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('d',/*该选项简称，如果是\x00则说明无简称*/
                             "delay",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "50",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动拉流代理间隔,单位毫秒",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('m',/*该选项简称，如果是\x00则说明无简称*/
                             "merge",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "300",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "合并写毫秒,合并写能提高性能",/*该选项说明文字*/
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


//此程序为zlm的转推性能测试工具，用于测试拉流代理转推性能	
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
    auto in_urls = cmd_main["inputs"];
    auto out_urls = cmd_main["outputs"];
    auto rtp_type = cmd_main["rtp"].as<int>();
    auto delay_ms = cmd_main["delay"].as<int>();
    auto merge_ms = cmd_main["merge"].as<int>();

    //设置日志	
    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", logLevel));
    //启动异步日志线程	
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    //设置线程数	
    EventPollerPool::setPoolSize(threads);
    WorkThreadPool::setPoolSize(threads);

    //设置合并写	
    mINI::Instance()[General::kMergeWriteMS] = merge_ms;


    std::vector<std::string> input_urls;
    std::vector<std::string> output_urls;

    auto parse_urls = [&]() {
        // 获取输入源列表	
        auto inputs = ::split(toolkit::File::loadFile(in_urls.c_str()), "\n");
        for(auto &url : inputs){
            if(url.empty() || url.find("://") == std::string::npos) {
                continue;
            }
            auto input_url = ::trim(url);
            input_urls.emplace_back(input_url);
        }
        // 获取输出源列表	
        auto outputs = ::split(toolkit::File::loadFile(out_urls.c_str()), "\n");
        for(auto &url : outputs){
            if(url.empty() || url.find("://") == std::string::npos){
                continue;
            }
            auto output_url = ::trim(url);
            output_urls.emplace_back(output_url);
        }

        if(input_urls.empty() || input_urls.size() != output_urls.size()){
            return -1;
        }

        for(size_t i = 0; i < input_urls.size(); i++){
            InfoL << "拉流地址: " << input_urls[i] << ",推流地址：" << output_urls[i];
        }
        return 0;
    };

    if (0 != parse_urls()){
        cout << "请检查inputs和outputs文件是否正确！" << endl;
        return -1;
    }

    //推流器map	
    recursive_mutex mtx;
    unordered_map<int, PlayerProxy::Ptr> proxy_map;
    unordered_map<int, MediaPusher::Ptr> pusher_map;

    auto add_pusher = [&](const MediaSource::Ptr &src, const string &url, int index) {
        auto pusher = std::make_shared<MediaPusher>(src);
        pusher->setOnCreateSocket([](const EventPoller::Ptr &poller) {
            //socket关闭互斥锁，提高性能	
            return std::make_shared<Socket>(poller, false);
        });
        //设置推流失败监听	
        pusher->setOnPublished([&mtx, &pusher_map, index](const SockException &ex) {
            if (ex) {
                //推流失败，移除之	
                lock_guard<recursive_mutex> lck(mtx);
                pusher_map.erase(index);
            }
        });
        //设置推流中途断开监听	
        pusher->setOnShutdown([&mtx, &pusher_map, index](const SockException &ex) {
            //推流中途失败，移除之	
            lock_guard<recursive_mutex> lck(mtx);
            pusher_map.erase(index);
        });
        //设置rtsp推流方式(在rtsp推流时有效)	
        (*pusher)[Client::kRtpType] = rtp_type;
        pusher->publish(url);
        //保持对象不销毁	
        lock_guard<recursive_mutex> lck(mtx);
        pusher_map.emplace(index, std::move(pusher));
        //休眠后再启动下一个推流，防止短时间海量链接	
        if (delay_ms > 0) {
            usleep(1000 * delay_ms);
        }
    };

    // 添加转推任务	
    for(size_t i = 0; i < input_urls.size(); i++) {
        //休眠一秒打印	
        sleep(1);
        auto schema = findSubString(output_urls[i].data(), nullptr, "://");
        if (schema != RTSP_SCHEMA && schema != RTMP_SCHEMA) {
            cout << "推流协议只支持rtsp或rtmp！" << endl;
            return -1;
        }
        ProtocolOption option;
        option.enable_ts = false;
        option.enable_fmp4 = false;
        option.enable_hls = false;
        option.enable_mp4 = false;
        option.modify_stamp = (int)ProtocolOption::kModifyStampRelative;
        //添加拉流代理	
        auto proxy = std::make_shared<PlayerProxy>(DEFAULT_VHOST, "app", std::to_string(i), option, -1, nullptr, 1);
        //开始拉流代理	
        proxy->play(input_urls[i]);
        proxy_map.emplace(i, std::move(proxy));
    }

    // 设置退出信号	
    static bool exit_flag = false;
    signal(SIGINT, [](int) { exit_flag = true; });
    while (!exit_flag) {
        //休眠一秒打印	
        sleep(1);

        size_t alive_pusher = 0;
        {
            lock_guard<recursive_mutex> lck(mtx);
            alive_pusher = pusher_map.size();
        }
        InfoL << "在线转推器个数:" << alive_pusher;

        auto find_pusher = [&](int index){
            lock_guard<recursive_mutex> lck(mtx);
            auto it = pusher_map.find(index);
            if (it == pusher_map.end()){
                return false;
            }
            return true;
        };
        for(size_t i = 0; i < input_urls.size(); i++) {
            if (!find_pusher(i)){
                auto input_url = input_urls[i];
                auto src = MediaSource::find(RTMP_SCHEMA, DEFAULT_VHOST, "app", std::to_string(i), false);
                if (src != nullptr){
                    add_pusher(src,output_urls[i],i);
                }
            }
        }
    }

    return 0;
}