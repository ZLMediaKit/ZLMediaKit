/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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