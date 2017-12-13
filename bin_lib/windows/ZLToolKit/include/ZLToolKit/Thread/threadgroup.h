/*
 * threadgroup.h
 *
 *  Created on: 2014-6-23
 *      Author: root
 */

#ifndef THREADGROUP_H_
#define THREADGROUP_H_

#include <set>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>

using namespace std;

namespace ZL {
namespace Thread {

class thread_group {
private:
	thread_group(thread_group const&);
	thread_group& operator=(thread_group const&);
public:
	thread_group() {
	}
	~thread_group() {
		for (auto &th : threads) {
			delete th.second;
		}
	}

	bool is_this_thread_in() {
		auto it = threads.find(this_thread::get_id());
		return it != threads.end();
	}

	bool is_thread_in(thread* thrd) {
		if (thrd) {
			auto it = threads.find(thrd->get_id());
			return it != threads.end();
		} else {
			return false;
		}
	}

	template<typename F>
	thread* create_thread(F threadfunc) {
		thread  *new_thread=new thread(threadfunc);
		threads[new_thread->get_id()] = new_thread;
		return new_thread;
	}

	void add_thread(thread* thrd) {
		if (thrd) {
			if (is_thread_in(thrd)) {
				throw runtime_error(
						"thread_group: trying to add a duplicated thread");
			}
			threads[thrd->get_id()] = thrd;
		}
	}

	void remove_thread(thread* thrd) {
		auto it = threads.find(thrd->get_id());
		if (it != threads.end()) {
			threads.erase(it);
		}
	}
	void join_all() {
		if (is_this_thread_in()) {
			throw runtime_error("thread_group: trying joining itself");
			return;
		}
		for (auto &it : threads) {
			if (it.second->joinable()) {
				it.second->join(); //等待线程主动退出
			}
		}
	}
	size_t size() {
		return threads.size();
	}
private:
	unordered_map<thread::id, thread*> threads;

};

} /* namespace Thread */
} /* namespace ZL */
#endif /* THREADGROUP_H_ */
