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

    // 如果retry_count<0,则一直重试播放；否则重试retry_count次数  [AUTO-TRANSLATED:5582d53c]
    // If retry_count < 0, then retry playback indefinitely; otherwise, retry retry_count times
    // 默认一直重试，创建此对象时候，需要外部保证MediaSource存在  [AUTO-TRANSLATED:c6159497]
    // Default is to retry indefinitely. When creating this object, the external environment needs to ensure that MediaSource exists.
    PusherProxy(const MediaSource::Ptr &src, int retry_count = -1, const toolkit::EventPoller::Ptr &poller = nullptr);
    ~PusherProxy() override;

    /**
     * 设置push结果回调，只触发一次；在publish执行之前有效
     * @param cb 回调对象
     * Set the push result callback, which is triggered only once; it is effective before publish is executed.
     * @param cb Callback object
     
     * [AUTO-TRANSLATED:7cd775fb]
     */
    void setPushCallbackOnce(const std::function<void(const toolkit::SockException &ex)> &cb);

    /**
     * 设置主动关闭回调
     * @param cb 回调对象
     * Set the active close callback
     * @param cb Callback object
     
     * [AUTO-TRANSLATED:83b7700a]
     */
    void setOnClose(const std::function<void(const toolkit::SockException &ex)> &cb);

    /**
     * 开始拉流播放
     * @param dstUrl 目标推流地址
     * Start pulling and playing the stream
     * @param dstUrl Target push stream address
     
     * [AUTO-TRANSLATED:a9a5da08]
     */
    void publish(const std::string &dstUrl) override;

    int getStatus();
    uint64_t getLiveSecs();
    uint64_t getRePublishCount();

private:
    // 重推逻辑函数  [AUTO-TRANSLATED:e0fa273c]
    // Repush logic function
    void rePublish(const std::string &dstUrl, int iFailedCnt);

private:
    int _retry_count;
    toolkit::Timer::Ptr _timer;
    toolkit::Ticker _live_ticker;
    // 0 表示正常 1 表示正在尝试推流  [AUTO-TRANSLATED:acb9835e]
    // 0 indicates normal, 1 indicates that the push stream is being attempted
    std::atomic<int> _live_status;
    std::atomic<uint64_t> _live_secs;
    std::atomic<uint64_t> _republish_count;
    std::weak_ptr<MediaSource> _weak_src;
    std::function<void(const toolkit::SockException &ex)> _on_close;
    std::function<void(const toolkit::SockException &ex)> _on_publish;
};

} /* namespace mediakit */

#endif // SRC_DEVICE_PUSHERPROXY_H
