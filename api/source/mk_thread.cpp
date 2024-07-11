﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
    SessionForC *obj = (SessionForC *)ctx;
    return (mk_thread)(obj->getPoller().get());
}

API_EXPORT mk_thread API_CALL mk_thread_from_tcp_client(mk_tcp_client ctx){
    assert(ctx);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *)ctx;
    return (mk_thread)((*client)->getPoller().get());
}

API_EXPORT mk_thread API_CALL mk_thread_from_pool(){
    return (mk_thread)(EventPollerPool::Instance().getPoller().get());
}

API_EXPORT mk_thread API_CALL mk_thread_from_pool_work(){
    return (mk_thread)(WorkThreadPool::Instance().getPoller().get());
}

API_EXPORT void API_CALL mk_async_do(mk_thread ctx,on_mk_async cb, void *user_data){
    assert(ctx && cb);
    EventPoller *poller = (EventPoller *)ctx;
    poller->async([cb,user_data](){
        cb(user_data);
    });
}

API_EXPORT void API_CALL mk_async_do2(mk_thread ctx, on_mk_async cb, void *user_data, on_user_data_free user_data_free){
    assert(ctx && cb);
    EventPoller *poller = (EventPoller *)ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    poller->async([cb, ptr]() { cb(ptr.get()); });
}

API_EXPORT void API_CALL mk_async_do_delay(mk_thread ctx, size_t ms, on_mk_async cb, void *user_data) {
    mk_async_do_delay2(ctx, ms, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_async_do_delay2(mk_thread ctx, size_t ms, on_mk_async cb, void *user_data, on_user_data_free user_data_free){
    assert(ctx && cb && ms);
    EventPoller *poller = (EventPoller *)ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    poller->doDelayTask(ms, [cb, ptr]() {
        cb(ptr.get());
        return 0;
    });
}

API_EXPORT void API_CALL mk_sync_do(mk_thread ctx,on_mk_async cb, void *user_data){
    assert(ctx && cb);
    EventPoller *poller = (EventPoller *)ctx;
    poller->sync([cb, user_data]() { cb(user_data); });
}

class TimerForC : public std::enable_shared_from_this<TimerForC>{
public:
    using Ptr = std::shared_ptr<TimerForC>;

    TimerForC(on_mk_timer cb, std::shared_ptr<void> user_data) {
        _cb = cb;
        _user_data = std::move(user_data);
    }

    uint64_t operator()(){
        lock_guard<recursive_mutex> lck(_mxt);
        if(!_cb){
            return 0;
        }
        return _cb(_user_data.get());
    }

    void cancel(){
        lock_guard<recursive_mutex> lck(_mxt);
        _cb = nullptr;
        _task->cancel();
    }

    void start(uint64_t ms ,EventPoller &poller){
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
    std::shared_ptr<void> _user_data;
    recursive_mutex _mxt;
    EventPoller::DelayTask::Ptr _task;
};

API_EXPORT mk_timer API_CALL mk_timer_create(mk_thread ctx, uint64_t delay_ms, on_mk_timer cb, void *user_data) {
    return mk_timer_create2(ctx, delay_ms, cb, user_data, nullptr);
}

API_EXPORT mk_timer API_CALL mk_timer_create2(mk_thread ctx, uint64_t delay_ms, on_mk_timer cb, void *user_data, on_user_data_free user_data_free){
    assert(ctx && cb);
    EventPoller *poller = (EventPoller *)ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    TimerForC::Ptr *ret = new TimerForC::Ptr(new TimerForC(cb, ptr));
    (*ret)->start(delay_ms,*poller);
    return (mk_timer)ret;
}

API_EXPORT void API_CALL mk_timer_release(mk_timer ctx){
    assert(ctx);
    TimerForC::Ptr *obj = (TimerForC::Ptr *)ctx;
    (*obj)->cancel();
    delete obj;
}

class WorkThreadPoolForC : public TaskExecutorGetterImp {
public:
    WorkThreadPoolForC(const char *name, size_t n_thread, int priority) {
        //最低优先级
        addPoller(name, n_thread, (ThreadPool::Priority) priority, false);
    }

    EventPoller::Ptr getPoller() {
        return static_pointer_cast<EventPoller>(getExecutor());
    }
};

API_EXPORT mk_thread_pool API_CALL mk_thread_pool_create(const char *name, size_t n_thread, int priority) {
    return (mk_thread_pool)new WorkThreadPoolForC(name, n_thread, priority);
}

API_EXPORT int API_CALL mk_thread_pool_release(mk_thread_pool pool) {
    assert(pool);
    delete (WorkThreadPoolForC *) pool;
    return 0;
}

API_EXPORT mk_thread API_CALL mk_thread_from_thread_pool(mk_thread_pool pool) {
    assert(pool);
    return (mk_thread)(((WorkThreadPoolForC *) pool)->getPoller().get());
}

API_EXPORT mk_sem API_CALL mk_sem_create() {
    return (mk_sem)new toolkit::semaphore;
}

API_EXPORT void API_CALL mk_sem_release(mk_sem sem) {
    assert(sem);
    delete (toolkit::semaphore *) sem;
}

API_EXPORT void API_CALL mk_sem_post(mk_sem sem, size_t n) {
    assert(sem);
    ((toolkit::semaphore *) sem)->post(n);
}

API_EXPORT void API_CALL mk_sem_wait(mk_sem sem) {
    assert(sem);
    ((toolkit::semaphore *) sem)->wait();
}