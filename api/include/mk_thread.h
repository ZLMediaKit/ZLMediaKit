/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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

// /////////////////////////////////////////事件线程/////////////////////////////////////////////  [AUTO-TRANSLATED:6d564b3b]
// /////////////////////////////////////////事件线程/////////////////////////////////////////////
typedef struct mk_thread_t *mk_thread;

/**
 * 获取tcp会话对象所在事件线程
 * @param ctx tcp会话对象
 * @return 对象所在事件线程
 * Get the event thread where the tcp session object is located
 * @param ctx tcp session object
 * @return The event thread where the object is located
 
 * [AUTO-TRANSLATED:17da57ec]
 */
API_EXPORT mk_thread API_CALL mk_thread_from_tcp_session(mk_tcp_session ctx);

/**
 * 获取tcp客户端对象所在事件线程
 * @param ctx tcp客户端
 * @return 对象所在事件线程
 * Get the event thread where the tcp client object is located
 * @param ctx tcp client
 * @return The event thread where the object is located
 
 * [AUTO-TRANSLATED:15d4174b]
 */
API_EXPORT mk_thread API_CALL mk_thread_from_tcp_client(mk_tcp_client ctx);

/**
 * 根据负载均衡算法，从事件线程池中随机获取一个事件线程
 * 如果在事件线程内执行此函数将返回本事件线程
 * 事件线程指的是定时器、网络io事件线程
 * @return 事件线程
 * Get an event thread randomly from the event thread pool according to the load balancing algorithm
 * If this function is executed within the event thread, it will return the current event thread
 * Event thread refers to timer, network io event thread
 * @return Event thread
 
 * [AUTO-TRANSLATED:5da37e1f]
 */
API_EXPORT mk_thread API_CALL mk_thread_from_pool();

/**
 * 根据负载均衡算法，从后台线程池中随机获取一个线程
 * 后台线程本质与事件线程相同，只是优先级更低，同时可以执行短时间的阻塞任务
 * ZLMediaKit中后台线程用于dns解析、mp4点播时的文件解复用
 * @return 后台线程
 * Get a thread randomly from the background thread pool according to the load balancing algorithm
 * Background threads are essentially the same as event threads, but they have lower priority and can execute short-term blocking tasks
 * Background threads in ZLMediaKit are used for dns resolution, file demultiplexing during mp4 on-demand
 * @return Background thread
 
 * [AUTO-TRANSLATED:3b552537]
 */
API_EXPORT mk_thread API_CALL mk_thread_from_pool_work();

typedef struct mk_thread_pool_t *mk_thread_pool;

/**
 * 创建线程池
 * @param name 线程池名称，方便调试
 * @param n_thread 线程个数，0时为cpu个数
 * @param priority 线程优先级，分为PRIORITY_LOWEST = 0,PRIORITY_LOW, PRIORITY_NORMAL, PRIORITY_HIGH, PRIORITY_HIGHEST
 * @return 线程池
 * Create a thread pool
 * @param name Thread pool name, for debugging
 * @param n_thread Number of threads, 0 for the number of cpus
 * @param priority Thread priority, divided into PRIORITY_LOWEST = 0,PRIORITY_LOW, PRIORITY_NORMAL, PRIORITY_HIGH, PRIORITY_HIGHEST
 * @return Thread pool
 
 * [AUTO-TRANSLATED:177acea2]
 */
API_EXPORT mk_thread_pool API_CALL mk_thread_pool_create(const char *name, size_t n_thread, int priority);

/**
 * 销毁线程池
 * @param pool 线程池
 * @return 0:成功
 * Destroy the thread pool
 * @param pool Thread pool
 * @return 0: Success
 
 * [AUTO-TRANSLATED:1f1b3582]
 */
API_EXPORT int API_CALL mk_thread_pool_release(mk_thread_pool pool);

/**
 * 从线程池获取一个线程
 * @param pool 线程池
 * @return 线程
 * Get a thread from the thread pool
 * @param pool Thread pool
 * @return Thread
 
 * [AUTO-TRANSLATED:f47de48e]
 */
API_EXPORT mk_thread API_CALL mk_thread_from_thread_pool(mk_thread_pool pool);

// /////////////////////////////////////////线程切换/////////////////////////////////////////////  [AUTO-TRANSLATED:5fc795bf]
// /////////////////////////////////////////线程切换/////////////////////////////////////////////
typedef void (API_CALL *on_mk_async)(void *user_data);

