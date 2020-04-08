/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_PUSHER_MEDIAPUSHER_H_
#define SRC_PUSHER_MEDIAPUSHER_H_

#include <memory>
#include <string>
#include "PusherBase.h"
#include "Thread/TaskExecutor.h"
using namespace toolkit;

namespace mediakit {

class MediaPusher : public PusherImp<PusherBase,PusherBase> {
public:
    typedef std::shared_ptr<MediaPusher> Ptr;

    MediaPusher(const string &schema,
                const string &strVhost,
                const string &strApp,
                const string &strStream,
                const EventPoller::Ptr &poller = nullptr);

    MediaPusher(const MediaSource::Ptr &src,
                const EventPoller::Ptr &poller = nullptr);

    virtual ~MediaPusher();
    void publish(const string &strUrl) override;
    EventPoller::Ptr getPoller();
private:
    std::weak_ptr<MediaSource> _src;
    EventPoller::Ptr _poller;
};

} /* namespace mediakit */

#endif /* SRC_PUSHER_MEDIAPUSHER_H_ */
