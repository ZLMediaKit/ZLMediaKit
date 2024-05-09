/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_DEVICE_PUSHERPROXY_H
#define SRC_DEVICE_PUSHERPROXY_H

#include "Pusher/MediaPusher.h"
#include "Util/TimeTicker.h"

namespace mediakit {

class PusherProxy
    : public MediaPusher
    , public std::enable_shared_from_this<PusherProxy> {
public:
    using Ptr = std::shared_ptr<PusherProxy>;

    // 如果retry_count<0,则一直重试播放；否则重试retry_count次数
    // 默认一直重试，创建此对象时候，需要外部保证MediaSource存在
    PusherProxy(const MediaSource::Ptr &src, int retry_count = -1, const toolkit::EventPoller::Ptr &poller = nullptr);
    ~PusherProxy() override;

    /**
     * 设置push结果回调，只触发一次；在publish执行之前有效
     * @param cb 回调对象
     */
    void setPushCallbackOnce(const std::function<void(const toolkit::SockException &ex)> &cb);

    /**
     * 设置主动关闭回调
     * @param cb 回调对象
     */
    void setOnClose(const std::function<void(const toolkit::SockException &ex)> &cb);

    /**
     * 开始拉流播放
     * @param dstUrl 目标推流地址
     */
    void publish(const std::string &dstUrl) override;

    int getStatus();
    uint64_t getLiveSecs();
    uint64_t getRePublishCount();

private:
    // 重推逻辑函数
    void rePublish(const std::string &dstUrl, int iFailedCnt);

private:
    int _retry_count;
    toolkit::Timer::Ptr _timer;
    toolkit::Ticker _live_ticker;
    // 0 表示正常 1 表示正在尝试推流
    std::atomic<int> _live_status;
    std::atomic<uint64_t> _live_secs;
    std::atomic<uint64_t> _republish_count;
    std::weak_ptr<MediaSource> _weak_src;
    std::function<void(const toolkit::SockException &ex)> _on_close;
    std::function<void(const toolkit::SockException &ex)> _on_publish;
};

} /* namespace mediakit */

#endif // SRC_DEVICE_PUSHERPROXY_H
