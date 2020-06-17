/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_thread.h"
#include "mk_tcp_private.h"
#include "Util/logger.h"
#include "Poller/EventPoller.h"
#include "Thread/WorkThreadPool.h"
using namespace std;
using namespace toolkit;

API_EXPORT mk_thread API_CALL mk_thread_from_tcp_session(mk_tcp_session ctx){
    assert(ctx);
    TcpSessionForC *obj = (TcpSessionForC *)ctx;
    return obj->getPoller().get();
}

API_EXPORT mk_thread API_CALL mk_thread_from_tcp_client(mk_tcp_client ctx){
    assert(ctx);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *)ctx;
    return (*client)->getPoller().get();
}

API_EXPORT mk_thread API_CALL mk_thread_from_pool(){
    return EventPollerPool::Instance().getPoller().get();
}

API_EXPORT mk_thread API_CALL mk_thread_from_pool_work(){
    return WorkThreadPool::Instance().getPoller().get();
}

API_EXPORT void API_CALL mk_async_do(mk_thread ctx,on_mk_async cb, void *user_data){
    assert(ctx && cb);
    EventPoller *poller = (EventPoller *)ctx;
    poller->async([cb,user_data](){
        cb(user_data);
    });
}

API_EXPORT void API_CALL mk_sync_do(mk_thread ctx,on_mk_async cb, void *user_data){
    assert(ctx && cb);
    EventPoller *poller = (EventPoller *)ctx;
    poller->sync([cb,user_data](){
        cb(user_data);
    });
}

class TimerForC : public std::enable_shared_from_this<TimerForC>{
public:
    typedef std::shared_ptr<TimerForC> Ptr;

    TimerForC(on_mk_timer cb, void *user_data){
        _cb = cb;
        _user_data = user_data;
    }

    ~TimerForC(){}

    uint64_t operator()(){
        lock_guard<recursive_mutex> lck(_mxt);
        if(!_cb){
            return 0;
        }
        return _cb(_user_data);
    }

    void cancel(){
        lock_guard<recursive_mutex> lck(_mxt);
        _cb = nullptr;
        _task->cancel();
    }

    void start(int ms ,EventPoller &poller){
        weak_ptr<TimerForC> weak_self = shared_from_this();
        _task = poller.doDelayTask(ms, [weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return (uint64_t) 0;
            }
            return (*strong_self)();
        });
    }
private:
    on_mk_timer _cb = nullptr;
    void *_user_data = nullptr;
    recursive_mutex _mxt;
    DelayTask::Ptr _task;
};

API_EXPORT mk_timer API_CALL mk_timer_create(mk_thread ctx,uint64_t delay_ms,on_mk_timer cb, void *user_data){
    assert(ctx && cb);
    EventPoller *poller = (EventPoller *)ctx;
    TimerForC::Ptr *ret = new TimerForC::Ptr(new TimerForC(cb, user_data));
    (*ret)->start(delay_ms,*poller);
    return ret;
}

API_EXPORT void API_CALL mk_timer_release(mk_timer ctx){
    assert(ctx);
    TimerForC::Ptr *obj = (TimerForC::Ptr *)ctx;
    (*obj)->cancel();
    delete obj;
}