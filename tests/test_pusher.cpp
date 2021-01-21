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

//推流器，保持强引用
MediaPusher::Ptr pusher;
Timer::Ptr g_timer;

//声明函数
void rePushDelay(const EventPoller::Ptr &poller,const string &schema,const string &vhost,const string &app, const string &stream, const string &url);

//创建推流器并开始推流
void createPusher(const EventPoller::Ptr &poller, const string &schema,const string &vhost,const string &app, const string &stream, const string &url) {
    //创建推流器并绑定一个MediaSource
    pusher.reset(new MediaPusher(schema,vhost, app, stream,poller));
    //可以指定rtsp推流方式，支持tcp和udp方式，默认tcp
//    (*pusher)[Client::kRtpType] = Rtsp::RTP_UDP;
    //设置推流中断处理逻辑
    pusher->setOnShutdown([poller,schema,vhost, app, stream, url](const SockException &ex) {
        WarnL << "Server connection is closed:" << ex.getErrCode() << " " << ex.what();
        //重试
        rePushDelay(poller,schema,vhost,app, stream, url);
    });
    //设置发布结果处理逻辑
    pusher->setOnPublished([poller,schema,vhost, app, stream, url](const SockException &ex) {
        if (ex) {
            WarnL << "Publish fail:" << ex.getErrCode() << " " << ex.what();
            //如果发布失败，就重试
            rePushDelay(poller,schema,vhost,app, stream, url);
        } else {
            InfoL << "Publish success,Please play with player:" << url;
        }
    });
    pusher->publish(url);
}

//推流失败或断开延迟2秒后重试推流
void rePushDelay(const EventPoller::Ptr &poller,const string &schema,const string &vhost,const string &app, const string &stream, const string &url) {
    g_timer = std::make_shared<Timer>(2.0f,[poller,schema,vhost,app, stream, url]() {
        InfoL << "Re-Publishing...";
        //重新推流
        createPusher(poller,schema,vhost,app, stream, url);
        //此任务不重复
        return false;
    }, poller);
}

//这里才是真正执行main函数，你可以把函数名(domain)改成main，然后就可以输入自定义url了
int domain(const string &playUrl, const string &pushUrl) {
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    auto poller = EventPollerPool::Instance().getPoller();

    //拉一个流，生成一个RtmpMediaSource，源的名称是"app/stream"
    //你也可以以其他方式生成RtmpMediaSource，比如说MP4文件（请查看test_rtmpPusherMp4.cpp代码）
    MediaInfo info(pushUrl);
    PlayerProxy::Ptr player(new PlayerProxy(DEFAULT_VHOST, "app", "stream",false,false,-1 , poller));
    //可以指定rtsp拉流方式，支持tcp和udp方式，默认tcp
//    (*player)[Client::kRtpType] = Rtsp::RTP_UDP;
    player->play(playUrl.data());

    //监听RtmpMediaSource注册事件,在PlayerProxy播放成功后触发
    NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaChanged,
                                         [pushUrl,poller](BroadcastMediaChangedArgs) {
                                             //媒体源"app/stream"已经注册，这时方可新建一个RtmpPusher对象并绑定该媒体源
                                             if(bRegist && pushUrl.find(sender.getSchema()) == 0){
                                                 createPusher(poller,sender.getSchema(),sender.getVhost(),sender.getApp(), sender.getId(), pushUrl);
                                             }
                                         });

    //设置退出信号处理函数
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






