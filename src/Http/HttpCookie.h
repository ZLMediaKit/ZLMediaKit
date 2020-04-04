/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
using namespace std;

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

    void setPath(const string &path);
    void setHost(const string &host);
    void setExpires(const string &expires,const string &server_date);
    void setKeyVal(const string &key,const string &val);
    operator bool ();

    const string &getKey() const ;
    const string &getVal() const ;
private:
    string _host;
    string _path = "/";
    uint32_t _expire = 0;
    string _key;
    string _val;
};


/**
 * http客户端cookie全局保存器
 */
class HttpCookieStorage{
public:
    ~HttpCookieStorage(){}
    static HttpCookieStorage &Instance();
    void set(const HttpCookie::Ptr &cookie);
    vector<HttpCookie::Ptr> get(const string &host,const string &path);
private:
    HttpCookieStorage(){};
private:
    unordered_map<string/*host*/,map<string/*cookie path*/,map<string/*cookie_key*/,HttpCookie::Ptr> > > _all_cookie;
    mutex _mtx_cookie;
};


} /* namespace mediakit */

#endif //ZLMEDIAKIT_HTTPCOOKIE_H
