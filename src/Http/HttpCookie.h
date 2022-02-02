/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_HTTPCOOKIE_H
#define ZLMEDIAKIT_HTTPCOOKIE_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>

namespace mediakit {

/**
 * http客户端cookie对象
 */
class HttpCookie {
public:
    typedef std::shared_ptr<HttpCookie> Ptr;
    friend class HttpCookieStorage;
    HttpCookie(){}
    ~HttpCookie(){}

    void setPath(const std::string &path);
    void setHost(const std::string &host);
    void setExpires(const std::string &expires,const std::string &server_date);
    void setKeyVal(const std::string &key,const std::string &val);
    operator bool ();

    const std::string &getKey() const ;
    const std::string &getVal() const ;
private:
    std::string _host;
    std::string _path = "/";
    std::string _key;
    std::string _val;
    time_t _expire = 0;
};


/**
 * http客户端cookie全局保存器
 */
class HttpCookieStorage{
public:
    ~HttpCookieStorage(){}
    static HttpCookieStorage &Instance();
    void set(const HttpCookie::Ptr &cookie);
    std::vector<HttpCookie::Ptr> get(const std::string &host,const std::string &path);
private:
    HttpCookieStorage(){};
private:
    std::unordered_map<std::string/*host*/, std::map<std::string/*cookie path*/,std::map<std::string/*cookie_key*/, HttpCookie::Ptr> > > _all_cookie;
    std::mutex _mtx_cookie;
};


} /* namespace mediakit */

#endif //ZLMEDIAKIT_HTTPCOOKIE_H
