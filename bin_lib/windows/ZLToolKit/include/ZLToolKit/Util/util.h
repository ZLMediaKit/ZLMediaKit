/*
 * util.h
 *
 *  Created on: 2016年8月4日
 *      Author: xzl
 */

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_

#if defined(_WIN32)
#include <WinSock2.h>
#pragma comment (lib,"WS2_32")
#else
#include <unistd.h>
#include <sys/time.h>
#endif // defined(_WIN32)

#include <stdio.h>
#include <string.h>
#include <string>
#include <sstream>

using namespace std;

namespace ZL {
namespace Util {

class _StrPrinter {
public:
	_StrPrinter() {
	}
	template<typename T>
	_StrPrinter& operator <<(const T& data) {
		ss << data;
		return *this;
	}
	string operator <<(std::ostream&(*f)(std::ostream&)) const {
		return ss.str();
	}
private:
	stringstream ss;
};

#define StrPrinter _StrPrinter()

string makeRandStr(int sz, bool printable = true);
string hexdump(const void *buf, size_t len);
string exePath();
string exeDir();
string exeName();
void setExePath(const string &path);

#ifndef bzero
#define bzero(ptr,size)  memset((ptr),0,(size));
#endif //bzero

#if defined(ANDROID)
template <typename T>
std::string to_string(T value){
    std::ostringstream os ;
    os <<  std::forward<T>(value);
    return os.str() ;
}
#endif//ANDROID

#if defined(_WIN32)

int gettimeofday(struct timeval *tp, void *tzp);
int strcasecmp(const char *strA,const char *strB);
void usleep(int micro_seconds);
void sleep(int second);

#endif //WIN32

}  // namespace Util
}  // namespace ZL

#endif /* UTIL_UTIL_H_ */
