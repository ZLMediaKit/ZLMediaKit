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
using namespace std;

namespace mediakit {

PusherProxy::PusherProxy(const MediaSource::Ptr &src, int retry_count, const EventPoller::Ptr &poller)
            : MediaPusher(src, poller){
    _retry_count = retry_count;
    _on_close = [](const SockException &) {};
    _weak_src = src;
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

void PusherProxy::publish(const string &dst_url) {
    std::weak_ptr<PusherProxy> weak_self = shared_from_this();
    std::shared_ptr<int> failed_cnt(new int(0));

    setOnPublished([weak_self, dst_url, failed_cnt](const SockException &err) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }

        if (strong_self->_on_publish) {
            strong_self->_on_publish(err);
            strong_self->_on_publish = nullptr;
        }

        auto src = strong_self->_weak_src.lock();
        if (!err) {
            // 推流成功
            *failed_cnt = 0;
            InfoL << "Publish " << dst_url << " success";
        } else if (src && (*failed_cnt < strong_self->_retry_count || strong_self->_retry_count < 0)) {
            // 推流失败，延时重试推送
            strong_self->rePublish(dst_url, (*failed_cnt)++);
        } else {
            //如果媒体源已经注销, 或达到了最大重试次数，回调关闭
            strong_self->_on_close(err);
        }
    });

    setOnShutdown([weak_self, dst_url, failed_cnt](const SockException &err) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }

        auto src = strong_self->_weak_src.lock();
        //推流异常中断，延时重试播放
        if (src && (*failed_cnt < strong_self->_retry_count || strong_self->_retry_count < 0)) {
            strong_self->rePublish(dst_url, (*failed_cnt)++);
        } else {
            //如果媒体源已经注销, 或达到了最大重试次数，回调关闭
            strong_self->_on_close(err);
        }
    });

    MediaPusher::publish(dst_url);
}

void PusherProxy::rePublish(const string &dst_url, int failed_cnt) {
    auto delay = MAX(2 * 1000, MIN(failed_cnt * 3000, 60 * 1000));
    weak_ptr<PusherProxy> weak_self = shared_from_this();
    _timer = std::make_shared<Timer>(delay / 1000.0f, [weak_self, dst_url, failed_cnt]() {
        //推流失败次数越多，则延时越长
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        WarnL << "推流重试[" << failed_cnt << "]:" << dst_url;
        strong_self->MediaPusher::publish(dst_url);
        return false;
    }, getPoller());
}

} /* namespace mediakit */
