/*
 * MediaSender.h
 *
 *  Created on: 2016年9月1日
 *      Author: xzl
 */

#ifndef SRC_MEDIASENDER_H_
#define SRC_MEDIASENDER_H_
#include "Thread/ThreadPool.hpp"
using namespace ZL::Thread;

class MediaSender {
public:
	static ThreadPool & sendThread() {
		static ThreadPool pool(1);
		return pool;
	}
private:
	MediaSender();
	virtual ~MediaSender();
};

#endif /* SRC_MEDIASENDER_H_ */
