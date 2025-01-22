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
#include "Util/NoticeCenter.h"
#include "Poller/EventPoller.h"
#include "Player/PlayerProxy.h"
#include "Rtmp/RtmpPusher.h"
#include "Common/config.h"
#include "Pusher/MediaPusher.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

// 推流器，保持强引用  [AUTO-TRANSLATED:b2287a61]
// Streamer, keep a strong reference
MediaPusher::Ptr pusher;
Timer::Ptr g_timer;

// 声明函数  [AUTO-TRANSLATED:f3911a32]
// Declare function
void rePushDelay(const EventPoller::Ptr &poller,const string &schema,const string &vhost,const string &app, const string &stream, const string &url);

// 创建推流器并开始推流  [AUTO-TRANSLATED:583100b5]
// Create a streamer and start streaming
void createPusher(const EventPoller::Ptr &poller, const string &schema,const string &vhost,const string &app, const string &stream, const string &url) {
    // 创建推流器并绑定一个MediaSource  [AUTO-TRANSLATED:b0721d46]
    // Create a streamer and bind a MediaSource
    pusher.reset(new MediaPusher(schema,vhost, app, stream,poller));
    // 可以指定rtsp推流方式，支持tcp和udp方式，默认tcp  [AUTO-TRANSLATED:bb0be012]
    // You can specify the RTSP streaming method, supporting both TCP and UDP methods, defaulting to TCP
//    (*pusher)[Client::kRtpType] = Rtsp::RTP_UDP;
    // 设置推流中断处理逻辑  [AUTO-TRANSLATED:aa6c0405]
    // Set the streaming interruption handling logic
    pusher->setOnShutdown([poller,schema,vhost, app, stream, url](const SockException &ex) {
        WarnL << "Server connection is closed:" << ex.getErrCode() << " " << ex.what();
        // 重试  [AUTO-TRANSLATED:d96b4814]
        // Retry
        rePushDelay(poller,schema,vhost,app, stream, url);
    });
    // 设置发布结果处理逻辑  [AUTO-TRANSLATED:0ee98a13]
    // Set the publishing result handling logic
    pusher->setOnPublished([poller,schema,vhost, app, stream, url](const SockException &ex) {
        if (ex) {
            WarnL << "Publish fail:" << ex.getErrCode() << " " << ex.what();
            // 如果发布失败，就重试  [AUTO-TRANSLATED:67aff5bd]
            // If publishing fails, retry
            rePushDelay(poller,schema,vhost,app, stream, url);
        } else {
            InfoL << "Publish success,Please play with player:" << url;
        }
    });
    pusher->publish(url);
}

// 推流失败或断开延迟2秒后重试推流  [AUTO-TRANSLATED:bc496634]
// If streaming fails or is disconnected, retry streaming after a 2-second delay
void rePushDelay(const EventPoller::Ptr &poller,const string &schema,const string &vhost,const string &app, const string &stream, const string &url) {
    g_timer = std::make_shared<Timer>(2.0f,[poller,schema,vhost,app, stream, url]() {
        InfoL << "Re-Publishing...";
        // 重新推流  [AUTO-TRANSLATED:edb1e699]
        // Re-stream
        createPusher(poller,schema,vhost,app, stream, url);
        // 此任务不重复  [AUTO-TRANSLATED:84deec34]
        // This task is not repeated
        return false;
    }, poller);
}

// 这里才是真正执行main函数，你可以把函数名(domain)改成main，然后就可以输入自定义url了  [AUTO-TRANSLATED:a441f1a2]
// This is where the main function is actually executed, you can change the function name (domain) to main, and then you can enter a custom URL
int domain(const string &playUrl, const string &pushUrl) {
    // 设置日志  [AUTO-TRANSLATED:50372045]
    // Set the log
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    auto poller = EventPollerPool::Instance().getPoller();

    // 拉一个流，生成一个RtmpMediaSource，源的名称是"app/stream"  [AUTO-TRANSLATED:38b8ab6f]
    // Pull a stream and generate an RtmpMediaSource, the source name is "app/stream"
    // 你也可以以其他方式生成RtmpMediaSource，比如说MP4文件（请查看test_rtmpPusherMp4.cpp代码）  [AUTO-TRANSLATED:c94c5914]
    // You can also generate RtmpMediaSource in other ways, such as an MP4 file (please refer to the test_rtmpPusherMp4.cpp code)
    MediaInfo info(pushUrl);

    ProtocolOption option;
    option.enable_hls = false;
    option.enable_mp4 = false;
    auto tuple = MediaTuple{DEFAULT_VHOST, "app", "stream", ""};
    PlayerProxy::Ptr player(new PlayerProxy(tuple, option, -1, poller));
    // 可以指定rtsp拉流方式，支持tcp和udp方式，默认tcp  [AUTO-TRANSLATED:5169e341]
    // You can specify the RTSP streaming method, supporting both TCP and UDP methods, defaulting to TCP
//    (*player)[Client::kRtpType] = Rtsp::RTP_UDP;
    player->play(playUrl.data());

    // 监听RtmpMediaSource注册事件,在PlayerProxy播放成功后触发  [AUTO-TRANSLATED:da417dbe]
    // Listen for RtmpMediaSource registration events, triggered after PlayerProxy playback is successful
    NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaChanged,
                                         [pushUrl,poller](BroadcastMediaChangedArgs) {
                                             // 媒体源"app/stream"已经注册，这时方可新建一个RtmpPusher对象并绑定该媒体源  [AUTO-TRANSLATED:3670cbfd]
                                             // The media source "app/stream" has been registered, at this point you can create a new RtmpPusher object and bind it to the media source
                                             if (bRegist && pushUrl.find(sender.getSchema()) == 0) {
                                                 auto tuple = sender.getMediaTuple();
                                                 createPusher(poller, sender.getSchema(), tuple.vhost, tuple.app, tuple.stream, pushUrl);
                                             }
                                         });

    // 设置退出信号处理函数  [AUTO-TRANSLATED:4f047770]
    // Set the exit signal processing function
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    pusher.reset();
    g_timer.reset();
    return 0;
}


int main(int argc, char *argv[]) {
    return domain("rtmp://live.hkstv.hk.lxdns.com/live/hks1", "rtsp://127.0.0.1/live/rtsp_push");
}






