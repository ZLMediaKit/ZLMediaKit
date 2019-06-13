/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
