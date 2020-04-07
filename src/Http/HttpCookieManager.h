/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_COOKIEMANAGER_H
#define SRC_HTTP_COOKIEMANAGER_H

#include <memory>
#include <unordered_map>
#include "Util/mini.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "Network/Socket.h"
#include "Common/Parser.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

#define COOKIE_DEFAULT_LIFE (7 * 24 * 60 * 60)

namespace mediakit {

class HttpCookieManager;

/**
 * cookie对象，用于保存cookie的一些相关属性
 */
class HttpServerCookie : public AnyStorage , public noncopyable{
public:
    typedef std::shared_ptr<HttpServerCookie> Ptr;
    /**
     * 构建cookie
     * @param manager cookie管理者对象
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户唯一id
     * @param cookie cookie随机字符串
     * @param max_elapsed 最大过期时间，单位秒
     */

    HttpServerCookie(const std::shared_ptr<HttpCookieManager> &manager,
                     const string &cookie_name,
                     const string &uid,
                     const string &cookie,
                     uint64_t max_elapsed);
    ~HttpServerCookie() ;

    /**
     * 获取uid
     * @return uid
     */
    const string &getUid() const;

    /**
     * 获取http中Set-Cookie字段的值
     * @param cookie_name 该cookie的名称，譬如 MY_SESSION
     * @param path http访问路径
     * @return 例如 MY_SESSION=XXXXXX;expires=Wed, Jun 12 2019 06:30:48 GMT;path=/index/files/
     */
    string getCookie(const string &path) const;

    /**
     * 获取cookie随机字符串
     * @return cookie随机字符串
     */
    const string& getCookie() const;

    /**
     * 获取该cookie名
     * @return
     */
    const string& getCookieName() const;

    /**
     * 更新该cookie的过期时间，可以让此cookie不失效
     */
    void updateTime();

    /**
     * 判断该cookie是否过期
     * @return
     */
    bool isExpired();

    /**
     * 获取区域锁
     * @return
     */
    std::shared_ptr<lock_guard<recursive_mutex> > getLock();
private:
    string cookieExpireTime() const ;
private:
    string _uid;
    string _cookie_name;
    string _cookie_uuid;
    uint64_t _max_elapsed;
    Ticker _ticker;
    recursive_mutex _mtx;
    std::weak_ptr<HttpCookieManager> _manager;
};

/**
 * cookie随机字符串生成器
 */
class RandStrGeneator{
public:
    RandStrGeneator() = default;
    ~RandStrGeneator() = default;

    /**
     * 获取不碰撞的随机字符串
     * @return 随机字符串
     */
    string obtain();

    /**
     * 释放随机字符串
     * @param str 随机字符串
     */
    void release(const string &str);
private:
    string obtain_l();
private:
    //碰撞库
    unordered_set<string> _obtained;
    //增长index，防止碰撞用
    int _index = 0;
};

/**
 * cookie管理器，用于管理cookie的生成以及过期管理，同时实现了同账号异地挤占登录功能
 * 该对象实现了同账号最多登录若干个设备
 */
class HttpCookieManager : public std::enable_shared_from_this<HttpCookieManager> {
public:
    typedef std::shared_ptr<HttpCookieManager> Ptr;
    friend class HttpServerCookie;
    ~HttpCookieManager();

    /**
     *  获取单例
     */
    static HttpCookieManager &Instance();

    /**
     * 添加cookie
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户id，如果为空则为匿名登录
     * @param max_client 该账号最多登录多少个设备
     * @param max_elapsed 该cookie过期时间，单位秒
     * @return cookie对象
     */
    HttpServerCookie::Ptr addCookie(const string &cookie_name,const string &uid, uint64_t max_elapsed = COOKIE_DEFAULT_LIFE,int max_client = 1);

    /**
     * 根据cookie随机字符串查找cookie对象
     * @param cookie_name cookie名，例如MY_SESSION
     * @param cookie cookie随机字符串
     * @return cookie对象，可以为nullptr
     */
    HttpServerCookie::Ptr getCookie(const string &cookie_name,const string &cookie);

    /**
     * 从http头中获取cookie对象
     * @param cookie_name cookie名，例如MY_SESSION
     * @param http_header http头
     * @return cookie对象
     */
    HttpServerCookie::Ptr getCookie(const string &cookie_name,const StrCaseMap &http_header);

    /**
     * 根据uid获取cookie
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户id
     * @return cookie对象
     */
    HttpServerCookie::Ptr getCookieByUid(const string &cookie_name,const string &uid);

    /**
     * 删除cookie，用户登出时使用
     * @param cookie cookie对象，可以为nullptr
     * @return
     */
    bool delCookie(const HttpServerCookie::Ptr &cookie);
private:
    HttpCookieManager();
    void onManager();
    /**
     * 构造cookie对象时触发，目的是记录某账号下多个cookie
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户id
     * @param cookie cookie随机字符串
     */
    void onAddCookie(const string &cookie_name,const string &uid,const string &cookie);

    /**
     * 析构cookie对象时触发
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户id
     * @param cookie cookie随机字符串
     */
    void onDelCookie(const string &cookie_name,const string &uid,const string &cookie);

    /**
     * 获取某用户名下最先登录时的cookie，目的是实现某用户下最多登录若干个设备
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户id
     * @param max_client 最多登录的设备个数
     * @return 最早的cookie随机字符串
     */
    string getOldestCookie(const string &cookie_name,const string &uid, int max_client = 1);

    /**
     * 删除cookie
     * @param cookie_name cookie名，例如MY_SESSION
     * @param cookie cookie随机字符串
     * @return 成功true
     */
    bool delCookie(const string &cookie_name,const string &cookie);
private:
    unordered_map<string/*cookie_name*/,unordered_map<string/*cookie*/,HttpServerCookie::Ptr/*cookie_data*/> >_map_cookie;
    unordered_map<string/*cookie_name*/,unordered_map<string/*uid*/,map<uint64_t/*cookie time stamp*/,string/*cookie*/> > >_map_uid_to_cookie;
    recursive_mutex _mtx_cookie;
    Timer::Ptr _timer;
    RandStrGeneator _geneator;
};

}//namespace mediakit


#endif //SRC_HTTP_COOKIEMANAGER_H
