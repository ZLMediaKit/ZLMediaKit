/*
 * Session.h
 *
 *  Created on: 2015年10月27日
 *      Author: root
 */

#ifndef SERVER_LIMITEDSESSION_H_
#define SERVER_LIMITEDSESSION_H_
#include <memory>
#include "Util/logger.h"
#include "TcpSession.h"

using namespace std;
using namespace ZL::Util;

namespace ZL {
namespace Network {

template<int MaxCount>
class TcpLimitedSession: public TcpSession {
public:
	TcpLimitedSession(const std::shared_ptr<ThreadPool> &_th, const Socket::Ptr &_sock) :
		TcpSession(_th,_sock) {
		lock_guard<recursive_mutex> lck(stackMutex());
		static uint64_t maxSeq(0);
		sessionSeq = maxSeq++;
		auto &stack = getStack();
		stack.emplace(this);

		if(stack.size() > MaxCount){
			auto it = stack.begin();
			(*it)->safeShutdown();
			stack.erase(it);
			WarnL << "超过TCP个数限制:" << MaxCount;
		}
	}
	virtual ~TcpLimitedSession() {
		lock_guard<recursive_mutex> lck(stackMutex());
		getStack().erase(this);
	}
private:
	uint64_t sessionSeq; //会话栈顺序
	struct Comparer {
		bool operator()(TcpLimitedSession *x, TcpLimitedSession *y) const {
			return x->sessionSeq < y->sessionSeq;
		}
	};
	static recursive_mutex &stackMutex(){
		static recursive_mutex mtx;
		return mtx;
	}
	//RTSP会话栈,先创建的在前面
	static set<TcpLimitedSession *, Comparer> &getStack(){
		static set<TcpLimitedSession *, Comparer> stack;
		return stack;
	}
};


} /* namespace Session */
} /* namespace ZL */

#endif /* SERVER_LIMITEDSESSION_H_ */
