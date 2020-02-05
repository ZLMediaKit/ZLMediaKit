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


#ifndef ZLMEDIAKIT_MEDIASOURCE_H
#define ZLMEDIAKIT_MEDIASOURCE_H

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "Common/config.h"
#include "Common/Parser.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Util/NoticeCenter.h"
#include "Extension/Track.h"

using namespace std;
using namespace toolkit;

namespace toolkit{
    class TcpSession;
}// namespace toolkit

namespace mediakit {

class MediaSource;
class MediaSourceEvent{
public:
    MediaSourceEvent(){};
    virtual ~MediaSourceEvent(){};

    // 通知拖动进度条
    virtual bool seekTo(MediaSource &sender,uint32_t ui32Stamp){
        return false;
    }

    // 通知其停止推流
    virtual bool close(MediaSource &sender,bool force) {
        return false;
    }

    // 通知无人观看
    virtual void onNoneReader(MediaSource &sender);

    // 观看总人数
    virtual int totalReaderCount(MediaSource &sender) = 0;
};

/**
 * 解析url获取媒体相关信息
 */
class MediaInfo{
public:
    MediaInfo(){}
    ~MediaInfo(){}
    MediaInfo(const string &url){ parse(url); }
    void parse(const string &url);
public:
    string _schema;
    string _host;
    string _port;
    string _vhost;
    string _app;
    string _streamid;
    string _param_strs;
};

/**
 * 媒体源，任何rtsp/rtmp的直播流都源自该对象
 */
class MediaSource: public TrackSource, public enable_shared_from_this<MediaSource> {
public:
    typedef std::shared_ptr<MediaSource> Ptr;
    typedef unordered_map<string, weak_ptr<MediaSource> > StreamMap;
    typedef unordered_map<string, StreamMap > AppStreamMap;
    typedef unordered_map<string, AppStreamMap > VhostAppStreamMap;
    typedef unordered_map<string, VhostAppStreamMap > SchemaVhostAppStreamMap;

    MediaSource(const string &strSchema, const string &strVhost, const string &strApp, const string &strId) ;
    virtual ~MediaSource() ;

    // 获取协议类型
    const string& getSchema() const;
    // 虚拟主机
    const string& getVhost() const;
    // 应用名
    const string& getApp() const;
    // 流id
    const string& getId() const;

    // 设置TrackSource
    void setTrackSource(const std::weak_ptr<TrackSource> &track_src);
    // 获取所有Track
    vector<Track::Ptr> getTracks(bool trackReady = true) const override;

    // 设置监听者
    virtual void setListener(const std::weak_ptr<MediaSourceEvent> &listener);
    // 获取监听者
    const std::weak_ptr<MediaSourceEvent>& getListener() const;


    // 本协议获取观看者个数，可能返回本协议的观看人数，也可能返回总人数
    virtual int readerCount() = 0;
    // 观看者个数，包括(hls/rtsp/rtmp)
    virtual int totalReaderCount();

    // 获取流当前时间戳
    virtual uint32_t getTimeStamp(TrackType trackType) { return 0; };
    // 设置时间戳
    virtual void setTimeStamp(uint32_t uiStamp) {};

    // 拖动进度条
    bool seekTo(uint32_t ui32Stamp);
    // 关闭该流
    bool close(bool force);
    // 该流无人观看
    void onNoneReader();

    // 同步查找流
    static Ptr find(const string &schema, const string &vhost, const string &app, const string &id, bool bMake = true) ;
    // 异步查找流
    static void findAsync(const MediaInfo &info, const std::shared_ptr<TcpSession> &session, const function<void(const Ptr &src)> &cb);
    // 遍历所有流
    static void for_each_media(const function<void(const Ptr &src)> &cb);
protected:
    void regist() ;
    bool unregist() ;
    void unregisted();
private:
    string _strSchema;
    string _strVhost;
    string _strApp;
    string _strId;
    std::weak_ptr<MediaSourceEvent> _listener;
    weak_ptr<TrackSource> _track_source;
    static SchemaVhostAppStreamMap g_mapMediaSrc;
    static recursive_mutex g_mtxMediaSrc;
};

} /* namespace mediakit */


#endif //ZLMEDIAKIT_MEDIASOURCE_H
