/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpCookieManager.h"
#include "Common/config.h"
#include "Util/MD5.h"
#include "Util/util.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

//////////////////////////////HttpServerCookie////////////////////////////////////
HttpServerCookie::HttpServerCookie(
    const std::shared_ptr<HttpCookieManager> &manager, const string &cookie_name, const string &uid,
    const string &cookie, uint64_t max_elapsed) {
    _uid = uid;
    _max_elapsed = max_elapsed;
    _cookie_uuid = cookie;
    _cookie_name = cookie_name;
    _manager = manager;
    manager->onAddCookie(_cookie_name, _uid, _cookie_uuid);
}

HttpServerCookie::~HttpServerCookie() {
    auto strongManager = _manager.lock();
    if (strongManager) {
        strongManager->onDelCookie(_cookie_name, _uid, _cookie_uuid);
    }
}

const string &HttpServerCookie::getUid() const {
    return _uid;
}

string HttpServerCookie::getCookie(const string &path) const {
    return (StrPrinter << _cookie_name << "=" << _cookie_uuid << ";expires=" << cookieExpireTime() << ";path=" << path);
}

const string &HttpServerCookie::getCookie() const {
    return _cookie_uuid;
}

const string &HttpServerCookie::getCookieName() const {
    return _cookie_name;
}

void HttpServerCookie::updateTime() {
    _ticker.resetTime();
}

bool HttpServerCookie::isExpired() {
    return _ticker.elapsedTime() > _max_elapsed * 1000;
}

void HttpServerCookie::setAttach(toolkit::Any attach) {
    _attach = std::move(attach);
}

string HttpServerCookie::cookieExpireTime() const {
    char buf[64];
    time_t tt = time(nullptr) + _max_elapsed;
    strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}
//////////////////////////////CookieManager////////////////////////////////////
INSTANCE_IMP(HttpCookieManager);

HttpCookieManager::HttpCookieManager() {
    // 定时删除过期的cookie，防止内存膨胀  [AUTO-TRANSLATED:dd9dc9c0]
    // Delete expired cookies periodically to prevent memory bloat
    _timer = std::make_shared<Timer>(
        10.0f,
        [this]() {
            onManager();
            return true;
        },
        nullptr);
}

HttpCookieManager::~HttpCookieManager() {
    _timer.reset();
}

void HttpCookieManager::onManager() {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    // 先遍历所有类型  [AUTO-TRANSLATED:4917ee89]
    // First iterate through all types
    for (auto it_name = _map_cookie.begin(); it_name != _map_cookie.end();) {
        // 再遍历该类型下的所有cookie  [AUTO-TRANSLATED:0aab9e18]
        // Then iterate through all cookies under that type
        for (auto it_cookie = it_name->second.begin(); it_cookie != it_name->second.end();) {
            if (it_cookie->second->isExpired()) {
                // cookie过期,移除记录  [AUTO-TRANSLATED:8b48b8a2]
                // Cookie expired, remove record
                DebugL << it_cookie->second->getUid() << " cookie过期:" << it_cookie->second->getCookie();
                it_cookie = it_name->second.erase(it_cookie);
                continue;
            }
            ++it_cookie;
        }

        if (it_name->second.empty()) {
            // 该类型下没有任何cookie记录,移除之  [AUTO-TRANSLATED:92e3b783]
            // There are no cookie records under this type, remove it
            DebugL << "该path下没有任何cookie记录:" << it_name->first;
            it_name = _map_cookie.erase(it_name);
            continue;
        }
        ++it_name;
    }
}

