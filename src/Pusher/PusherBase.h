/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
    typedef std::function<void(const SockException &ex)> Event;

    static Ptr createPusher(const EventPoller::Ptr &poller,
                            const MediaSource::Ptr &src,
                            const string &strUrl);

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

template<typename Parent,typename Delegate>
class PusherImp : public Parent {
public:
    typedef std::shared_ptr<PusherImp> Ptr;

    template<typename ...ArgsType>
    PusherImp(ArgsType &&...args):Parent(std::forward<ArgsType>(args)...){}

    virtual ~PusherImp(){}

    /**
     * 开始推流
     * @param strUrl 推流url，支持rtsp/rtmp
     */
    void publish(const string &strUrl) override{
        if (_delegate) {
            _delegate->publish(strUrl);
        }
    }

    /**
     * 中断推流
     */
    void teardown() override{
        if (_delegate) {
            _delegate->teardown();
        }
    }

    /**
     * 摄像推流结果回调
     * @param onPublished
     */
    void setOnPublished(const PusherBase::Event &cb) override{
        if (_delegate) {
            _delegate->setOnPublished(cb);
        }
        _publishCB = cb;
    }

    /**
     * 设置断开回调
     * @param onShutdown
     */
    void setOnShutdown(const PusherBase::Event &cb) override{
        if (_delegate) {
            _delegate->setOnShutdown(cb);
        }
        _shutdownCB = cb;
    }
protected:
    PusherBase::Event _shutdownCB;
    PusherBase::Event _publishCB;
    std::shared_ptr<Delegate> _delegate;
};


} /* namespace mediakit */

#endif /* SRC_PUSHER_PUSHERBASE_H_ */
