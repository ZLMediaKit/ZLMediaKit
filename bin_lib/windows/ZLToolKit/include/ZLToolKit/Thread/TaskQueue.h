/*
 * TaskQueue.h
 *
 *  Created on: 2013-10-11
 *      Author: root
 */

#ifndef TASKQUEUE_H_
#define TASKQUEUE_H_

#include <deque>
#include <atomic>
#include <mutex>
#include <functional>
#include "spin_mutex.h"
#include "semaphore.h"

using namespace std;

namespace ZL {
namespace Thread {

//实现了一个基于函数对象的任务列队，该列队是线程安全的，任务列队任务数由信号量控制
class TaskQueue {
public:
	TaskQueue() {
	}
	//打入任务至列队
	template <typename T>
	void push_task(T &&task_func) {
		{
			lock_guard<spin_mutex> lock(my_mutex);
			my_queue.emplace_back(std::forward<T>(task_func));
		}
		sem.post();
	}
	template <typename T>
	void push_task_first(T &&task_func) {
		{
			lock_guard<spin_mutex> lock(my_mutex);
			my_queue.emplace_front(std::forward<T>(task_func));
		}
		sem.post();
	}
	//清空任务列队
	void push_exit(unsigned int n) {
		sem.post(n);
	}
	//从列队获取一个任务，由执行线程执行
	bool get_task(function<void(void)> &tsk) {
		sem.wait();
		lock_guard<spin_mutex> lock(my_mutex);
		if (my_queue.size() == 0) {
			return false;
		}
		tsk = my_queue.front();
		my_queue.pop_front();
		return true;
	}
    uint64_t size() const{
        lock_guard<spin_mutex> lock(my_mutex);
        return my_queue.size();
    }
private:
	deque<function<void(void)>> my_queue;
	mutable spin_mutex my_mutex;
	semaphore sem;
};

} /* namespace Thread */
} /* namespace ZL */
#endif /* TASKQUEUE_H_ */
