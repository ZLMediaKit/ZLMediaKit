//
//  AsyncTaskThread.h
//  xzl
//
//  Created by xzl on 15/6/8.
//

#ifndef AsyncTaskThread_h
#define AsyncTaskThread_h

#include <stdio.h>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include "Util/util.h"

using namespace std;
using namespace ZL::Util;

#define TASK_INTERVAL 50

namespace ZL {
namespace Thread {
typedef struct {
	uint64_t type;
	uint64_t timeLine;
	uint64_t tickTime;
	function<bool()> task;
} TaskInfo;


class AsyncTaskThread {
public:
	//the timer default 30s
	AsyncTaskThread(uint64_t millisecond_sleep);
	~AsyncTaskThread();
	void DoTaskDelay(uint64_t type, uint64_t millisecond, const function<bool()> &func);
	void CancelTask(uint64_t type);
	static AsyncTaskThread &Instance(uint32_t millisecond_sleep = TASK_INTERVAL) {
		static AsyncTaskThread *instance(new AsyncTaskThread(millisecond_sleep));
		return *instance;
	}
	static void Destory(){
		delete &AsyncTaskThread::Instance();
	}
private:
	recursive_mutex _mtx;
	unordered_multimap<uint64_t, std::shared_ptr<TaskInfo> > taskMap;
	unordered_set<uint64_t> needCancel;
	inline uint64_t getNowTime();
	thread *taskThread;
	void DoTask();
	atomic_bool threadExit;
	condition_variable_any cond;
	uint64_t millisecond_sleep;
};

class AsyncTaskHelper
{
public:
	AsyncTaskHelper(uint64_t millisecond,const function<bool()> &task){
		AsyncTaskThread::Instance().DoTaskDelay(reinterpret_cast<uint64_t>(this),millisecond,task);
	}
	virtual ~AsyncTaskHelper(){
		AsyncTaskThread::Instance().CancelTask(reinterpret_cast<uint64_t>(this));
	}
};

} /* namespace Thread */
} /* namespace ZL */

#endif /* defined(AsyncTaskThread_h) */
