/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpCookie.h"
#include "Util/util.h"
#include "Util/logger.h"

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
static uint32_t timeStrToInt(const string &date){
    struct tm tt;
    strptime(date.data(),"%a, %b %d %Y %H:%M:%S %Z",&tt);
    return mktime(&tt);
}
void HttpCookie::setExpires(const string &expires,const string &server_date){
    _expire = timeStrToInt(expires);
    if(!server_date.empty()){
        _expire =  time(NULL) + (_expire - timeStrToInt(server_date));
//        DebugL <<  (timeStrToInt(expires) - timeStrToInt(server_date)) / 60;
    }
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
    _all_cookie[cookie->_host][cookie->_path][cookie->_key] = cookie;
}

vector<HttpCookie::Ptr> HttpCookieStorage::get(const string &host, const string &path) {
    vector<HttpCookie::Ptr> ret(0);
    lock_guard<mutex> lck(_mtx_cookie);
    auto it =  _all_cookie.find(host);
    if(it == _all_cookie.end()){
        //未找到该host相关记录
        return ret;
    }
    //遍历该host下所有path
    for(auto &pr : it->second){
        if(path.find(pr.first) != 0){
            //这个path不匹配
            continue;
        }
        //遍历该path下的各个cookie
        for(auto it_cookie = pr.second.begin() ; it_cookie != pr.second.end() ; ){
            if(!*(it_cookie->second)){
                //该cookie已经过期，移除之
                it_cookie = pr.second.erase(it_cookie);
                continue;
            }
            //保存有效cookie
            ret.emplace_back(it_cookie->second);
            ++it_cookie;
        }
    }
    return ret;
}


} /* namespace mediakit */
