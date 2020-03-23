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


#include "MediaSource.h"
#include "Record/MP4Reader.h"
#include "Util/util.h"
#include "Network/sockutil.h"
#include "Network/TcpSession.h"

using namespace toolkit;

namespace mediakit {

recursive_mutex MediaSource::g_mtxMediaSrc;
MediaSource::SchemaVhostAppStreamMap MediaSource::g_mapMediaSrc;

MediaSource::MediaSource(const string &strSchema, const string &strVhost, const string &strApp, const string &strId) :
        _strSchema(strSchema), _strApp(strApp), _strId(strId) {
    if (strVhost.empty()) {
        _strVhost = DEFAULT_VHOST;
    } else {
        _strVhost = strVhost;
    }
}

MediaSource::~MediaSource() {
    unregist();
}

const string& MediaSource::getSchema() const {
    return _strSchema;
}

const string& MediaSource::getVhost() const {
    return _strVhost;
}

const string& MediaSource::getApp() const {
    //获取该源的id
    return _strApp;
}

const string& MediaSource::getId() const {
    return _strId;
}

vector<Track::Ptr> MediaSource::getTracks(bool trackReady) const {
    auto strongPtr = _track_source.lock();
    if(strongPtr){
        return strongPtr->getTracks(trackReady);
    }
    return vector<Track::Ptr>();
}

void MediaSource::setTrackSource(const std::weak_ptr<TrackSource> &track_src) {
    _track_source = track_src;
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaResetTracks, *this);
}

void MediaSource::setListener(const std::weak_ptr<MediaSourceEvent> &listener){
    _listener = listener;
}

const std::weak_ptr<MediaSourceEvent>& MediaSource::getListener() const{
    return _listener;
}

int MediaSource::totalReaderCount(){
    auto listener = _listener.lock();
    if(!listener){
        return readerCount();
    }
    return listener->totalReaderCount(*this);
}
bool MediaSource::seekTo(uint32_t ui32Stamp) {
    auto listener = _listener.lock();
    if(!listener){
        return false;
    }
    return listener->seekTo(*this,ui32Stamp);
}

bool MediaSource::close(bool force) {
    auto listener = _listener.lock();
    if(!listener){
        return false;
    }
    return listener->close(*this,force);
}

void MediaSource::onNoneReader(){
    auto listener = _listener.lock();
    if(!listener){
        return;
    }
    if (listener->totalReaderCount(*this) == 0) {
        listener->onNoneReader(*this);
    }
}

void MediaSource::for_each_media(const function<void(const MediaSource::Ptr &src)> &cb) {
    lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
    for (auto &pr0 : g_mapMediaSrc) {
        for (auto &pr1 : pr0.second) {
            for (auto &pr2 : pr1.second) {
                for (auto &pr3 : pr2.second) {
                    auto src = pr3.second.lock();
                    if(src){
                        cb(src);
                    }
                }
            }
        }
    }
}

template<typename MAP, typename FUNC>
static bool searchMedia(MAP &map, const string &schema, const string &vhost, const string &app, const string &id, FUNC &&func) {
    auto it0 = map.find(schema);
    if (it0 == map.end()) {
        //未找到协议
        return false;
    }
    auto it1 = it0->second.find(vhost);
    if (it1 == it0->second.end()) {
        //未找到vhost
        return false;
    }
    auto it2 = it1->second.find(app);
    if (it2 == it1->second.end()) {
        //未找到app
        return false;
    }
    auto it3 = it2->second.find(id);
    if (it3 == it2->second.end()) {
        //未找到streamId
        return false;
    }
    return func(it0, it1, it2, it3);
}

template<typename MAP, typename IT0, typename IT1, typename IT2>
static void eraseIfEmpty(MAP &map, IT0 it0, IT1 it1, IT2 it2) {
    if (it2->second.empty()) {
        it1->second.erase(it2);
        if (it1->second.empty()) {
            it0->second.erase(it1);
            if (it0->second.empty()) {
                map.erase(it0);
            }
        }
    }
};