HttpServerCookie::Ptr HttpCookieManager::addCookie(const string &cookie_name, const string &uid_in, uint64_t max_elapsed, toolkit::Any attach, int max_client) {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    auto cookie = _generator.obtain();
    auto uid = uid_in.empty() ? cookie : uid_in;
    auto oldCookie = getOldestCookie(cookie_name, uid, max_client);
    if (!oldCookie.empty()) {
        // 假如该账号已经登录了，那么删除老的cookie。  [AUTO-TRANSLATED:f18d826d]
        // If the account has already logged in, delete the old cookie.
        // 目的是实现单账号多地登录时挤占登录  [AUTO-TRANSLATED:8a64aec7]
        // The purpose is to achieve login squeeze when multiple devices log in with the same account
        delCookie(cookie_name, oldCookie);
    }
    HttpServerCookie::Ptr data(new HttpServerCookie(shared_from_this(), cookie_name, uid, cookie, max_elapsed));
    data->setAttach(std::move(attach));
    // 保存该账号下的新cookie  [AUTO-TRANSLATED:e476c9c8]
    // Save the new cookie under this account
    _map_cookie[cookie_name][cookie] = data;
    return data;
}

HttpServerCookie::Ptr HttpCookieManager::getCookie(const string &cookie_name, const string &cookie) {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    auto it_name = _map_cookie.find(cookie_name);
    if (it_name == _map_cookie.end()) {
        // 不存在该类型的cookie  [AUTO-TRANSLATED:d32b0997]
        // There is no cookie of this type
        return nullptr;
    }
    auto it_cookie = it_name->second.find(cookie);
    if (it_cookie == it_name->second.end()) {
        // 该类型下没有对应的cookie  [AUTO-TRANSLATED:62caa764]
        // There is no corresponding cookie under this type
        return nullptr;
    }
    if (it_cookie->second->isExpired()) {
        // cookie过期  [AUTO-TRANSLATED:a980453f]
        // Cookie expired
        DebugL << "cookie过期:" << it_cookie->second->getCookie();
        it_name->second.erase(it_cookie);
        return nullptr;
    }
    return it_cookie->second;
}

HttpServerCookie::Ptr HttpCookieManager::getCookie(const string &cookie_name, const StrCaseMap &http_header) {
    auto it = http_header.find("Cookie");
    if (it == http_header.end()) {
        return nullptr;
    }
    auto cookie = findSubString(it->second.data(), (cookie_name + "=").data(), ";");
    if (cookie.empty()) {
        cookie = findSubString(it->second.data(), (cookie_name + "=").data(), nullptr);
    }
    if (cookie.empty()) {
        return nullptr;
    }
    return getCookie(cookie_name, cookie);
}

HttpServerCookie::Ptr HttpCookieManager::getCookieByUid(const string &cookie_name, const string &uid) {
    if (cookie_name.empty() || uid.empty()) {
        return nullptr;
    }
    auto cookie = getOldestCookie(cookie_name, uid);
    if (cookie.empty()) {
        return nullptr;
    }
    return getCookie(cookie_name, cookie);
}

bool HttpCookieManager::delCookie(const HttpServerCookie::Ptr &cookie) {
    if (!cookie) {
        return false;
    }
    return delCookie(cookie->getCookieName(), cookie->getCookie());
}

bool HttpCookieManager::delCookie(const string &cookie_name, const string &cookie) {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    auto it_name = _map_cookie.find(cookie_name);
    if (it_name == _map_cookie.end()) {
        return false;
    }
    return it_name->second.erase(cookie);
}

void HttpCookieManager::onAddCookie(const string &cookie_name, const string &uid, const string &cookie) {
    // 添加新的cookie，我们记录下这个uid下有哪些cookie，目的是实现单账号多地登录时挤占登录  [AUTO-TRANSLATED:60b752e9]
    // Add a new cookie, we record which cookies are under this uid, the purpose is to achieve login squeeze when multiple devices log in with the same account
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    // 相同用户下可以存在多个cookie(意味多地登录)，这些cookie根据登录时间的早晚依次排序  [AUTO-TRANSLATED:1e0b93b9]
    // Multiple cookies can exist under the same user (meaning multiple devices log in), these cookies are sorted in order of login time
    _map_uid_to_cookie[cookie_name][uid][getCurrentMillisecond()] = cookie;
}

