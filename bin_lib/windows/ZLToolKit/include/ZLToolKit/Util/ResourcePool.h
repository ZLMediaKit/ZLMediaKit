/*
 * ResourcePool.h
 *
 *  Created on: 2015年10月29日
 *      Author: root
 */

#ifndef UTIL_RECYCLEPOOL_H_
#define UTIL_RECYCLEPOOL_H_

#include <mutex>
#include <deque>
#include <memory>
#include <atomic>
#include <functional>
#include <unordered_set>

namespace ZL {
namespace Util {
using namespace std;

template<typename C, int poolSize = 10>
class ResourcePool {
public:
	typedef std::shared_ptr<C> ValuePtr;
	ResourcePool() {
			pool.reset(new _ResourcePool());
	}
#if (!defined(__GNUC__)) || (__GNUC__ >= 5)
	template<typename ...ArgTypes>
	ResourcePool(ArgTypes &&...args) {
		pool.reset(new _ResourcePool(std::forward<ArgTypes>(args)...));
	}
#endif //(!defined(__GNUC__)) || (__GNUC__ >= 5)
	void reSize(int size) {
		pool->setSize(size);
	}
	ValuePtr obtain() {
		return pool->obtain();
	}
	void quit(const ValuePtr &ptr) {
		pool->quit(ptr);
	}
private:

	class _ResourcePool: public enable_shared_from_this<_ResourcePool> {
	public:
		typedef std::shared_ptr<C> ValuePtr;
		_ResourcePool() {
			poolsize = poolSize;
			allotter = []()->C* {
				return new C();
			};
		}
#if (!defined(__GNUC__)) || (__GNUC__ >= 5)
		template<typename ...ArgTypes>
		_ResourcePool(ArgTypes &&...args) {
			poolsize = poolSize;
			allotter = [args...]()->C* {
				return new C(args...);
			};
		}
#endif //(!defined(__GNUC__)) || (__GNUC__ >= 5)
		virtual ~_ResourcePool(){
			std::lock_guard<mutex> lck(_mutex);
			for(auto &ptr : objs){
				delete ptr;
			}
		}
		void setSize(int size) {
			poolsize = size;
		}
		ValuePtr obtain() {
			std::lock_guard<mutex> lck(_mutex);
			C *ptr = nullptr;
			if (objs.size() == 0) {
				ptr = allotter();
			} else {
				ptr = objs.front();
				objs.pop_front();
			}
			return ValuePtr(ptr, Deleter(this->shared_from_this()));
		}
		void quit(const ValuePtr &ptr) {
			std::lock_guard<mutex> lck(_mutex);
			quitSet.emplace(ptr.get());
		}
	private:
		class Deleter {
		public:
			Deleter(const std::shared_ptr<_ResourcePool> &pool) {
				weakPool = pool;
			}
			void operator()(C *ptr) {
				auto strongPool = weakPool.lock();
				if (strongPool) {
					strongPool->recycle(ptr);
				} else {
					delete ptr;
				}
			}
		private:
			weak_ptr<_ResourcePool> weakPool;
		};
	private:
		void recycle(C *obj) {
			std::lock_guard<mutex> lck(_mutex);
			auto it = quitSet.find(obj);
			if (it != quitSet.end()) {
				delete obj;
				quitSet.erase(it);
				return;
			}
			if ((int)objs.size() >= poolsize) {
				delete obj;
				return;
			}
			objs.push_back(obj);
		}

		deque<C*> objs;
		unordered_set<C*> quitSet;
		function<C*(void)> allotter;
		mutex _mutex;
		int poolsize;
	};
	std::shared_ptr<_ResourcePool> pool;
};

} /* namespace util */
} /* namespace im */

#endif /* UTIL_RECYCLEPOOL_H_ */
