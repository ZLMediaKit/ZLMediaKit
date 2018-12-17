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

#include "HttpCookie.h"
#include "Util/util.h"

#if defined(_WIN32)
#include "strptime_win.h"
#endif

using namespace toolkit;
namespace mediakit {

void HttpCookie::setPath(const string &path){
    _path = path;
}
void HttpCookie::setHost(const string &host){
    _host = host;
}
void HttpCookie::setExpires(const string &expires){
    struct tm tt;
    strptime(expires.data(),"%a, %b %d %Y %H:%M:%S %Z",&tt);
    _expire = mktime(&tt);
}
void HttpCookie::setKeyVal(const string &key,const string &val){
    _key = key;
    _val = val;
}
HttpCookie::operator bool (){
    return !_host.empty() && !_key.empty() && !_val.empty() && (_expire > time(NULL));
}

const string &HttpCookie::getVal() const {
    return _val;
}

const string &HttpCookie::getKey() const{
    return _key;
}


HttpCookieStorage &HttpCookieStorage::Instance(){
    static HttpCookieStorage instance;
    return instance;
}

void HttpCookieStorage::set(const HttpCookie::Ptr &cookie) {
    lock_guard<mutex> lck(_mtx_cookie);
    if(!cookie || !(*cookie)){
        return;
    }
    _all_cookie[cookie->_host][cookie->_path] = cookie;
}

vector<HttpCookie::Ptr> HttpCookieStorage::get(const string &host, const string &path) {
    vector<HttpCookie::Ptr> ret(0);
    lock_guard<mutex> lck(_mtx_cookie);
    auto it =  _all_cookie.find(host);
    if(it == _all_cookie.end()){
        return ret;
    }
    auto &path_cookie = it->second;

    auto lam = [&](const string &sub_path){
        auto it_cookie = path_cookie.find(sub_path);
        if(it_cookie != path_cookie.end()){
            if(*(it_cookie->second)){
                ret.emplace_back(it_cookie->second);
            }else{
                path_cookie.erase(it_cookie);
            }
        }
    };


    int pos = 0;
    do{
        auto sub_path = path.substr(0,pos + 1);
        lam(sub_path);
        pos = path.find('/',1 + pos);
    }while (pos != string::npos);
    lam(path);
    return ret;
}


} /* namespace mediakit */
