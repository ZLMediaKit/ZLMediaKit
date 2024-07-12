/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_STRCODING_H_
#define SRC_HTTP_STRCODING_H_

#include <iostream>
#include <string>
#include <cstdint>
namespace mediakit {

class strCoding {
public:
    static std::string UrlEncodePath(const std::string &str); //url路径 utf8编码
    static std::string UrlEncodeComponent(const std::string &str); // url参数 utf8编码
    static std::string UrlDecodePath(const std::string &str); //url路径 utf8解码
    static std::string UrlDecodeComponent(const std::string &str); // url参数 utf8解码
    static std::string UrlEncodeUserOrPass(const std::string &str); // url中用户名与密码编码
    static std::string UrlDecodeUserOrPass(const std::string &str); // url中用户名与密码解码
#if defined(_WIN32)
    static std::string UTF8ToGB2312(const std::string &str);//utf_8转为gb2312
    static std::string GB2312ToUTF8(const std::string &str); //gb2312 转utf_8
#endif//defined(_WIN32)
private:
    strCoding(void);
    virtual ~strCoding(void);
};

} /* namespace mediakit */

#endif /* SRC_HTTP_STRCODING_H_ */
