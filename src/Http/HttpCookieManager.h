/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_COOKIEMANAGER_H
#define SRC_HTTP_COOKIEMANAGER_H

#include "Common/Parser.h"
#include "Network/Socket.h"
#include "Util/TimeTicker.h"
#include "Util/mini.h"
#include "Util/util.h"
#include <memory>
#include <unordered_map>

#define COOKIE_DEFAULT_LIFE (7 * 24 * 60 * 60)

namespace mediakit {

class HttpCookieManager;

/**
 * cookie对象，用于保存cookie的一些相关属性
 * cookie object, used to store some related attributes of the cookie
 
 * [AUTO-TRANSLATED:267fbbc3]
 */
class HttpServerCookie : public toolkit::noncopyable {
public:
    using Ptr = std::shared_ptr<HttpServerCookie>;
    /**
     * 构建cookie
     * @param manager cookie管理者对象
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户唯一id
     * @param cookie cookie随机字符串
     * @param max_elapsed 最大过期时间，单位秒
     * Construct cookie
     * @param manager cookie manager object
     * @param cookie_name cookie name, such as MY_SESSION
     * @param uid user unique id
     * @param cookie cookie random string
     * @param max_elapsed maximum expiration time, in seconds
     
     * [AUTO-TRANSLATED:a24f209d]
     */

    HttpServerCookie(
        const std::shared_ptr<HttpCookieManager> &manager, const std::string &cookie_name, const std::string &uid,
        const std::string &cookie, uint64_t max_elapsed);
    ~HttpServerCookie();

    /**
     * 获取uid
     * @return uid
     * Get uid
     * @return uid
     
     * [AUTO-TRANSLATED:71a3afab]
     */
    const std::string &getUid() const;

    /**
     * 获取http中Set-Cookie字段的值
     * @param cookie_name 该cookie的名称，譬如 MY_SESSION
     * @param path http访问路径
     * @return 例如 MY_SESSION=XXXXXX;expires=Wed, Jun 12 2019 06:30:48 GMT;path=/index/files/
     * Get the value of the Set-Cookie field in http
     * @param cookie_name the name of this cookie, such as MY_SESSION
     * @param path http access path
     * @return For example, MY_SESSION=XXXXXX;expires=Wed, Jun 12 2019 06:30:48 GMT;path=/index/files/
     
     * [AUTO-TRANSLATED:8699036b]
     */
    std::string getCookie(const std::string &path) const;

    /**
     * 获取cookie随机字符串
     * @return cookie随机字符串
     * Get cookie random string
     * @return cookie random string
     
     * [AUTO-TRANSLATED:1853611a]
     */
    const std::string &getCookie() const;

    /**
     * 获取该cookie名
     * @return
     * Get the name of this cookie
     * @return
     
     * [AUTO-TRANSLATED:6251f9f5]
     */
    const std::string &getCookieName() const;

    /**
     * 更新该cookie的过期时间，可以让此cookie不失效
     * Update the expiration time of this cookie, so that this cookie will not expire
     
     * [AUTO-TRANSLATED:d3a3300b]
     */
    void updateTime();

    /**
     * 判断该cookie是否过期
     * @return
     * Determine whether this cookie has expired
     * @return
     
     * [AUTO-TRANSLATED:3b0d3d59]
     */
    bool isExpired();

    /**
     * 设置附加数据
     * Set additional data
     
     * [AUTO-TRANSLATED:afde9874]
     */
    void setAttach(toolkit::Any attach);

    /*
     * 获取附加数据
     /*
     * Get additional data
     
     * [AUTO-TRANSLATED:e277d75d]
     */
    template <class T>
    T& getAttach() {
        return _attach.get<T>();
    }

private:
    std::string cookieExpireTime() const;

private:
    std::string _uid;
    std::string _cookie_name;
    std::string _cookie_uuid;
    uint64_t _max_elapsed;
    toolkit::Ticker _ticker;
    toolkit::Any _attach;
    std::weak_ptr<HttpCookieManager> _manager;
};

/**
 * cookie随机字符串生成器
 * cookie random string generator
 
 * [AUTO-TRANSLATED:501ea34c]
 */
class RandStrGenerator {
public:

    /**
     * 获取不碰撞的随机字符串
     * @return 随机字符串
     * Get a random string that does not collide
     * @return random string
     
     * [AUTO-TRANSLATED:6daa3fd8]
     */
    std::string obtain();

    /**
     * 释放随机字符串
     * @param str 随机字符串
     * Release random string
     * @param str random string
     
     * [AUTO-TRANSLATED:90ea164a]
     */
    void release(const std::string &str);

private:
    std::string obtain_l();

private:
    // 碰撞库  [AUTO-TRANSLATED:25a2ca2b]
    // Collision library
    std::unordered_set<std::string> _obtained;
    // 增长index，防止碰撞用  [AUTO-TRANSLATED:85778468]
    // Increase index, used to prevent collisions
    int _index = 0;
};

/**
 * cookie管理器，用于管理cookie的生成以及过期管理，同时实现了同账号异地挤占登录功能
 * 该对象实现了同账号最多登录若干个设备
 * Cookie manager, used to manage cookie generation and expiration management, and also implements the function of occupying login from different locations with the same account
 * This object implements the function that the same account can log in to at most several devices
 
 * [AUTO-TRANSLATED:ad6008e8]
 */
class HttpCookieManager : public std::enable_shared_from_this<HttpCookieManager> {
public:
    friend class HttpServerCookie;
    using Ptr =  std::shared_ptr<HttpCookieManager>;
    ~HttpCookieManager();

