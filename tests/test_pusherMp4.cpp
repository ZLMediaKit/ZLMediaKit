/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Common/config.h"
#include "Common/Parser.h"
#include "Poller/EventPoller.h"
#include "Pusher/MediaPusher.h"
#include "Record/MP4Reader.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

// 这里才是真正执行main函数，你可以把函数名(domain)改成main，然后就可以输入自定义url了  [AUTO-TRANSLATED:8438b572]
// This is where the main function is actually executed, you can change the function name (domain) to main, and then you can enter a custom URL
int domain(const string &file, const string &url) {
    // 设置日志  [AUTO-TRANSLATED:50ba02ba]
    // Set log
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    // 关闭所有转协议  [AUTO-TRANSLATED:2a58bc8f]
    // Close all protocol conversions
    mINI::Instance()[Protocol::kEnableMP4] = 0;
    mINI::Instance()[Protocol::kEnableFMP4] = 0;
    mINI::Instance()[Protocol::kEnableHls] = 0;
    mINI::Instance()[Protocol::kEnableHlsFmp4] = 0;
    mINI::Instance()[Protocol::kEnableTS] = 0;
    mINI::Instance()[Protocol::kEnableRtsp] = 0;
    mINI::Instance()[Protocol::kEnableRtmp] = 0;

    // 根据url获取媒体协议类型，注意大小写  [AUTO-TRANSLATED:3cd6622a]
    // Get the media protocol type based on the URL, note the case
    auto schema = strToLower(findSubString(url.data(), nullptr, "://").substr(0, 4));

    // 只开启推流协议对应的转协议  [AUTO-TRANSLATED:1c4975ae]
    // Only enable the protocol conversion corresponding to the push protocol
    mINI::Instance()["protocol.enable_" + schema] = 1;

    // 从mp4文件加载生成MediaSource对象  [AUTO-TRANSLATED:5e5b04ca]
    // Load the MediaSource object from the mp4 file
    auto tuple = MediaTuple {DEFAULT_VHOST, "live", "stream", ""};
    auto reader = std::make_shared<MP4Reader>(tuple, file);
    // 开始加载mp4，ref_self设置为false，这样reader对象设置为nullptr就能注销了，file_repeat可以设置为空，这样文件读完了就停止推流了  [AUTO-TRANSLATED:88a5c8ae]
    // Start loading the mp4, set ref_self to false, so that the reader object can be set to nullptr to cancel, file_repeat can be set to empty, so that the file is read and the push stream is stopped
    reader->startReadMP4(100, false, true);
    auto src = MediaSource::find(schema, DEFAULT_VHOST, "live", "stream", false);

    if (!src) {
        // 文件不存在  [AUTO-TRANSLATED:f97c8ce8]
        // File does not exist
        WarnL << "File not existed: " << file;
        return -1;
    }

    // 选择一个poller线程绑定  [AUTO-TRANSLATED:82d4a318]
    // Select a poller thread to bind
    auto poller = EventPollerPool::Instance().getPoller();
    // 创建推流器并绑定一个MediaSource  [AUTO-TRANSLATED:c1ab71ca]
    // Create a pusher and bind a MediaSource
    auto pusher = std::make_shared<MediaPusher>(src, poller);

    std::weak_ptr<MediaSource> weak_src = src;
    // src用完了，可以直接置空，防止main函数持有它(MP4Reader持有它即可)  [AUTO-TRANSLATED:52e6393a]
    // src is used up, can be set to empty directly, to prevent the main function from holding it (MP4Reader holds it)
    src = nullptr;

    // 可以指定rtsp推流方式，支持tcp和udp方式，默认tcp  [AUTO-TRANSLATED:235480a7]
    // You can specify the rtsp push method, support tcp and udp methods, default tcp
    //(*pusher)[Client::kRtpType] = Rtsp::RTP_UDP;

    // 设置推流中断处理逻辑  [AUTO-TRANSLATED:76774d30]
    // Set the interrupt handling logic during pushing
    std::weak_ptr<MediaPusher> weak_pusher = pusher;
    pusher->setOnShutdown([poller, url, weak_pusher, weak_src](const SockException &ex) {
        if (!weak_src.lock()) {
            // 媒体注销导致的推流中断，不在重试推流  [AUTO-TRANSLATED:625b3e1a]
            // Media cancellation causes push interruption, no retry push
            WarnL << "MediaSource released:" << ex << ", publish stopped";
            return;
        }
        WarnL << "Server connection is closed:" << ex << ", republish after 2 seconds";
        // 重新推流, 2秒后重试  [AUTO-TRANSLATED:f8a261a3]
        // Repush, retry after 2 seconds
        poller->doDelayTask(2 * 1000, [weak_pusher, url]() {
            if (auto strong_push = weak_pusher.lock()) {
                strong_push->publish(url);
            }
            return 0;
        });
    });

    // 设置发布结果处理逻辑  [AUTO-TRANSLATED:ce9de055]
    // Set the publish result handling logic
    pusher->setOnPublished([poller, weak_pusher, url](const SockException &ex) {
        if (!ex) {
            InfoL << "Publish success, please play with player:" << url;
            return;
        }

        WarnL << "Publish fail:" << ex << ", republish after 2 seconds";
        // 如果发布失败，就重试  [AUTO-TRANSLATED:b37fd4aa]
        // If the publish fails, retry
        poller->doDelayTask(2 * 1000, [weak_pusher, url]() {
            if (auto strong_push = weak_pusher.lock()) {
                strong_push->publish(url);
            }
            return 0;
        });
    });
    pusher->publish(url);

    // sleep(5);
    // reader 置空可以终止推流相关资源  [AUTO-TRANSLATED:02442070]
    // reader set to empty can terminate the push-related resources
    // reader = nullptr;

    // 设置退出信号处理函数  [AUTO-TRANSLATED:69e8f733]
    // Set the exit signal processing function
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); }); // 设置退出信号
    sem.wait();
    return 0;
}

int main(int argc, char *argv[]) {
    // 可以使用test_server生成的mp4文件  [AUTO-TRANSLATED:a9552282]
    // You can use the mp4 file generated by test_server
    // 文件使用绝对路径，推流url支持rtsp和rtmp  [AUTO-TRANSLATED:efc8c3b5]
    // File uses absolute path, push URL supports rtsp and rtmp
    // return domain("/Users/xiongziliang/Downloads/mp4/Quantum.mp4", "rtsp://127.0.0.1/live/rtsp_push");
    return domain(argv[1], argv[2]);
}
