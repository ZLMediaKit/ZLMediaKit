/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EventPoller_h
#define EventPoller_h

#include <mutex>
#include <thread>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include "PipeWrap.h"
#include "Util/logger.h"
#include "Util/List.h"
#include "Thread/TaskExecutor.h"
#include "Thread/ThreadPool.h"
#include "Network/Buffer.h"

#if defined(__linux__) || defined(__linux)
#define HAS_EPOLL
#endif //__linux__

namespace toolkit {

class EventPoller : public TaskExecutor, public AnyStorage, public std::enable_shared_from_this<EventPoller> {
public:
    friend class TaskExecutorGetterImp;

    using Ptr = std::shared_ptr<EventPoller>;
    using PollEventCB = std::function<void(int event)>;
    using PollCompleteCB = std::function<void(bool success)>;
    using DelayTask = TaskCancelableImp<uint64_t(void)>;

    typedef enum {
        Event_Read = 1 << 0, //读事件
        Event_Write = 1 << 1, //写事件
        Event_Error = 1 << 2, //错误事件
        Event_LT = 1 << 3,//水平触发
    } Poll_Event;

    ~EventPoller();

    /**
     * 获取EventPollerPool单例中的第一个EventPoller实例，
     * 保留该接口是为了兼容老代码
     * @return 单例
     */
    static EventPoller &Instance();

    /**
     * 添加事件监听
     * @param fd 监听的文件描述符
     * @param event 事件类型，例如 Event_Read | Event_Write
     * @param cb 事件回调functional
     * @return -1:失败，0:成功
     */
    int addEvent(int fd, int event, PollEventCB cb);

    /**
     * 删除事件监听
     * @param fd 监听的文件描述符
     * @param cb 删除成功回调functional
     * @return -1:失败，0:成功
     */
    int delEvent(int fd, PollCompleteCB cb = nullptr);

    /**
     * 修改监听事件类型
     * @param fd 监听的文件描述符
     * @param event 事件类型，例如 Event_Read | Event_Write
     * @return -1:失败，0:成功
     */
    int modifyEvent(int fd, int event, PollCompleteCB cb = nullptr);

    /**
     * 异步执行任务
     * @param task 任务
     * @param may_sync 如果调用该函数的线程就是本对象的轮询线程，那么may_sync为true时就是同步执行任务
     * @return 是否成功，一定会返回true
     */
    Task::Ptr async(TaskIn task, bool may_sync = true) override;

    /**
     * 同async方法，不过是把任务打入任务列队头，这样任务优先级最高
     * @param task 任务
     * @param may_sync 如果调用该函数的线程就是本对象的轮询线程，那么may_sync为true时就是同步执行任务
     * @return 是否成功，一定会返回true
     */
    Task::Ptr async_first(TaskIn task, bool may_sync = true) override;

    /**
     * 判断执行该接口的线程是否为本对象的轮询线程
     * @return 是否为本对象的轮询线程
     */
    bool isCurrentThread();

    /**
     * 延时执行某个任务
     * @param delay_ms 延时毫秒数
     * @param task 任务，返回值为0时代表不再重复任务，否则为下次执行延时，如果任务中抛异常，那么默认不重复任务
     * @return 可取消的任务标签
     */
    DelayTask::Ptr doDelayTask(uint64_t delay_ms, std::function<uint64_t()> task);

    /**
     * 获取当前线程关联的Poller实例
     */
    static EventPoller::Ptr getCurrentPoller();

    /**
     * 获取当前线程下所有socket共享的读缓存
     */
    BufferRaw::Ptr getSharedBuffer();

    /**
     * 获取poller线程id
     */
    std::thread::id getThreadId() const;

    /**
     * 获取线程名
     */
    const std::string& getThreadName() const;

private:
    /**
     * 本对象只允许在EventPollerPool中构造
     */
    EventPoller(std::string name);

    /**
     * 执行事件轮询
     * @param blocked 是否用执行该接口的线程执行轮询
     * @param ref_self 是记录本对象到thread local变量
     */
    void runLoop(bool blocked, bool ref_self);

