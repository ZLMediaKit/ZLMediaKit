/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#include "StreamProxyPusher.h"
#include "Common/config.h"
#include "Pusher/MediaPusher.h"
#include "Poller/EventPoller.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

StreamProxyPusher::StreamProxyPusher() {
    _poller = EventPollerPool::Instance().getPoller();
}

StreamProxyPusher::~StreamProxyPusher() {
}

//创建推流器并开始推流
void StreamProxyPusher::createPusher(const string schema, const string vhost, const string app, const string stream,
                                     const string url) {
    //创建推流器并绑定一个MediaSource
    _pusher.reset(new MediaPusher(schema, vhost, app, stream, _poller));
    //可以指定rtsp推流方式，支持tcp和udp方式，默认tcp
    //    (*pusher)[Client::kRtpType] = Rtsp::RTP_UDP;
    //设置推流中断处理逻辑
    std::weak_ptr<StreamProxyPusher> weakSelf = shared_from_this();
    _pusher->setOnShutdown([weakSelf](const SockException &ex) {
        WarnL << "Server connection is closed:" << ex.getErrCode() << " " << ex.what();
        //重试
        auto strongSelf = weakSelf.lock();
        if (strongSelf) {
            strongSelf->_timer = std::make_shared<Timer>(2, [weakSelf]() {
                InfoL << "Re-Publishing...";
                //重新推流
                auto strongSelf = weakSelf.lock();
                if (strongSelf)
                    strongSelf->createPusher(strongSelf->_schema, strongSelf->_vhost, strongSelf->_app,
                                             strongSelf->_stream, strongSelf->_dstUrl);
                //此任务不重复
                return false;
            }, strongSelf->_poller);
        }
    });
    //设置发布结果处理逻辑
    _pusher->setOnPublished([weakSelf](const SockException &ex) {
        if (ex) {
            WarnL << "Publish fail:" << ex.getErrCode() << " " << ex.what();
            //如果发布失败，就重试
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                strongSelf->_timer = std::make_shared<Timer>(2, [weakSelf]() {
                    InfoL << "Re-Publishing...";
                    //重新推流
                    auto strongSelf = weakSelf.lock();
                    if (strongSelf) {
                        strongSelf->createPusher(strongSelf->_schema, strongSelf->_vhost, strongSelf->_app,
                                                 strongSelf->_stream, strongSelf->_dstUrl);
                    }
                    //此任务不重复
                    return false;
                }, strongSelf->_poller);
            }
        } else {
            auto strongSelf = weakSelf.lock();
            if (strongSelf)
                InfoL << "Publish success,Please play with player:" << strongSelf->_dstUrl;
        }
    });
    _pusher->publish(url);
}

void StreamProxyPusher::setOnClose(const function<void()> &cb) {
    _onClose = cb;
}

void StreamProxyPusher::play(const string &vhost, const string &app, const string &stream, const string &url,
                             bool enable_rtsp,
                             bool enable_rtmp, bool enable_hls, bool enable_mp4,
                             int rtp_type, const function<void(const SockException &ex)> &cb,
                             const string &dst_url) {
    _schema = FindField(dst_url.data(), nullptr, "://");;
    _vhost = vhost;
    _app = app;
    _stream = stream;
    _url = url;
    _dstUrl = dst_url;
    _player.reset(new PlayerProxy(vhost, app, stream, enable_rtsp, enable_rtmp, enable_hls, enable_mp4));

    //指定RTP over TCP(播放rtsp时有效)
    (*_player)[kRtpType] = rtp_type;
    //开始播放，如果播放失败或者播放中止，将会自动重试若干次，默认一直重试
    _player->setPlayCallbackOnce(cb);

    //被主动关闭拉流
    _player->setOnClose(_onClose);
    _player->play(url);
    if (!dst_url.empty()) {
        std::weak_ptr<StreamProxyPusher> weakSelf = shared_from_this();
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaChanged,
                                             [weakSelf, stream, dst_url](BroadcastMediaChangedArgs) {
                                                 //媒体源"app/stream"已经注册，这时方可新建一个RtmpPusher对象并绑定该媒体源
                                                 if (bRegist && dst_url.find(sender.getSchema()) == 0 &&
                                                     stream.find(sender.getId()) == 0) {
                                                     auto strongSelf = weakSelf.lock();
                                                     if (strongSelf)
                                                         strongSelf->createPusher(sender.getSchema(), sender.getVhost(),
                                                                                  sender.getApp(),
                                                                                  sender.getId(), dst_url);
                                                 }
                                             });
    }
}