void findAsync_l(const MediaInfo &info, const std::shared_ptr<TcpSession> &session, bool retry,
                 const function<void(const MediaSource::Ptr &src)> &cb){
    auto src = MediaSource::find(info._schema, info._vhost, info._app, info._streamid, true);
    if(src || !retry){
        cb(src);
        return;
    }

    void *listener_tag = session.get();
    weak_ptr<TcpSession> weakSession = session;
    //广播未找到流,此时可以立即去拉流，这样还来得及
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastNotFoundStream,info,*session);

    //最多等待一定时间，如果这个时间内，流未注册上，那么返回未找到流
    GET_CONFIG(int,maxWaitMS,General::kMaxStreamWaitTimeMS);

    //若干秒后执行等待媒体注册超时回调
    auto onRegistTimeout = session->getPoller()->doDelayTask(maxWaitMS,[cb,listener_tag](){
        //取消监听该事件
        NoticeCenter::Instance().delListener(listener_tag,Broadcast::kBroadcastMediaChanged);
        cb(nullptr);
        return 0;
    });

    auto onRegist = [listener_tag,weakSession,info,cb,onRegistTimeout](BroadcastMediaChangedArgs) {
        auto strongSession = weakSession.lock();
        if(!strongSession) {
            //自己已经销毁
            //取消延时任务，防止多次回调
            onRegistTimeout->cancel();
            //取消事件监听
            NoticeCenter::Instance().delListener(listener_tag,Broadcast::kBroadcastMediaChanged);
            return;
        }

        if (!bRegist ||
            sender.getSchema() != info._schema ||
            sender.getVhost() != info._vhost ||
            sender.getApp() != info._app ||
            sender.getId() != info._streamid) {
            //不是自己感兴趣的事件，忽略之
            return;
        }

        //取消延时任务，防止多次回调
        onRegistTimeout->cancel();
        //取消事件监听
        NoticeCenter::Instance().delListener(listener_tag,Broadcast::kBroadcastMediaChanged);

        //播发器请求的流终于注册上了，切换到自己的线程再回复
        strongSession->async([weakSession,info,cb](){
            auto strongSession = weakSession.lock();
            if(!strongSession) {
                return;
            }
            DebugL << "收到媒体注册事件,回复播放器:" << info._schema << "/" << info._vhost << "/" << info._app << "/" << info._streamid;
            //再找一遍媒体源，一般能找到
            findAsync_l(info,strongSession,false,cb);
        }, false);
    };
    //监听媒体注册事件
    NoticeCenter::Instance().addListener(listener_tag, Broadcast::kBroadcastMediaChanged, onRegist);
}

void MediaSource::findAsync(const MediaInfo &info, const std::shared_ptr<TcpSession> &session,const function<void(const Ptr &src)> &cb){
    return findAsync_l(info, session, true, cb);
}

MediaSource::Ptr MediaSource::find(const string &schema, const string &vhost_tmp, const string &app, const string &id, bool bMake) {
    string vhost = vhost_tmp;
    if(vhost.empty()){
        vhost = DEFAULT_VHOST;
    }

    GET_CONFIG(bool,enableVhost,General::kEnableVhost);
    if(!enableVhost){
        vhost = DEFAULT_VHOST;
    }

    MediaSource::Ptr ret;
    {
        lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
        //查找某一媒体源，找到后返回
        searchMedia(g_mapMediaSrc, schema, vhost, app, id, [&](SchemaVhostAppStreamMap::iterator &it0,
                                                               VhostAppStreamMap::iterator &it1,
                                                               AppStreamMap::iterator &it2,
                                                               StreamMap::iterator &it3) {
            ret = it3->second.lock();
            if (!ret) {
                //该对象已经销毁
                it2->second.erase(it3);
                eraseIfEmpty(g_mapMediaSrc, it0, it1, it2);
                return false;
            }
            return true;
        });
    }

    if(!ret && bMake){
        //未查找媒体源，则创建一个
        ret = MP4Reader::onMakeMediaSource(schema, vhost,app,id);
    }
    return ret;
}
void MediaSource::regist() {
    GET_CONFIG(bool,enableVhost,General::kEnableVhost);
    if(!enableVhost){
        _strVhost = DEFAULT_VHOST;
    }
    //注册该源，注册后服务器才能找到该源
    {
        lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
        g_mapMediaSrc[_strSchema][_strVhost][_strApp][_strId] =  shared_from_this();
    }
    InfoL << _strSchema << " " << _strVhost << " " << _strApp << " " << _strId;
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaChanged, true, *this);
}

