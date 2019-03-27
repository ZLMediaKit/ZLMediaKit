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

#ifndef SRC_PUSHER_PUSHERBASE_H_
#define SRC_PUSHER_PUSHERBASE_H_

#include <map>
#include <memory>
#include <string>
#include <functional>
#include "Network/Socket.h"
#include "Util/mini.h"
#include "Common/MediaSource.h"
using namespace toolkit;

namespace mediakit {


class PusherBase : public mINI{
public:
    typedef std::shared_ptr<PusherBase> Ptr;

    static Ptr createPusher(const MediaSource::Ptr &src,
                            const string &strUrl);

    //指定网卡ip
    static const char kNetAdapter[];
    //设置rtp传输类型，可选项有0(tcp，默认)、1(udp)、2(组播)
    //设置方法:player[PusherBase::kRtpType] = 0/1/2;
    static const char kRtpType[];
    //rtsp认证用户名
    static const char kRtspUser[];
    //rtsp认证用用户密码，可以是明文也可以是md5,md5密码生成方式 md5(username:realm:password)
    static const char kRtspPwd[];
    //rtsp认证用用户密码是否为md5类型
    static const char kRtspPwdIsMD5[];
    //播放超时时间，默认10,000 毫秒
    static const char kPlayTimeoutMS[];
    //rtp/rtmp包接收超时时间，默认5000秒
    static const char kMediaTimeoutMS[];
    //rtsp/rtmp心跳时间,默认5000毫秒
    static const char kBeatIntervalMS[];

    typedef std::function<void(const SockException &ex)> Event;

    PusherBase();
    virtual ~PusherBase(){}

    /**
     * 开始推流
     * @param strUrl 视频url，支持rtsp/rtmp
     */
    virtual void publish(const string &strUrl) = 0;

    /**
     * 中断推流
     */
    virtual void teardown() = 0;

    /**
     * 摄像推流结果回调
     * @param onPublished
     */
    virtual void setOnPublished(const Event &cb) = 0;

    /**
     * 设置断开回调
     * @param onShutdown
     */
    virtual void setOnShutdown(const Event &cb) = 0;
};

template<typename Parent,typename Parser>
class PusherImp : public Parent {
public:
    typedef std::shared_ptr<PusherImp> Ptr;
    PusherImp(){}
    virtual ~PusherImp(){}

    /**
     * 开始推流
     * @param strUrl 推流url，支持rtsp/rtmp
     */
    void publish(const string &strUrl) override{
        if (_parser) {
            _parser->publish(strUrl);
        }
    }

    /**
     * 中断推流
     */
    void teardown() override{
        if (_parser) {
            _parser->teardown();
        }
    }

    /**
     * 摄像推流结果回调
     * @param onPublished
     */
    void setOnPublished(const PusherBase::Event &cb) override{
        if (_parser) {
            _parser->setOnPublished(cb);
        }
        _publishCB = cb;
    }

    /**
     * 设置断开回调
     * @param onShutdown
     */
    void setOnShutdown(const PusherBase::Event &cb) override{
        if (_parser) {
            _parser->setOnShutdown(cb);
        }
        _shutdownCB = cb;
    }
protected:
    PusherBase::Event _shutdownCB;
    PusherBase::Event _publishCB;
    std::shared_ptr<Parser> _parser;
};


} /* namespace mediakit */

#endif /* SRC_PUSHER_PUSHERBASE_H_ */
