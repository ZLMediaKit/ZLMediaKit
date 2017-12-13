/*
 * NoticeCenter.h
 *
 *  Created on: 2017年2月17日
 *      Author: xzl
 */

#ifndef SRC_UTIL_NOTICECENTER_H_
#define SRC_UTIL_NOTICECENTER_H_

#include <mutex>
#include <memory>
#include <string>
#include <exception>
#include <functional>
#include <unordered_map>
#include "function_traits.h"

using namespace std;

namespace ZL {
namespace Util {


class NoticeCenter {
public:
	class InterruptException : public std::runtime_error
	{
	public:
		InterruptException():std::runtime_error("InterruptException"){}
		virtual ~InterruptException(){}
	};

	virtual ~NoticeCenter(){}
	static NoticeCenter &Instance(){
		static NoticeCenter instance;
		return instance;
	}
	template<typename ...ArgsType>
	bool emitEvent(const char *strEvent,ArgsType &&...args){
		lock_guard<recursive_mutex> lck(_mtxListener);
		auto it0 = _mapListener.find(strEvent);
		if (it0 == _mapListener.end()) {
			return false;
		}
		for(auto &pr : it0->second){
			typedef function<void(ArgsType &&...)> funType;
			funType *obj = (funType *)(pr.second.get());
			try{
				(*obj)(std::forward<ArgsType>(args)...);
			}catch(InterruptException &ex){
				break;
			}
		}
		return it0->second.size();
	}


	template<typename FUN>
	void addListener(void *tag, const char *strEvent, const FUN &fun) {
		typedef typename function_traits<FUN>::stl_function_type funType;
		std::shared_ptr<void> pListener(new funType(fun), [](void *ptr) {
			funType *obj = (funType *)ptr;
			delete obj;
		});
		lock_guard<recursive_mutex> lck(_mtxListener);
		_mapListener[strEvent][tag] = pListener;
	}


	void delListener(void *tag,const char *strEvent){
		lock_guard<recursive_mutex> lck(_mtxListener);
		auto it = _mapListener.find(strEvent);
		if(it == _mapListener.end()){
			return;
		}
		it->second.erase(tag);
		if(it->second.empty()){
			_mapListener.erase(it);
		}
	}
	void delListener(void *tag){
		lock_guard<recursive_mutex> lck(_mtxListener);
		for(auto it = _mapListener.begin();it != _mapListener.end();){
			it->second.erase(tag);
			if(it->second.empty()){
				it = _mapListener.erase(it);
				continue;
			}
			++it;
		}
	}

private:
	NoticeCenter(){}
	recursive_mutex _mtxListener;
	unordered_map<string,unordered_map<void *,std::shared_ptr<void> > > _mapListener;

};

} /* namespace Util */
} /* namespace ZL */

#endif /* SRC_UTIL_NOTICECENTER_H_ */
