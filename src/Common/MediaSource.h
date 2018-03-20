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


#ifndef ZLMEDIAKIT_MEDIASOURCE_H
#define ZLMEDIAKIT_MEDIASOURCE_H

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "Common/config.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Util/NoticeCenter.h"
#include "Rtsp/Rtsp.h"

using namespace std;
using namespace Config;
using namespace ZL::Util;

namespace ZL {
namespace Media {

class MediaSourceEvent
{
public:
    MediaSourceEvent(){};
    virtual ~MediaSourceEvent(){};
public:
    virtual bool seekTo(uint32_t ui32Stamp){
        //拖动进度条
        return false;
    }
    virtual uint32_t getStamp() {
        //获取时间戳
        return 0;
    }
    virtual bool shutDown() {
        //通知其停止推流
        return false;
    }
};
class MediaInfo
{
public:
    MediaInfo(){}
    MediaInfo(const string &url){
        parse(url);
    }
    ~MediaInfo(){}

    void parse(const string &url);

    string &operator[](const string &key){
        return m_params[key];
    }
public:
    string m_schema;
    string m_host;
    string m_port;
    string m_vhost;
    string m_app;
    string m_streamid;
    StrCaseMap m_params;
    string m_param_strs;

};


class MediaSource: public enable_shared_from_this<MediaSource> {
public:
    typedef std::shared_ptr<MediaSource> Ptr;
    typedef unordered_map<string, weak_ptr<MediaSource> > StreamMap;
    typedef unordered_map<string, StreamMap > AppStreamMap;
    typedef unordered_map<string, AppStreamMap > VhostAppStreamMap;
    typedef unordered_map<string, VhostAppStreamMap > SchemaVhostAppStreamMap;

    MediaSource(const string &strSchema,
                const string &strVhost,
                const string &strApp,
                const string &strId) :
            m_strSchema(strSchema),
            m_strApp(strApp),
            m_strId(strId) {
        if(strVhost.empty()){
            m_strVhost = DEFAULT_VHOST;
        }else{
            m_strVhost = strVhost;
        }
    }
    virtual ~MediaSource() {
        unregist();
    }

    virtual bool regist() ;
    virtual bool unregist() ;

    static Ptr find(const string &schema,
                    const string &vhost,
                    const string &app,
                    const string &id,
                    bool bMake = true) ;

    const string& getSchema() const {
        return m_strSchema;
    }
    const string& getVhost() const {
        return m_strVhost;
    }
    const string& getApp() const {
        //获取该源的id
        return m_strApp;
    }
    const string& getId() const {
        return m_strId;
    }

    bool seekTo(uint32_t ui32Stamp) {
        auto listener = m_listener.lock();
        if(!listener){
            return false;
        }
        return listener->seekTo(ui32Stamp);
    }

    uint32_t getStamp() {
        auto listener = m_listener.lock();
        if(!listener){
            return 0;
        }
        return listener->getStamp();
    }
    bool shutDown() {
        auto listener = m_listener.lock();
        if(!listener){
            return false;
        }
        return listener->shutDown();
    }
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        m_listener = listener;
    }

    template <typename FUN>
    static void for_each_media(FUN && fun){
        lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
        for (auto &pr0 : g_mapMediaSrc){
            for(auto &pr1 : pr0.second){
                for(auto &pr2 : pr1.second){
                    for(auto &pr3 : pr2.second){
                        fun(pr0.first,pr1.first,pr2.first,pr3.first,pr3.second.lock());
                    }
                }
            }
        }
    }
private:
        template <typename FUN>
        static bool searchMedia(const string &schema,
                                const string &vhost,
                                const string &app,
                                const string &id,
                                FUN &&fun){
        auto it0 = g_mapMediaSrc.find(schema);
        if (it0 == g_mapMediaSrc.end()) {
            //未找到协议
            return false;
        }
        auto it1 = it0->second.find(vhost);
        if(it1 == it0->second.end()){
            //未找到vhost
            return false;
        }
        auto it2 = it1->second.find(app);
        if(it2 == it1->second.end()){
            //未找到app
            return false;
        }
        auto it3 = it2->second.find(id);
        if(it3 == it2->second.end()){
            //未找到streamId
            return false;
        }
        return fun(it0,it1,it2,it3);
    }
    template <typename IT0,typename IT1,typename IT2>
    static void eraseIfEmpty(IT0 it0,IT1 it1,IT2 it2){
        if(it2->second.empty()){
            it1->second.erase(it2);
            if(it1->second.empty()){
                it0->second.erase(it1);
                if(it0->second.empty()){
                    g_mapMediaSrc.erase(it0);
                }
            }
        }
    };

    void unregisted();
protected:
    std::weak_ptr<MediaSourceEvent> m_listener;
private:
    string m_strSchema;//协议类型
    string m_strVhost; //vhost
    string m_strApp; //媒体app
    string m_strId; //媒体id
    static SchemaVhostAppStreamMap g_mapMediaSrc; //静态的媒体源表
    static recursive_mutex g_mtxMediaSrc; //访问静态的媒体源表的互斥锁
};

} /* namespace Media */
} /* namespace ZL */


#endif //ZLMEDIAKIT_MEDIASOURCE_H
