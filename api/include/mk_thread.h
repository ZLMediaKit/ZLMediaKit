/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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

/**
 * 根据负载均衡算法，从事件线程池中随机获取一个事件线程
 * 如果在事件线程内执行此函数将返回本事件线程
 * 事件线程指的是定时器、网络io事件线程
 * @return 事件线程
 */
API_EXPORT mk_thread API_CALL mk_thread_from_pool();

/**
 * 根据负载均衡算法，从后台线程池中随机获取一个线程
 * 后台线程本质与事件线程相同，只是优先级更低，同时可以执行短时间的阻塞任务
 * ZLMediaKit中后台线程用于dns解析、mp4点播时的文件解复用
 * @return 后台线程
 */
API_EXPORT mk_thread API_CALL mk_thread_from_pool_work();

typedef void* mk_thread_pool;

/**
 * 创建线程池
 * @param name 线程池名称，方便调试
 * @param n_thread 线程个数，0时为cpu个数
 * @param priority 线程优先级，分为PRIORITY_LOWEST = 0,PRIORITY_LOW, PRIORITY_NORMAL, PRIORITY_HIGH, PRIORITY_HIGHEST
 * @return 线程池
 */
API_EXPORT mk_thread_pool API_CALL mk_thread_pool_create(const char *name, size_t n_thread, int priority);

/**
 * 销毁线程池
 * @param pool 线程池
 * @return 0:成功
 */
API_EXPORT int API_CALL mk_thread_pool_release(mk_thread_pool pool);

/**
 * 从线程池获取一个线程
 * @param pool 线程池
 * @return 线程
 */
API_EXPORT mk_thread API_CALL mk_thread_from_thread_pool(mk_thread_pool pool);

///////////////////////////////////////////线程切换/////////////////////////////////////////////
typedef void (API_CALL *on_mk_async)(void *user_data);

/**
 * 切换到事件线程并异步执行
 * @param ctx 事件线程
 * @param cb 回调函数
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_async_do(mk_thread ctx, on_mk_async cb, void *user_data);

/**
 * 切换到事件线程并延时执行
 * @param ctx 事件线程
 * @param ms 延时时间，单位毫秒
 * @param cb 回调函数
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_async_do_delay(mk_thread ctx, size_t ms, on_mk_async cb, void *user_data);

/**
 * 切换到事件线程并同步执行
 * @param ctx 事件线程
 * @param cb 回调函数
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_sync_do(mk_thread ctx, on_mk_async cb, void *user_data);

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
API_EXPORT mk_timer API_CALL mk_timer_create(mk_thread ctx, uint64_t delay_ms, on_mk_timer cb, void *user_data);

/**
 * 销毁和取消定时器
 * @param ctx 定时器对象
 */
API_EXPORT void API_CALL mk_timer_release(mk_timer ctx);

///////////////////////////////////////////信号量/////////////////////////////////////////////

typedef void* mk_sem;

/**
 * 创建信号量
 */
API_EXPORT mk_sem API_CALL mk_sem_create();

/**
 * 销毁信号量
 */
API_EXPORT void API_CALL mk_sem_release(mk_sem sem);

/**
 * 信号量加n
 */
API_EXPORT void API_CALL mk_sem_post(mk_sem sem, size_t n);

/**
 * 信号量减1
 * @param sem
 */
API_EXPORT void API_CALL mk_sem_wait(mk_sem sem);

#ifdef __cplusplus
}
#endif
#endif //MK_THREAD_H
