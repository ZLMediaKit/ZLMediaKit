/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef Timer_h
#define Timer_h

#include <functional>
#include "EventPoller.h"

namespace toolkit {

class Timer {
public:
    using Ptr = std::shared_ptr<Timer>;

    /**
     * 构造定时器
     * @param second 定时器重复秒数
     * @param cb 定时器任务，返回true表示重复下次任务，否则不重复，如果任务中抛异常，则默认重复下次任务
     * @param poller EventPoller对象，可以为nullptr
     */
    Timer(float second, const std::function<bool()> &cb, const EventPoller::Ptr &poller);
    ~Timer();

private:
    std::weak_ptr<EventPoller::DelayTask> _tag;
    //定时器保持EventPoller的强引用
    EventPoller::Ptr _poller;
};

}  // namespace toolkit
#endif /* Timer_h */
