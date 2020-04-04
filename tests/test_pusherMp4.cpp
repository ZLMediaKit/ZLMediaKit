/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
#include "Record/MP4Reader.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

//推流器，保持强引用
MediaPusher::Ptr pusher;
Timer::Ptr g_timer;


//声明函数
//推流失败或断开延迟2秒后重试推流
void rePushDelay(const EventPoller::Ptr &poller,
                 const string &schema,
                 const string &vhost,
                 const string &app,
                 const string &stream,
                 const string &filePath,
                 const string &url) ;

//创建推流器并开始推流
void createPusher(const EventPoller::Ptr &poller,
                  const string &schema,
                  const string &vhost,
                  const string &app,
                  const string &stream,
                  const string &filePath,
                  const string &url) {
    //不限制APP名，并且指定文件绝对路径
    auto src = MediaSource::createFromMP4(schema,vhost,app,stream,filePath, false);
    if(!src){
        //文件不存在
        WarnL << "MP4文件不存在:" << filePath;
        return;
    }

    //创建推流器并绑定一个MediaSource
    pusher.reset(new MediaPusher(src,poller));
    //可以指定rtsp推流方式，支持tcp和udp方式，默认tcp
//    (*pusher)[Client::kRtpType] = Rtsp::RTP_UDP;

    //设置推流中断处理逻辑
    pusher->setOnShutdown([poller,schema,vhost,app,stream,filePath, url](const SockException &ex) {
        WarnL << "Server connection is closed:" << ex.getErrCode() << " " << ex.what();
        //重新推流
        rePushDelay(poller,schema,vhost,app, stream,filePath, url);
    });
    //设置发布结果处理逻辑
    pusher->setOnPublished([poller,schema,vhost,app,stream,filePath, url](const SockException &ex) {
        if (ex) {
            WarnL << "Publish fail:" << ex.getErrCode() << " " << ex.what();
            //如果发布失败，就重试
            rePushDelay(poller,schema,vhost,app, stream, filePath ,url);
        }else {
            InfoL << "Publish success,Please play with player:" << url;
        }
    });
    pusher->publish(url);
}

//推流失败或断开延迟2秒后重试推流
void rePushDelay(const EventPoller::Ptr &poller,
                 const string &schema,
                 const string &vhost,
                 const string &app,
                 const string &stream,
                 const string &filePath,
                 const string &url) {
    g_timer = std::make_shared<Timer>(2,[poller,schema,vhost,app, stream, filePath,url]() {
        InfoL << "Re-Publishing...";
        //重新推流
        createPusher(poller,schema,vhost,app, stream, filePath,url);
        //此任务不重复
        return false;
    }, poller);
}

//这里才是真正执行main函数，你可以把函数名(domain)改成main，然后就可以输入自定义url了
int domain(const string & filePath,const string & pushUrl){
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    auto poller = EventPollerPool::Instance().getPoller();
    //vhost/app/stream可以随便自己填，现在不限制app应用名了
    createPusher(poller,FindField(pushUrl.data(), nullptr,"://"),DEFAULT_VHOST,"live","stream",filePath,pushUrl);

    //设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    pusher.reset();
    g_timer.reset();
    return 0;
}



int main(int argc,char *argv[]){
    //可以使用test_server生成的mp4文件
    //文件使用绝对路径，推流url支持rtsp和rtmp
    return domain("/Users/xzl/git/ZLMediaKit/release/mac/Debug/www/record/live/rtsp_test1/2020-04-03/15-32-24.mp4","rtsp://127.0.0.1/live/rtsp_push");
}





