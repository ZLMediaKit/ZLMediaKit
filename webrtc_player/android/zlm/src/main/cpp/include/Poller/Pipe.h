/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef Pipe_h
#define Pipe_h

#include <functional>
#include "PipeWrap.h"
#include "EventPoller.h"

namespace toolkit {

class Pipe {
public:
    using onRead = std::function<void(int size, const char *buf)>;

    Pipe(const onRead &cb = nullptr, const EventPoller::Ptr &poller = nullptr);
    ~Pipe();

    void send(const char *send, int size = 0);

private:
    std::shared_ptr<PipeWrap> _pipe;
    EventPoller::Ptr _poller;
};

}  // namespace toolkit
#endif /* Pipe_h */