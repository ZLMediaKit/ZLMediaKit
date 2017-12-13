/*
 * onceToken.h
 *
 *  Created on: 2016年8月10日
 *      Author: xzl
 */

#ifndef UTIL_ONCETOKEN_H_
#define UTIL_ONCETOKEN_H_

#include "functional"

using namespace std;

namespace ZL {
namespace Util {

class onceToken {
public:
	typedef function<void(void)> task;
	onceToken(const task &onConstructed, const task &_onDestructed) {
		if (onConstructed) {
			onConstructed();
		}
		onDestructed = _onDestructed;
	}
	virtual ~onceToken() {
		if (onDestructed) {
			onDestructed();
		}
	}
private:
	onceToken();
	onceToken(const onceToken &);
	onceToken(onceToken &&);
	onceToken &operator =(const onceToken &);
	onceToken &operator =(onceToken &&);
	task onDestructed;
};

} /* namespace Util */
} /* namespace ZL */

#endif /* UTIL_ONCETOKEN_H_ */
