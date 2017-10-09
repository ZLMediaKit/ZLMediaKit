/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
	static string UrlUTF8Encode(const string &str); //urlutf8 编码
	static string UrlUTF8Decode(const string &str); //urlutf8解码

	static string UrlGB2312Encode(const string &str); //urlgb2312编码 
	static string UrlGB2312Decode(const string &str); //urlgb2312解码 
#if defined(_WIN32)
	static string UTF8ToGB2312(const string &str);//utf_8转为gb2312 
	static string GB2312ToUTF8(const string &str); //gb2312 转utf_8 
#endif//defined(_WIN32)
private:
	strCoding(void);
	virtual ~strCoding(void);
};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_STRCODING_H_ */