/**
 * 切换到事件线程并异步执行
 * @param ctx 事件线程
 * @param cb 回调函数
 * @param user_data 用户数据指针
 * Switch to the event thread and execute asynchronously
 * @param ctx Event thread
 * @param cb Callback function
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:55773ed5]
 */
API_EXPORT void API_CALL mk_async_do(mk_thread ctx, on_mk_async cb, void *user_data);
API_EXPORT void API_CALL mk_async_do2(mk_thread ctx, on_mk_async cb, void *user_data, on_user_data_free user_data_free);

/**
 * 切换到事件线程并延时执行
 * @param ctx 事件线程
 * @param ms 延时时间，单位毫秒
 * @param cb 回调函数
 * @param user_data 用户数据指针
 * Switch to the event thread and execute with delay
 * @param ctx Event thread
 * @param ms Delay time, in milliseconds
 * @param cb Callback function
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:35dfdb0e]
 */
API_EXPORT void API_CALL mk_async_do_delay(mk_thread ctx, size_t ms, on_mk_async cb, void *user_data);
API_EXPORT void API_CALL mk_async_do_delay2(mk_thread ctx, size_t ms, on_mk_async cb, void *user_data, on_user_data_free user_data_free);

/**
 * 切换到事件线程并同步执行
 * @param ctx 事件线程
 * @param cb 回调函数
 * @param user_data 用户数据指针
 * Switch to the event thread and execute synchronously
 * @param ctx Event thread
 * @param cb Callback function
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:1326dfb2]
 */
API_EXPORT void API_CALL mk_sync_do(mk_thread ctx, on_mk_async cb, void *user_data);

// /////////////////////////////////////////定时器/////////////////////////////////////////////  [AUTO-TRANSLATED:7f76781c]
// /////////////////////////////////////////定时器/////////////////////////////////////////////
typedef struct mk_timer_t *mk_timer;

/**
 * 定时器触发事件
 * @return 下一次触发延时(单位毫秒)，返回0则不再重复
 * Timer trigger event
 * @return Next trigger delay (in milliseconds), return 0 to stop repeating
 
 * [AUTO-TRANSLATED:f8846f56]
 */
typedef uint64_t (API_CALL *on_mk_timer)(void *user_data);

/**
 * 创建定时器
 * @param ctx 线程对象
 * @param delay_ms 执行延时，单位毫秒
 * @param cb 回调函数
 * @param user_data 用户数据指针
 * @return 定时器对象
 * Create a timer
 * @param ctx Thread object
 * @param delay_ms Execution delay, in milliseconds
 * @param cb Callback function
 * @param user_data User data pointer
 * @return Timer object
 
 * [AUTO-TRANSLATED:2d47864a]
 */
API_EXPORT mk_timer API_CALL mk_timer_create(mk_thread ctx, uint64_t delay_ms, on_mk_timer cb, void *user_data);
API_EXPORT mk_timer API_CALL mk_timer_create2(mk_thread ctx, uint64_t delay_ms, on_mk_timer cb, void *user_data, on_user_data_free user_data_free);

/**
 * 销毁和取消定时器
 * @param ctx 定时器对象
 * Destroy and cancel the timer
 * @param ctx Timer object
 
 * [AUTO-TRANSLATED:3fdb8534]
 */
API_EXPORT void API_CALL mk_timer_release(mk_timer ctx);

// /////////////////////////////////////////信号量/////////////////////////////////////////////  [AUTO-TRANSLATED:f41da57a]
// /////////////////////////////////////////信号量/////////////////////////////////////////////

typedef struct mk_sem_t *mk_sem;

/**
 * 创建信号量
 * Create a semaphore
 
 * [AUTO-TRANSLATED:dcd83058]
 */
API_EXPORT mk_sem API_CALL mk_sem_create();

/**
 * 销毁信号量
 * Destroy the semaphore
 
 * [AUTO-TRANSLATED:b298797b]
 */
API_EXPORT void API_CALL mk_sem_release(mk_sem sem);

/**
 * 信号量加n
 * Increase the semaphore by n
 
 * [AUTO-TRANSLATED:1f455c5d]
 */
API_EXPORT void API_CALL mk_sem_post(mk_sem sem, size_t n);

/**
 * 信号量减1
 * @param sem
 * Decrease the semaphore by 1
 * @param sem
 
 * [AUTO-TRANSLATED:626595d8]
 */
API_EXPORT void API_CALL mk_sem_wait(mk_sem sem);

#ifdef __cplusplus
}
#endif
#endif //MK_THREAD_H