    /**
     *  获取单例
     *  Get singleton
     
     * [AUTO-TRANSLATED:d082a6ee]
     */
    static HttpCookieManager &Instance();

    /**
     * 添加cookie
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户id，如果为空则为匿名登录
     * @param max_client 该账号最多登录多少个设备
     * @param max_elapsed 该cookie过期时间，单位秒
     * @return cookie对象
     * Add cookie
     * @param cookie_name cookie name, such as MY_SESSION
     * @param uid user id, if empty, it is anonymous login
     * @param max_client the maximum number of devices that this account can log in to
     * @param max_elapsed the expiration time of this cookie, in seconds
     * @return cookie object
     
     * [AUTO-TRANSLATED:c23f2321]
     */
    HttpServerCookie::Ptr addCookie(
        const std::string &cookie_name, const std::string &uid, uint64_t max_elapsed = COOKIE_DEFAULT_LIFE,
        toolkit::Any = toolkit::Any{},
        int max_client = 1);

    /**
     * 根据cookie随机字符串查找cookie对象
     * @param cookie_name cookie名，例如MY_SESSION
     * @param cookie cookie随机字符串
     * @return cookie对象，可以为nullptr
     * Find cookie object by cookie random string
     * @param cookie_name cookie name, such as MY_SESSION
     * @param cookie cookie random string
     * @return cookie object, can be nullptr
     
     * [AUTO-TRANSLATED:a0c7ed63]
     */
    HttpServerCookie::Ptr getCookie(const std::string &cookie_name, const std::string &cookie);

    /**
     * 从http头中获取cookie对象
     * @param cookie_name cookie名，例如MY_SESSION
     * @param http_header http头
     * @return cookie对象
     * Get cookie object from http header
     * @param cookie_name cookie name, such as MY_SESSION
     * @param http_header http header
     * @return cookie object
     
     * [AUTO-TRANSLATED:93661474]
     */
    HttpServerCookie::Ptr getCookie(const std::string &cookie_name, const StrCaseMap &http_header);

    /**
     * 根据uid获取cookie
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户id
     * @return cookie对象
     * Get cookie by uid
     * @param cookie_name cookie name, such as MY_SESSION
     * @param uid user id
     * @return cookie object
     
     * [AUTO-TRANSLATED:623277e4]
     */
    HttpServerCookie::Ptr getCookieByUid(const std::string &cookie_name, const std::string &uid);

    /**
     * 删除cookie，用户登出时使用
     * @param cookie cookie对象，可以为nullptr
     * @return
     * Delete cookie, used when user logs out
     * @param cookie cookie object, can be nullptr
     * @return
     
     * [AUTO-TRANSLATED:f80c6974]
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
     * Triggered when constructing a cookie object, the purpose is to record multiple cookies under a certain account
     * @param cookie_name cookie name, such as MY_SESSION
     * @param uid user id
     * @param cookie cookie random string
     
     * [AUTO-TRANSLATED:bb2bb670]
     */
    void onAddCookie(const std::string &cookie_name, const std::string &uid, const std::string &cookie);

    /**
     * 析构cookie对象时触发
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户id
     * @param cookie cookie随机字符串
     * Triggered when destructing a cookie object
     * @param cookie_name cookie name, such as MY_SESSION
     * @param uid user id
     * @param cookie cookie random string
     
     * [AUTO-TRANSLATED:bdf9cce5]
     */
    void onDelCookie(const std::string &cookie_name, const std::string &uid, const std::string &cookie);

    /**
     * 获取某用户名下最先登录时的cookie，目的是实现某用户下最多登录若干个设备
     * @param cookie_name cookie名，例如MY_SESSION
     * @param uid 用户id
     * @param max_client 最多登录的设备个数
     * @return 最早的cookie随机字符串
     * Get the cookie that logged in first under a certain username, the purpose is to implement the function that at most several devices can log in under a certain user
     * @param cookie_name cookie name, such as MY_SESSION
     * @param uid user id
     * @param max_client the maximum number of devices that can log in
     * @return the earliest cookie random string
     
     * [AUTO-TRANSLATED:431b0732]
     */
    std::string getOldestCookie(const std::string &cookie_name, const std::string &uid, int max_client = 1);

    /**
     * 删除cookie
     * @param cookie_name cookie名，例如MY_SESSION
     * @param cookie cookie随机字符串
     * @return 成功true
     * Delete cookie
     * @param cookie_name cookie name, such as MY_SESSION
     * @param cookie cookie random string
     * @return success true

     * [AUTO-TRANSLATED:09fa1e44]
     */
    bool delCookie(const std::string &cookie_name, const std::string &cookie);

private:
    std::unordered_map<
        std::string /*cookie_name*/, std::unordered_map<std::string /*cookie*/, HttpServerCookie::Ptr /*cookie_data*/>>
        _map_cookie;
    std::unordered_map<
        std::string /*cookie_name*/,
        std::unordered_map<std::string /*uid*/, std::map<uint64_t /*cookie time stamp*/, std::string /*cookie*/>>>
        _map_uid_to_cookie;
    std::recursive_mutex _mtx_cookie;
    toolkit::Timer::Ptr _timer;
    RandStrGenerator _generator;
};

} // namespace mediakit

#endif // SRC_HTTP_COOKIEMANAGER_H
