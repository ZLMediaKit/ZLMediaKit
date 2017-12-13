/*
 * WinSelect.h
 *
 *  Created on: 2017年3月2日
 *      Author: Jzan
 */

#ifndef SRC_POLLER_SELECTWRAP_H_
#define SRC_POLLER_SELECTWRAP_H_

namespace ZL {
namespace Poller {

class FdSet
{
public:
	FdSet();
	virtual ~FdSet();
	void fdZero();
	void fdSet(int fd);
	void fdClr(int fd);
	bool isSet(int fd);
	void *ptr;
private:
};

} /* namespace Poller */
} /* namespace ZL */
using namespace ZL::Poller;
int zl_select(int cnt,FdSet *read,FdSet *write,FdSet *err,struct timeval *tv);



#endif /* SRC_POLLER_SELECTWRAP_H_ */
