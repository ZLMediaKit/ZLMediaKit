//
// Created by xzl on 2018/9/23.
//

#ifndef ZLMEDIAKIT_HTTPCOOKIE_H
#define ZLMEDIAKIT_HTTPCOOKIE_H

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
using namespace std;

class HttpCookie {
public:
    typedef std::shared_ptr<HttpCookie> Ptr;
    friend class HttpCookieStorage;
    HttpCookie(){}
    ~HttpCookie(){}

    void setPath(const string &path);
    void setHost(const string &host);
    void setExpires(const string &expires);
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


class HttpCookieStorage{
public:
    ~HttpCookieStorage(){}
    static HttpCookieStorage &Instance();
    void set(const HttpCookie::Ptr &cookie);
    vector<HttpCookie::Ptr> get(const string &host,const string &path);
private:
    HttpCookieStorage(){};
private:
    unordered_map<string,unordered_map<string,HttpCookie::Ptr> > _all_cookie;
    mutex _mtx_cookie;
};

#endif //ZLMEDIAKIT_HTTPCOOKIE_H
