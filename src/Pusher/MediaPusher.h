/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_PUSHER_MEDIAPUSHER_H_
#define SRC_PUSHER_MEDIAPUSHER_H_

#include <memory>
#include <string>
#include "PusherBase.h"

namespace mediakit {

class MediaPusher : public PusherImp<PusherBase,PusherBase> {
public:
    using Ptr = std::shared_ptr<MediaPusher>;

    MediaPusher(const std::string &schema,
                const std::string &vhost,
                const std::string &app,
                const std::string &stream,
                const toolkit::EventPoller::Ptr &poller = nullptr);

    MediaPusher(const MediaSource::Ptr &src,
                const toolkit::EventPoller::Ptr &poller = nullptr);

    void publish(const std::string &url) override;
    toolkit::EventPoller::Ptr getPoller();
    void setOnCreateSocket(toolkit::Socket::onCreateSocket cb);

private:
    std::weak_ptr<MediaSource> _src;
    toolkit::EventPoller::Ptr _poller;
    toolkit::Socket::onCreateSocket _on_create_socket;
};

} /* namespace mediakit */

#endif /* SRC_PUSHER_MEDIAPUSHER_H_ */
