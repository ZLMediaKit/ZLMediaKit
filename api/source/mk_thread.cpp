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
using namespace std;
using namespace toolkit;

API_EXPORT mk_thread API_CALL mk_thread_from_tcp_session(mk_tcp_session ctx){
    assert(ctx);
    TcpSession *obj = (TcpSession *)ctx;
    return obj->getPoller().get();
}

API_EXPORT mk_thread API_CALL mk_thread_from_tcp_client(mk_tcp_client ctx){
    assert(ctx);
    TcpClient::Ptr *client = (TcpClient::Ptr *)ctx;
    return (*client)->getPoller().get();
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

API_EXPORT mk_timer API_CALL mk_timer_create(mk_thread ctx,uint64_t delay_ms,on_mk_timer cb, void *user_data){
    assert(ctx && cb);
    EventPoller *poller = (EventPoller *)ctx;
    auto ret = poller->doDelayTask(delay_ms,[cb,user_data](){
        return cb(user_data);
    });
    return new DelayTask::Ptr(ret);
}

API_EXPORT void API_CALL mk_timer_release(mk_timer ctx){
    assert(ctx);
    DelayTask::Ptr *obj = (DelayTask::Ptr *)ctx;
    (*obj)->cancel();
    delete obj;
}