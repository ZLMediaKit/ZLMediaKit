/*
 * WorkThreadPool.h
 *
 *  Created on: 2015年10月30日
 *      Author: root
 */

#ifndef UTIL_WORKTHREADPOOL_H_
#define UTIL_WORKTHREADPOOL_H_

#include <map>
#include <thread>
#include <memory>
#include <atomic>
#include <iostream>
#include <unordered_map>
#include "ThreadPool.h"

using namespace std;

namespace ZL {
namespace Thread {

class WorkThreadPool {
public:
	WorkThreadPool(int threadnum = thread::hardware_concurrency());
	virtual ~WorkThreadPool();
	std::shared_ptr<ThreadPool> &getWorkThread();
	static WorkThreadPool &Instance() {
		static WorkThreadPool *intance(new WorkThreadPool());
		return *intance;
	}
	static void Destory(){
		delete &(WorkThreadPool::Instance());
	}
private:
	int threadnum;
	atomic<int> threadPos;
	vector <std::shared_ptr<ThreadPool> > threads;
	void wait();
};

} /* namespace Thread */
} /* namespace ZL */

#endif /* UTIL_WORKTHREADPOOL_H_ */