//反注册该源
bool MediaSource::unregist() {
    bool ret;
    {
        lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
        ret = searchMedia(g_mapMediaSrc, _strSchema, _strVhost, _strApp, _strId,
                          [&](SchemaVhostAppStreamMap::iterator &it0,
                              VhostAppStreamMap::iterator &it1,
                              AppStreamMap::iterator &it2,
                              StreamMap::iterator &it3) {
                              auto strongMedia = it3->second.lock();
                              if (strongMedia && this != strongMedia.get()) {
                                  //不是自己,不允许反注册
                                  return false;
                              }
                              it2->second.erase(it3);
                              eraseIfEmpty(g_mapMediaSrc, it0, it1, it2);
                              return true;
                          });
    }

    if(ret){
        InfoL <<  "" <<  _strSchema << " " << _strVhost << " " << _strApp << " " << _strId;
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaChanged, false, *this);
    }
    return ret;
}

/////////////////////////////////////MediaInfo//////////////////////////////////////

void MediaInfo::parse(const string &url){
    //string url = "rtsp://127.0.0.1:8554/live/id?key=val&a=1&&b=2&vhost=vhost.com";
    auto schema_pos = url.find("://");
    if(schema_pos != string::npos){
        _schema = url.substr(0,schema_pos);
    }else{
        schema_pos = -3;
    }
    auto split_vec = split(url.substr(schema_pos + 3),"/");
    if(split_vec.size() > 0){
        auto vhost = split_vec[0];
        auto pos = vhost.find(":");
        if(pos != string::npos){
            _host = _vhost = vhost.substr(0,pos);
            _port = vhost.substr(pos + 1);
        } else{
            _host = _vhost = vhost;
        }

        if(_vhost == "localhost" || INADDR_NONE != inet_addr(_vhost.data())){
            //如果访问的是localhost或ip，那么则为默认虚拟主机
            _vhost = DEFAULT_VHOST;
        }

    }
    if(split_vec.size() > 1){
        _app = split_vec[1];
    }
    if(split_vec.size() > 2){
        string steamid;
        for(int i = 2 ; i < split_vec.size() ; ++i){
            steamid.append(split_vec[i] + "/");
        }
        if(steamid.back() == '/'){
            steamid.pop_back();
        }
        auto pos = steamid.find("?");
        if(pos != string::npos){
            _streamid = steamid.substr(0,pos);
            _param_strs = steamid.substr(pos + 1);
            auto params = Parser::parseArgs(_param_strs);
            if(params.find(VHOST_KEY) != params.end()){
                _vhost = params[VHOST_KEY];
            }
        } else{
            _streamid = steamid;
        }
    }

    GET_CONFIG(bool,enableVhost,General::kEnableVhost);
    if(!enableVhost || _vhost.empty()){
        //如果关闭虚拟主机或者虚拟主机为空，则设置虚拟主机为默认
        _vhost = DEFAULT_VHOST;
    }
}

/////////////////////////////////////MediaSourceEvent//////////////////////////////////////

void MediaSourceEvent::onNoneReader(MediaSource &sender){
    //没有任何读取器消费该源，表明该源可以关闭了
    GET_CONFIG(int, stream_none_reader_delay, General::kStreamNoneReaderDelayMS);

    weak_ptr<MediaSource> weakSender = sender.shared_from_this();
    _async_close_timer = std::make_shared<Timer>(stream_none_reader_delay / 1000.0, [weakSender]() {
        auto strongSender = weakSender.lock();
        if (!strongSender) {
            //对象已经销毁
            return false;
        }

        if (strongSender->totalReaderCount() != 0) {
            //还有人消费
            return false;
        }

        WarnL << "onNoneReader:"
              << strongSender->getSchema() << "/"
              << strongSender->getVhost() << "/"
              << strongSender->getApp() << "/"
              << strongSender->getId();

        //触发消息广播
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastStreamNoneReader, *strongSender);
        return false;
    }, nullptr);
}


} /* namespace mediakit */