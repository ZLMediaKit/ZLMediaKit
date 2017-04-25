/*
 * strCoding.h
 *
 *  Created on: 2016年9月22日
 *      Author: xzl
 */

#ifndef SRC_HTTP_STRCODING_H_
#define SRC_HTTP_STRCODING_H_

#include <iostream>
#include <string>

using namespace std;

namespace ZL {
namespace Http {

class strCoding {
public:
	static string UrlUTF8Encode(const char * str); //urlutf8 编码
	static string UrlUTF8Decode(const string &str); //urlutf8解码
private:
	strCoding(void);
	virtual ~strCoding(void);
	static inline char CharToInt(char ch);
	static inline char StrToBin(const char *str);
};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_STRCODING_H_ */
