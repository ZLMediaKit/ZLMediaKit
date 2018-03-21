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
#include <iostream>
#include "Util/logger.h"
#include "Util/NoticeCenter.h"
#include "Poller/EventPoller.h"
#include "Device/PlayerProxy.h"
#include "Rtmp/RtmpPusher.h"
#include "Common/config.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Rtmp;
using namespace ZL::Thread;
using namespace ZL::Network;
using namespace ZL::DEV;

//推流器，保持强引用
RtmpPusher::Ptr pusher;

//声明函数
void rePushDelay(const string &app, const string &stream, const string &url);

//创建推流器并开始推流
void createPusher(const string &app, const string &stream, const string &url) {
    //创建推流器并绑定一个RtmpMediaSource
    pusher.reset(new RtmpPusher(DEFAULT_VHOST, app.data(), stream.data()));
    //设置推流中断处理逻辑
    pusher->setOnShutdown([app, stream, url](const SockException &ex) {
        WarnL << "Server connection is closed:" << ex.getErrCode() << " " << ex.what();
        //重试
        rePushDelay(app, stream, url);
    });
    //设置发布结果处理逻辑
    pusher->setOnPublished([app, stream, url](const SockException &ex) {
        if (ex) {
            WarnL << "Publish fail:" << ex.getErrCode() << " " << ex.what();
            //如果发布失败，就重试
            rePushDelay(app, stream, url);
        } else {
            InfoL << "Publish success,Please play with player:" << url;
        }
    });
    pusher->publish(url.data());
}

//推流失败或断开延迟2秒后重试推流
void rePushDelay(const string &app, const string &stream, const string &url) {
    //上次延时两秒的任务可能还没执行，所以我们要先取消上次任务
    AsyncTaskThread::Instance().CancelTask(0);
    //2秒后执行重新推流的任务
    AsyncTaskThread::Instance().DoTaskDelay(0, 2000, [app, stream, url]() {
        InfoL << "Re-Publishing...";
        //重新推流
        createPusher(app, stream, url);
        //此任务不重复
        return false;
    });
}

//这里才是真正执行main函数，你可以把函数名(domain)改成main，然后就可以输入自定义url了
int domain(const string &playUrl, const string &pushUrl) {
    //设置退出信号处理函数
    signal(SIGINT, [](int) { EventPoller::Instance().shutdown(); });
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    {
        //拉一个流，生成一个RtmpMediaSource，源的名称是"app/stream"
        //你也可以以其他方式生成RtmpMediaSource，比如说MP4文件（请查看test_rtmpPusherMp4.cpp代码）
        PlayerProxy::Ptr player(new PlayerProxy(DEFAULT_VHOST, "app", "stream"));
        player->play(playUrl.data());

        //监听RtmpMediaSource注册事件,在PlayerProxy播放成功后触发
        NoticeCenter::Instance().addListener(nullptr, Config::Broadcast::kBroadcastMediaChanged,
                                             [pushUrl](BroadcastMediaChangedArgs) {
                                                 //媒体源"app/stream"已经注册，这时方可新建一个RtmpPusher对象并绑定该媒体源
                                                 if(bRegist && schema == RTMP_SCHEMA){
                                                     createPusher(app, stream, pushUrl);
                                                 }
                                             });

        //事件轮询
        EventPoller::Instance().runLoop();
    }
    //删除事件监听
    NoticeCenter::Instance().delListener(nullptr);

    //清理程序
    EventPoller::Destory();
    AsyncTaskThread::Destory();
    Logger::Destory();
    return 0;
}


int main(int argc, char *argv[]) {
    return domain("rtmp://live.hkstv.hk.lxdns.com/live/hks", "rtmp://jizan.iok.la/live/stream");
}






