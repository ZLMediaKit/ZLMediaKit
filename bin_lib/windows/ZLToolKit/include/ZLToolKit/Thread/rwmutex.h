/*
 * rwmutex.h
 *
 *  Created on: 2016年1月27日
 *      Author: 熊子良
 */

#ifndef UTIL_RWMUTEX_H_
#define UTIL_RWMUTEX_H_

#include <mutex>
#include <atomic>

using namespace std;

namespace ZL {
namespace Thread {

class rw_mutex {
public:
	rw_mutex() :
			reader_cnt(0) {
	}
	void lock(bool write_mode = true) {
		if (write_mode) {
			//write thread
			mtx_write.lock();
		} else {
			// read thread
			lock_guard<mutex> lck(mtx_reader);
			if (reader_cnt++ == 0) {
				mtx_write.lock();
			}
		}
	}
	void unlock(bool write_mode = true) {
		if (write_mode) {
			//write thread
			mtx_write.unlock();
		} else {
			// read thread
			lock_guard<mutex> lck(mtx_reader);
			if (--reader_cnt == 0) {
				mtx_write.unlock();
			}
		}
	}
	virtual ~rw_mutex() {
	}
private:
	mutex mtx_write;
	mutex mtx_reader;
	int reader_cnt;
};
class lock_guard_rw {
public:
	lock_guard_rw(rw_mutex &_mtx, bool _write_mode = true) :
			mtx(_mtx), write_mode(_write_mode) {
		mtx.lock(write_mode);
	}
	virtual ~lock_guard_rw() {
		mtx.unlock(write_mode);
	}
private:
	rw_mutex &mtx;
	bool write_mode;
};
} /* namespace Thread */
} /* namespace ZL */

#endif /* UTIL_RWMUTEX_H_ */
