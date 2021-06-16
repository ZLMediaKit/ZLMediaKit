/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "PusherProxy.h"

using namespace toolkit;

namespace mediakit {

PusherProxy::PusherProxy(const string& schema, const string &vhost, const string &app, const string &stream,
            int retry_count, const EventPoller::Ptr &poller)
            : MediaPusher(schema,vhost, app, stream, poller){
    _schema = schema;
    _vhost = vhost;
    _app = app;
    _stream_id = stream;
    _retry_count = retry_count;
    _on_close = [](const SockException &) {};
}

PusherProxy::~PusherProxy() {
    _timer.reset();
}

void PusherProxy::setPushCallbackOnce(const function<void(const SockException &ex)> &cb) {
    _on_publish = cb;
}

void PusherProxy::setOnClose(const function<void(const SockException &ex)> &cb) {
    _on_close = cb;
}

void PusherProxy::publish(const string& dstUrl) {
    std::weak_ptr<PusherProxy> weakSelf = shared_from_this();
    std::shared_ptr<int> piFailedCnt(new int(0));

    setOnPublished([weakSelf, dstUrl, piFailedCnt](const SockException &err) {
       auto strongSelf = weakSelf.lock();
       if (!strongSelf) return;

       if (strongSelf->_on_publish) {
           strongSelf->_on_publish(err);
           strongSelf->_on_publish = nullptr;
       }

        if (!err) {
            // 推流成功
            *piFailedCnt = 0;
            InfoL << "pusher publish " << dstUrl << " success";
        } else if (*piFailedCnt < strongSelf->_retry_count || strongSelf->_retry_count < 0) {
            // 推流失败，延时重试推送
            strongSelf->rePublish(dstUrl, (*piFailedCnt)++);
        } else {
            //达到了最大重试次数，回调关闭
            strongSelf->_on_close(err);
        }
    });

    setOnShutdown([weakSelf, dstUrl, piFailedCnt](const SockException &err) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) return;

        //推流异常中断，延时重试播放
        if (*piFailedCnt < strongSelf->_retry_count || strongSelf->_retry_count < 0) {
            strongSelf->rePublish(dstUrl, (*piFailedCnt)++);
        } else {
            //达到了最大重试次数，回调关闭
            strongSelf->_on_close(err);
        }
    });

    MediaPusher::publish(dstUrl);
    _publish_url = dstUrl;
}

void PusherProxy::rePublish(const string &dstUrl, int iFailedCnt) {
    auto iDelay = MAX(2 * 1000, MIN(iFailedCnt * 3000, 60 * 1000));
    weak_ptr<PusherProxy> weakSelf = shared_from_this();
    _timer = std::make_shared<Timer>(iDelay / 1000.0f, [weakSelf, dstUrl, iFailedCnt]() {
        //推流失败次数越多，则延时越长
        auto strongPusher = weakSelf.lock();
        if (!strongPusher) {
            return false;
        }
        WarnL << "推流重试[" << iFailedCnt << "]:" << dstUrl;
        strongPusher->MediaPusher::publish(dstUrl);
        return false;
    }, getPoller());
}

} /* namespace mediakit */
