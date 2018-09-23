//
// Created by xzl on 2018/9/23.
//

#include "HttpCookie.h"
#include "Util/util.h"
using namespace ZL::Util;

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
