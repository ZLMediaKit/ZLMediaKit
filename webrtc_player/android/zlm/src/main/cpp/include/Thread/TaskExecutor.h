/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLTOOLKIT_TASKEXECUTOR_H
#define ZLTOOLKIT_TASKEXECUTOR_H

#include <mutex>
#include <memory>
#include <functional>
#include "Util/List.h"
#include "Util/util.h"

namespace toolkit {

/**
* cpu负载计算器
*/
class ThreadLoadCounter {
public:
    /**
     * 构造函数
     * @param max_size 统计样本数量
     * @param max_usec 统计时间窗口,亦即最近{max_usec}的cpu负载率
     */
    ThreadLoadCounter(uint64_t max_size, uint64_t max_usec);
    ~ThreadLoadCounter() = default;

    /**
     * 线程进入休眠
     */
    void startSleep();

    /**
     * 休眠唤醒,结束休眠
     */
    void sleepWakeUp();

    /**
     * 返回当前线程cpu使用率，范围为 0 ~ 100
     * @return 当前线程cpu使用率
     */
    int load();

private:
    struct TimeRecord {
        TimeRecord(uint64_t tm, bool slp) {
            _time = tm;
            _sleep = slp;
        }

        bool _sleep;
        uint64_t _time;
    };

private:
    bool _sleeping = true;
    uint64_t _last_sleep_time;
    uint64_t _last_wake_time;
    uint64_t _max_size;
    uint64_t _max_usec;
    std::mutex _mtx;
    List<TimeRecord> _time_list;
};

class TaskCancelable : public noncopyable {
public:
    TaskCancelable() = default;
    virtual ~TaskCancelable() = default;
    virtual void cancel() = 0;
};

template<class R, class... ArgTypes>
class TaskCancelableImp;

template<class R, class... ArgTypes>
class TaskCancelableImp<R(ArgTypes...)> : public TaskCancelable {
public:
    using Ptr = std::shared_ptr<TaskCancelableImp>;
    using func_type = std::function<R(ArgTypes...)>;

    ~TaskCancelableImp() = default;

    template<typename FUNC>
    TaskCancelableImp(FUNC &&task) {
        _strongTask = std::make_shared<func_type>(std::forward<FUNC>(task));
        _weakTask = _strongTask;
    }

    void cancel() override {
        _strongTask = nullptr;
    }

    operator bool() {
        return _strongTask && *_strongTask;
    }

    void operator=(std::nullptr_t) {
        _strongTask = nullptr;
    }

    R operator()(ArgTypes ...args) const {
        auto strongTask = _weakTask.lock();
        if (strongTask && *strongTask) {
            return (*strongTask)(std::forward<ArgTypes>(args)...);
        }
        return defaultValue<R>();
    }

    template<typename T>
    static typename std::enable_if<std::is_void<T>::value, void>::type
    defaultValue() {}

    template<typename T>
    static typename std::enable_if<std::is_pointer<T>::value, T>::type
    defaultValue() {
        return nullptr;
    }

    template<typename T>
    static typename std::enable_if<std::is_integral<T>::value, T>::type
    defaultValue() {
        return 0;
    }

protected:
    std::weak_ptr<func_type> _weakTask;
    std::shared_ptr<func_type> _strongTask;
};

using TaskIn = std::function<void()>;
using Task = TaskCancelableImp<void()>;

class TaskExecutorInterface {
public:
    TaskExecutorInterface() = default;
    virtual ~TaskExecutorInterface() = default;

    /**
     * 异步执行任务
     * @param task 任务
     * @param may_sync 是否允许同步执行该任务
     * @return 任务是否添加成功
     */
    virtual Task::Ptr async(TaskIn task, bool may_sync = true) = 0;

    /**
     * 最高优先级方式异步执行任务
     * @param task 任务
     * @param may_sync 是否允许同步执行该任务
     * @return 任务是否添加成功
     */
    virtual Task::Ptr async_first(TaskIn task, bool may_sync = true);

    /**
     * 同步执行任务
     * @param task
     * @return
     */
    void sync(const TaskIn &task);

    /**
     * 最高优先级方式同步执行任务
     * @param task
     * @return
     */
    void sync_first(const TaskIn &task);
};

/**
* 任务执行器
*/
class TaskExecutor : public ThreadLoadCounter, public TaskExecutorInterface {
public:
    using Ptr = std::shared_ptr<TaskExecutor>;

    /**
     * 构造函数
     * @param max_size cpu负载统计样本数
     * @param max_usec cpu负载统计时间窗口大小
     */
    TaskExecutor(uint64_t max_size = 32, uint64_t max_usec = 2 * 1000 * 1000);
    ~TaskExecutor() = default;
};

class TaskExecutorGetter {
public:
    using Ptr = std::shared_ptr<TaskExecutorGetter>;

    virtual ~TaskExecutorGetter() = default;

    /**
     * 获取任务执行器
     * @return 任务执行器
     */
    virtual TaskExecutor::Ptr getExecutor() = 0;

    /**
     * 获取执行器个数
     */
    virtual size_t getExecutorSize() const = 0;
};

class TaskExecutorGetterImp : public TaskExecutorGetter {
public:
    TaskExecutorGetterImp() = default;
    ~TaskExecutorGetterImp() = default;

    /**
     * 根据线程负载情况，获取最空闲的任务执行器
     * @return 任务执行器
     */
    TaskExecutor::Ptr getExecutor() override;

    /**
     * 获取所有线程的负载率
     * @return 所有线程的负载率
     */
    std::vector<int> getExecutorLoad();

    /**
     * 获取所有线程任务执行延时，单位毫秒
     * 通过此函数也可以大概知道线程负载情况
     * @return
     */
    void getExecutorDelay(const std::function<void(const std::vector<int> &)> &callback);

    /**
     * 遍历所有线程
     */
    void for_each(const std::function<void(const TaskExecutor::Ptr &)> &cb);

    /**
     * 获取线程数
     */
    size_t getExecutorSize() const override;

protected:
    size_t addPoller(const std::string &name, size_t size, int priority, bool register_thread, bool enable_cpu_affinity = true);

protected:
    size_t _thread_pos = 0;
    std::vector<TaskExecutor::Ptr> _threads;
};

}//toolkit
#endif //ZLTOOLKIT_TASKEXECUTOR_H