    /**
     * 内部管道事件，用于唤醒轮询线程用
     */
    void onPipeEvent();

    /**
     * 切换线程并执行任务
     * @param task
     * @param may_sync
     * @param first
     * @return 可取消的任务本体，如果已经同步执行，则返回nullptr
     */
    Task::Ptr async_l(TaskIn task, bool may_sync = true, bool first = false);

    /**
     * 结束事件轮询
     * 需要指出的是，一旦结束就不能再次恢复轮询线程
     */
    void shutdown();

    /**
     * 刷新延时任务
     */
    uint64_t flushDelayTask(uint64_t now);

    /**
     * 获取select或epoll休眠时间
     */
    uint64_t getMinDelay();

    /**
     * 添加管道监听事件
     */
    void addEventPipe();

private:
    class ExitException : public std::exception {};

private:
    //标记loop线程是否退出
    bool _exit_flag;
    //线程名
    std::string _name;
    //当前线程下，所有socket共享的读缓存
    std::weak_ptr<BufferRaw> _shared_buffer;
    //执行事件循环的线程
    std::thread *_loop_thread = nullptr;
    //通知事件循环的线程已启动
    semaphore _sem_run_started;

    //内部事件管道
    PipeWrap _pipe;
    //从其他线程切换过来的任务
    std::mutex _mtx_task;
    List<Task::Ptr> _list_task;

    //保持日志可用
    Logger::Ptr _logger;

#if defined(HAS_EPOLL)
    //epoll相关
    int _epoll_fd = -1;
    unordered_map<int, std::shared_ptr<PollEventCB> > _event_map;
#else
    //select相关
    struct Poll_Record {
        using Ptr = std::shared_ptr<Poll_Record>;
        int fd;
        int event;
        int attach;
        PollEventCB call_back;
    };
    unordered_map<int, Poll_Record::Ptr> _event_map;
#endif //HAS_EPOLL
    unordered_map<int, bool> _event_cache_expired_map;

    //定时器相关
    std::multimap<uint64_t, DelayTask::Ptr> _delay_task_map;
};

class EventPollerPool : public std::enable_shared_from_this<EventPollerPool>, public TaskExecutorGetterImp {
public:
    using Ptr = std::shared_ptr<EventPollerPool>;
    static const std::string kOnStarted;
    #define EventPollerPoolOnStartedArgs EventPollerPool &pool, size_t &size

    ~EventPollerPool() = default;

    /**
     * 获取单例
     * @return
     */
    static EventPollerPool &Instance();

    /**
     * 设置EventPoller个数，在EventPollerPool单例创建前有效
     * 在不调用此方法的情况下，默认创建thread::hardware_concurrency()个EventPoller实例
     * @param size  EventPoller个数，如果为0则为thread::hardware_concurrency()
     */
    static void setPoolSize(size_t size = 0);

    /**
     * 内部创建线程是否设置cpu亲和性，默认设置cpu亲和性
     */
    static void enableCpuAffinity(bool enable);

    /**
     * 获取第一个实例
     * @return
     */
    EventPoller::Ptr getFirstPoller();

    /**
     * 根据负载情况获取轻负载的实例
     * 如果优先返回当前线程，那么会返回当前线程
     * 返回当前线程的目的是为了提高线程安全性
     * @param prefer_current_thread 是否优先获取当前线程
     */
    EventPoller::Ptr getPoller(bool prefer_current_thread = true);

    /**
     * 设置 getPoller() 是否优先返回当前线程
     * 在批量创建Socket对象时，如果优先返回当前线程，
     * 那么将导致负载不够均衡，所以可以暂时关闭然后再开启
     * @param flag 是否优先返回当前线程
     */
    void preferCurrentThread(bool flag = true);

private:
    EventPollerPool();

private:
    bool _prefer_current_thread = true;
};

}  // namespace toolkit
#endif /* EventPoller_h */
