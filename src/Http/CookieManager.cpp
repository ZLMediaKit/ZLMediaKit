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

#include "Util/util.h"
#include "Util/MD5.h"
#include "Common/config.h"
#include "CookieManager.h"

//////////////////////////////CookieData////////////////////////////////////
CookieData::CookieData(const CookieManager::Ptr &manager,
                       const string &cookie,
                       const string &uid,
                       uint64_t max_elapsed,
                       const string &path){
    _uid = uid;
    _max_elapsed = max_elapsed;
    _cookie_uuid = cookie;
    _path = path;
    _manager = manager;
    manager->onAddCookie(path,uid,cookie);
}

CookieData::~CookieData() {
    auto strongManager = _manager.lock();
    if(strongManager){
        strongManager->onDelCookie(_path,_uid,_cookie_uuid);
    }
}
string CookieData::getCookie(const string &cookie_name) const {
    return (StrPrinter << cookie_name << "=" << _cookie_uuid << ";expires=" << cookieExpireTime() << ";path=" << _path);
}

bool CookieData::isExpired() {
    return _ticker.elapsedTime() > _max_elapsed * 1000;
}

void CookieData::updateTime() {
    _ticker.resetTime();
}
const string & CookieData::getUid() const{
    return _uid;
}
string CookieData::cookieExpireTime() const{
    char buf[64];
    time_t tt = time(NULL) + _max_elapsed;
    strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

const string& CookieData::getCookie() const {
    return _cookie_uuid;
}

const string& CookieData::getPath() const{
    return _path;
}

//////////////////////////////CookieManager////////////////////////////////////
INSTANCE_IMP(CookieManager);

CookieData::Ptr CookieManager::addCookie(const string &uidIn, int max_client ,uint64_t max_elapsed, const string &path) {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    auto cookie = _geneator.obtain();
    auto uid = uidIn.empty() ? cookie : uidIn;
    auto oldCookie = getOldestCookie(uid, max_client , path);
    if(!oldCookie.empty()){
        //假如该账号已经登录了，那么删除老的cookie。
        //目的是实现单账号多地登录时挤占登录
        delCookie(oldCookie,path);
    }
    CookieData::Ptr data(new CookieData(shared_from_this(),cookie,uid,max_elapsed,path));
    //保存该账号下的新cookie
    _map_cookie[path][cookie] = data;
    return data;
}

bool CookieManager::delCookie(const CookieData::Ptr &cookie) {
    if(!cookie){
        return false;
    }
    return delCookie(cookie->getPath(),cookie->getCookie());
}

bool CookieManager::delCookie(const string &path , const string &cookie) {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    auto it = _map_cookie.find(path);
    if(it == _map_cookie.end()){
        return false;
    }
    return it->second.erase(cookie);
}

CookieData::Ptr CookieManager::getCookie(const string &cookie, const string &path) {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    auto it_path = _map_cookie.find(path);
    if(it_path == _map_cookie.end()){
        //该path下没有任何cookie
        return nullptr;
    }
    auto it_cookie = it_path->second.find(cookie);
    if(it_cookie == it_path->second.end()){
        //该path下没有对应的cookie
        return nullptr;
    }
    if(it_cookie->second->isExpired()){
        //cookie过期
        it_path->second.erase(it_cookie);
        return nullptr;
    }
    return it_cookie->second;
}

CookieData::Ptr CookieManager::getCookie(const StrCaseMap &http_header, const string &cookie_name, const string &path) {
    auto it = http_header.find("Cookie");
    if (it == http_header.end()) {
        return nullptr;
    }
    auto cookie = FindField(it->second.data(), (cookie_name + "=").data(), ";");
    if (!cookie.size()) {
        cookie = FindField(it->second.data(), (cookie_name + "=").data(), nullptr);
    }
    if(cookie.empty()){
        return nullptr;
    }
    return CookieManager::Instance().getCookie(cookie, path);
}

CookieManager::CookieManager() {
    //定时删除过期的cookie，防止内存膨胀
    _timer = std::make_shared<Timer>(10,[this](){
        onManager();
        return true;
    }, nullptr);
}

CookieManager::~CookieManager() {
    _timer.reset();
}

void CookieManager::onManager() {
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    //先遍历所有path
    for(auto it_path = _map_cookie.begin() ; it_path != _map_cookie.end() ;){
        //再遍历该path下的所有cookie
        for (auto it_cookie = it_path->second.begin() ; it_cookie != it_path->second.end() ; ){
            if(it_cookie->second->isExpired()){
                //cookie过期,移除记录
                WarnL << it_cookie->second->getUid() << " cookie过期";
                it_cookie = it_path->second.erase(it_cookie);
                continue;
            }
            ++it_cookie;
        }

        if(it_path->second.empty()){
            //该path下没有任何cooki记录,移除之
            WarnL << "该path下没有任何cooki记录:" << it_path->first;
            it_path = _map_cookie.erase(it_path);
            continue;
        }
        ++it_path;
    }
}

void CookieManager::onAddCookie(const string &path,const string &uid,const string &cookie){
    //添加新的cookie，我们记录下这个uid下有哪些cookie，目的是实现单账号多地登录时挤占登录
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    //相同用户下可以存在多个cookie(意味多地登录)，这些cookie根据登录时间的早晚依次排序
    _map_uid_to_cookie[path][uid][getCurrentMillisecond()] = cookie;
}
void CookieManager::onDelCookie(const string &path,const string &uid,const string &cookie){
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    //回收随机字符串
    _geneator.release(cookie);

    auto it_path = _map_uid_to_cookie.find(path);
    if(it_path == _map_uid_to_cookie.end()){
        //该path下未有任意用户登录
        return;
    }
    auto it_uid = it_path->second.find(uid);
    if(it_uid == it_path->second.end()){
        //该用户尚未登录
        return;
    }

    //遍历同一名用户下的所有客户端，移除命中的客户端
    for(auto it_cookie = it_uid->second.begin() ; it_cookie != it_uid->second.end() ; ++it_cookie ){
        if(it_cookie->second != cookie) {
            //不是该cookie
            continue;
        }
        //移除该用户名下的某个cookie，这个设备cookie将失效
        it_uid->second.erase(it_cookie);

        if(it_uid->second.size() != 0) {
            break;
        }

        //该用户名下没有任何设备在线，移除之
        it_path->second.erase(it_uid);

        if(it_path->second.size() != 0) {
            break;
        }
        //该path下未有任何用户在线，移除之
        _map_uid_to_cookie.erase(it_path);
        break;
    }

}

string CookieManager::getOldestCookie(const string &uid, int max_client,const string &path){
    lock_guard<recursive_mutex> lck(_mtx_cookie);
    auto it_path = _map_uid_to_cookie.find(path);
    if(it_path == _map_uid_to_cookie.end()){
        //该路径下未有任意cookie
        return "";
    }
    auto it_uid = it_path->second.find(uid);
    if(it_uid == it_path->second.end()){
        //该用户从未登录过
        return "";
    }
    if(it_uid->second.size() < MAX(1,max_client)){
        //同一名用户下，客户端个数还没达到限制个数
        return "";
    }
    //客户端个数超过限制，移除最先登录的客户端
    return it_uid->second.begin()->second;
}

/////////////////////////////////CookieGeneator////////////////////////////////////
string CookieGeneator::obtain(){
    //获取唯一的防膨胀的随机字符串
    while (true){
        auto str = obtain_l();
        if(_obtained.find(str) == _obtained.end()){
            //没有重复
            _obtained.emplace(str);
            return str;
        }
    }
}
void CookieGeneator::release(const string &str){
    //从防膨胀库中移除
    _obtained.erase(str);
}

string CookieGeneator::obtain_l(){
    //12个伪随机字节 + 4个递增的整形字节，然后md5即为随机字符串
    auto str = makeRandStr(12,false);
    str.append((char *)&_index, sizeof(_index));
    ++_index;
    return MD5(str).hexdigest();
}