void HttpCookieManager::onDelCookie(const string &cookie_name, const string &uid, const string &cookie) {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    // 回收随机字符串  [AUTO-TRANSLATED:18a699ff]
    // Recycle random string
    _generator.release(cookie);

    auto it_name = _map_uid_to_cookie.find(cookie_name);
    if (it_name == _map_uid_to_cookie.end()) {
        // 该类型下未有任意用户登录  [AUTO-TRANSLATED:8ba458b9]
        // No user has logged in under this type
        return;
    }
    auto it_uid = it_name->second.find(uid);
    if (it_uid == it_name->second.end()) {
        // 该用户尚未登录  [AUTO-TRANSLATED:ec07ce1b]
        // This user has not logged in yet
        return;
    }

    // 遍历同一名用户下的所有客户端，移除命中的客户端  [AUTO-TRANSLATED:cae6e264]
    // Iterate through all clients under the same user and remove the matching client
    for (auto it_cookie = it_uid->second.begin(); it_cookie != it_uid->second.end(); ++it_cookie) {
        if (it_cookie->second != cookie) {
            // 不是该cookie  [AUTO-TRANSLATED:cf5eca3b]
            // Not this cookie
            continue;
        }
        // 移除该用户名下的某个cookie，这个设备cookie将失效  [AUTO-TRANSLATED:bf2de2a0]
        // Remove a cookie under this username, this device cookie will become invalid
        it_uid->second.erase(it_cookie);

        if (!it_uid->second.empty()) {
            break;
        }

        // 该用户名下没有任何设备在线，移除之  [AUTO-TRANSLATED:6a8a2305]
        // There are no devices online under this username, remove it
        it_name->second.erase(it_uid);

        if (!it_name->second.empty()) {
            break;
        }
        // 该类型下未有任何用户在线，移除之  [AUTO-TRANSLATED:e705cfe6]
        // There are no users online under this type, remove it
        _map_uid_to_cookie.erase(it_name);
        break;
    }
}

string HttpCookieManager::getOldestCookie(const string &cookie_name, const string &uid, int max_client) {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    auto it_name = _map_uid_to_cookie.find(cookie_name);
    if (it_name == _map_uid_to_cookie.end()) {
        // 不存在该类型的cookie  [AUTO-TRANSLATED:d32b0997]
        // There is no cookie of this type
        return "";
    }
    auto it_uid = it_name->second.find(uid);
    if (it_uid == it_name->second.end()) {
        // 该用户从未登录过  [AUTO-TRANSLATED:fc6dbcf6]
        // This user has never logged in
        return "";
    }
    if ((int)it_uid->second.size() < MAX(1, max_client)) {
        // 同一名用户下，客户端个数还没达到限制个数  [AUTO-TRANSLATED:a31f6ada]
        // Under the same user, the number of clients has not reached the limit
        return "";
    }
    // 客户端个数超过限制，移除最先登录的客户端  [AUTO-TRANSLATED:a284ce91]
    // The number of clients exceeds the limit, remove the first client to log in
    return it_uid->second.begin()->second;
}

/////////////////////////////////RandStrGenerator////////////////////////////////////
string RandStrGenerator::obtain() {
    // 获取唯一的防膨胀的随机字符串  [AUTO-TRANSLATED:1306465c]
    // Get a unique anti-bloating random string
    while (true) {
        auto str = obtain_l();
        if (_obtained.find(str) == _obtained.end()) {
            // 没有重复  [AUTO-TRANSLATED:16af311b]
            // No duplicates
            _obtained.emplace(str);
            return str;
        }
    }
}

void RandStrGenerator::release(const string &str) {
    // 从防膨胀库中移除  [AUTO-TRANSLATED:1165d5fe]
    // Remove from the anti-bloating library
    _obtained.erase(str);
}

string RandStrGenerator::obtain_l() {
    // 12个伪随机字节 + 4个递增的整形字节，然后md5即为随机字符串  [AUTO-TRANSLATED:8571a327]
    // 12 pseudo-random bytes + 4 incrementing integer bytes, then md5 is the random string
    auto str = makeRandStr(12, false);
    str.append((char *)&_index, sizeof(_index));
    ++_index;
    return MD5(str).hexdigest();
}

} // namespace mediakit