/*
 * ThreadPool.h
 *
 *  Created on: 2013-10-11
 *      Author: root
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_


#include <assert.h>
#include <vector>
#include "threadgroup.h"
#include "TaskQueue.h"
#include "Util/util.h"
#include "Util/logger.h"

using namespace ZL::Util;

namespace ZL {
namespace Thread {
class ThreadPool {
public:
	enum Priority {
		PRIORITY_LOWEST = 0,
		PRIORITY_LOW,
		PRIORITY_NORMAL,
		PRIORITY_HIGH,
		PRIORITY_HIGHEST
	};

	//num:线程池线程个数
	ThreadPool(int num, Priority _priority = PRIORITY_NORMAL) :
			thread_num(num), avaible(true), priority(_priority) {
		start();
	}
	~ThreadPool() {
		wait();
	}

	//把任务打入线程池并异步执行
	template <typename T>
	bool async(T &&task) {
		if (!avaible) {
			return false;
		}
		if (my_thread_group.is_this_thread_in()) {
			task();
		} else {
			my_queue.push_task(std::forward<T>(task));
		}
		return true;
	}
	template <typename T>
	bool async_first(T &&task) {
		if (!avaible) {
			return false;
		}
		if (my_thread_group.is_this_thread_in()) {
			task();
		} else {
			my_queue.push_task_first(std::forward<T>(task));
		}
		return true;
	}
	template <typename T>
	bool sync(T &&task){
		semaphore sem;
		bool flag = async([&](){
			task();
			sem.post();
		});
		if(flag){
			sem.wait();
		}
		return flag;
	}
	template <typename T>
	bool sync_first(T &&task) {
		semaphore sem;
		bool flag = async_first([&]() {
			task();
			sem.post();
		});
		if (flag) {
			sem.wait();
		}
		return flag;
	}
	//同步等待线程池执行完所有任务并退出
	void wait() {
		exit();
		my_thread_group.join_all();
	}
    uint64_t size() const{
        return my_queue.size();
    }
	static ThreadPool &Instance() {
		//单例模式
		static ThreadPool instance(thread::hardware_concurrency());
		return instance;
	}
	static bool setPriority(Priority _priority = PRIORITY_NORMAL,
			thread::native_handle_type threadId = 0) {
		// set priority
#if defined(_WIN32)
		static int Priorities[] = { THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_HIGHEST };
		if (_priority != PRIORITY_NORMAL && SetThreadPriority(GetCurrentThread(), Priorities[_priority]) == 0) {
			return false;
		}
		return true;
#else
		static int Min = sched_get_priority_min(SCHED_OTHER);
		if (Min == -1) {
			return false;
		}
		static int Max = sched_get_priority_max(SCHED_OTHER);
		if (Max == -1) {
			return false;
		}
		static int Priorities[] = { Min, Min + (Max - Min) / 4, Min
			+ (Max - Min) / 2, Min + (Max - Min) / 4, Max };

		if (threadId == 0) {
			threadId = pthread_self();
		}
		struct sched_param params;
		params.sched_priority = Priorities[_priority];
		return pthread_setschedparam(threadId, SCHED_OTHER, &params) == 0;
#endif
	}
private:
	TaskQueue my_queue;
	thread_group my_thread_group;
	int thread_num;
	volatile bool avaible;
	Priority priority;
	//发送空任务至任务列队，通知线程主动退出
	void exit() {
		avaible = false;
		my_queue.push_exit(thread_num);
	}
	void start() {
		if (thread_num <= 0)
			return;
		for (int i = 0; i < thread_num; ++i) {
			my_thread_group.create_thread(bind(&ThreadPool::run, this));
		}
	}
	void run() {
		ThreadPool::setPriority(priority);
		function<void(void)> task;
		while (true) {
			if (my_queue.get_task(task)) {
				try {
					task();
				} catch (std::exception &ex) {
					FatalL << ex.what();
				}
				task = nullptr;
			} else {
				//空任务，退出线程
				break;
			}
		}
	}
}
;

} /* namespace Thread */
} /* namespace ZL */
#endif /* THREADPOOL_H_ */
