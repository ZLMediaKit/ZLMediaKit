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

#ifndef MK_THREAD_H
#define MK_THREAD_H

#include <assert.h>
#include "mk_common.h"
#include "mk_tcp.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////事件线程/////////////////////////////////////////////
typedef void* mk_thread;

/**
 * 获取tcp会话对象所在事件线程
 * @param ctx tcp会话对象
 * @return 对象所在事件线程
 */
API_EXPORT mk_thread API_CALL mk_thread_from_tcp_session(mk_tcp_session ctx);

/**
 * 获取tcp客户端对象所在事件线程
 * @param ctx tcp客户端
 * @return 对象所在事件线程
 */
API_EXPORT mk_thread API_CALL mk_thread_from_tcp_client(mk_tcp_client ctx);

///////////////////////////////////////////线程切换/////////////////////////////////////////////
typedef void (API_CALL *on_mk_async)(void *user_data);

/**
 * 切换到事件线程并异步执行
 * @param ctx 事件线程
 * @param cb 回调函数
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_async_do(mk_thread ctx,on_mk_async cb, void *user_data);

/**
 * 切换到事件线程并同步执行
 * @param ctx 事件线程
 * @param cb 回调函数
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_sync_do(mk_thread ctx,on_mk_async cb, void *user_data);

///////////////////////////////////////////定时器/////////////////////////////////////////////
typedef void* mk_timer;

/**
 * 定时器触发事件
 * @return 下一次触发延时(单位毫秒)，返回0则不再重复
 */
typedef uint64_t (API_CALL *on_mk_timer)(void *user_data);

/**
 * 创建定时器
 * @param ctx 线程对象
 * @param delay_ms 执行延时，单位毫秒
 * @param cb 回调函数
 * @param user_data 用户数据指针
 * @return 定时器对象
 */
API_EXPORT mk_timer API_CALL mk_timer_create(mk_thread ctx,uint64_t delay_ms, on_mk_timer cb, void *user_data);

/**
 * 销毁和取消定时器
 * @param ctx 定时器对象
 */
API_EXPORT void API_CALL mk_timer_release(mk_timer ctx);

#ifdef __cplusplus
}
#endif
#endif //MK_THREAD_H
