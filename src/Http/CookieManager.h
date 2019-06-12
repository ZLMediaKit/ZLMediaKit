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

#ifndef SRC_HTTP_COOKIEMANAGER_H
#define SRC_HTTP_COOKIEMANAGER_H

#include <memory>
#include <unordered_map>
#include "Util/mini.h"
#include "Util/TimeTicker.h"
#include "Network/Socket.h"
#include "Rtsp/Rtsp.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

#define COOKIE_DEFAULT_LIFE (7 * 24 * 60 * 60)

class CookieManager;

/**
 * cookie对象，用于保存cookie的一些相关属性
 */
class CookieData : public mINI , public noncopyable{
public:
    typedef std::shared_ptr<CookieData> Ptr;
    /**
     * 构建cookie
     * @param manager cookie管理者对象
     * @param cookie cookie随机字符串
     * @param uid 用户唯一id
     * @param max_elapsed 最大过期时间，单位秒
     * @param path http路径，譬如/index/files/
     */
    CookieData(const std::shared_ptr<CookieManager> &manager,const string &cookie,
               const string &uid,uint64_t max_elapsed,const string &path);
    ~CookieData() ;

    /**
     * 获取uid
     * @return uid
     */
    const string &getUid() const;

    /**
     * 获取http中Set-Cookie字段的值
     * @param cookie_name 该cookie的名称，譬如 MY_SESSION
     * @return 例如 MY_SESSION=XXXXXX;expires=Wed, Jun 12 2019 06:30:48 GMT;path=/index/files/
     */
    string getCookie(const string &cookie_name) const;

    /**
     * 获取cookie随机字符串
     * @return cookie随机字符串
     */
    const string& getCookie() const;

    /**
     * 获取该cookie对应的path
     * @return
     */
    const string& getPath() const;

    /**
     * 更新该cookie的过期时间，可以让此cookie不失效
     */
    void updateTime();

    /**
     * 判断该cookie是否过期
     * @return
     */
    bool isExpired();
private:
    string cookieExpireTime() const ;
private:
    string _uid;
    string _path;
    string _cookie_uuid;
    uint64_t _max_elapsed;
    Ticker _ticker;
    std::weak_ptr<CookieManager> _manager;
};

/**
 * cookie随机字符串生成器
 */
class CookieGeneator{
public:
    CookieGeneator() = default;
    ~CookieGeneator() = default;

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
class CookieManager : public std::enable_shared_from_this<CookieManager> {
public:
    typedef std::shared_ptr<CookieManager> Ptr;
    friend class CookieData;
    ~CookieManager();

    /**
     *  获取单例
     */
    static CookieManager &Instance();

    /**
     * 添加cookie
     * @param uid 用户id，如果为空则为匿名登录
     * @param max_client 该账号最多登录多少个设备
     * @param max_elapsed 该cookie过期时间，单位秒
     * @param path 该cookie对应的http路径
     * @return cookie对象
     */
    CookieData::Ptr addCookie(const string &uid, int max_client , uint64_t max_elapsed = COOKIE_DEFAULT_LIFE,const string &path = "/" );

    /**
     * 根据cookie随机字符串查找cookie对象
     * @param cookie cookie随机字符串
     * @param path 该cookie对应的http路径
     * @return cookie对象，可以为nullptr
     */
    CookieData::Ptr getCookie(const string &cookie,const string &path = "/");

    /**
     * 从http头中获取cookie对象
     * @param http_header http头
     * @param cookie_name cookie名
     * @param path http路径
     * @return cookie对象
     */
    CookieData::Ptr getCookie(const StrCaseMap &http_header,const string &cookie_name , const string &path = "/");

    /**
     * 删除cookie，用户登出时使用
     * @param cookie cookie对象，可以为nullptr
     * @return
     */
    bool delCookie(const CookieData::Ptr &cookie);


    /**
     * 获取某用户名下最先登录时的cookie，目的是实现某用户下最多登录若干个设备
     * @param path http路径
     * @param uid 用户id
     * @param max_client 最多登录的设备个数
     * @return 最早的cookie随机字符串
     */
    string getOldestCookie( const string &uid, int max_client ,const string &path = "/");
private:
    CookieManager();
    void onManager();
    /**
     * 构造cookie对象时触发，目的是记录某账号下多个cookie
     * @param path http路径
     * @param uid 用户id
     * @param cookie cookie随机字符串
     */
    void onAddCookie(const string &path,const string &uid,const string &cookie);

    /**
     * 析构cookie对象时触发
     * @param path http路径
     * @param uid 用户id
     * @param cookie cookie随机字符串
     */
    void onDelCookie(const string &path,const string &uid,const string &cookie);

    /**
     * 删除cookie
     * @param path http路径
     * @param cookie cookie随机字符串
     * @return 成功true
     */
    bool delCookie(const string &path,const string &cookie);
private:
    unordered_map<string/*path*/,unordered_map<string/*cookie*/,CookieData::Ptr/*cookie_data*/> >_map_cookie;
    unordered_map<string/*path*/,unordered_map<string/*uid*/,map<uint64_t/*cookie time stamp*/,string/*cookie*/> > >_map_uid_to_cookie;
    recursive_mutex _mtx_cookie;
    Timer::Ptr _timer;
    CookieGeneator _geneator;
};




#endif //SRC_HTTP_COOKIEMANAGER_H
