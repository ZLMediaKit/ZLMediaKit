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
#include "MediaFile/MediaReader.h"
#include "Util/util.h"
#include "Rtsp/Rtsp.h"
#include "Network/sockutil.h"
#include "Network/TcpSession.h"

using namespace toolkit;

namespace mediakit {

recursive_mutex MediaSource::g_mtxMediaSrc;
MediaSource::SchemaVhostAppStreamMap MediaSource::g_mapMediaSrc;


void MediaSource::findAsync(const MediaInfo &info,
                            const std::shared_ptr<TcpSession> &session,
                            bool retry,
                            const function<void(const MediaSource::Ptr &src)> &cb){

    auto src = MediaSource::find(info._schema,
                                 info._vhost,
                                 info._app,
                                 info._streamid,
                                 true);
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
        if(!bRegist || schema != info._schema || vhost != info._vhost || app != info._app ||stream != info._streamid){
            //不是自己感兴趣的事件，忽略之
            return;
        }

        //取消延时任务，防止多次回调
        onRegistTimeout->cancel();

        //播发器请求的流终于注册上了
        auto strongSession = weakSession.lock();
        if(!strongSession) {
            return;
        }

        //切换到自己的线程再回复
        strongSession->async([listener_tag,weakSession,info,cb](){
            auto strongSession = weakSession.lock();
            if(!strongSession) {
                return;
            }
            DebugL << "收到媒体注册事件,回复播放器:" << info._schema << "/" << info._vhost << "/" << info._app << "/" << info._streamid;
            //再找一遍媒体源，一般能找到
            findAsync(info,strongSession,false,cb);
            //取消事件监听
            NoticeCenter::Instance().delListener(listener_tag,Broadcast::kBroadcastMediaChanged);
        }, false);
    };
    //监听媒体注册事件
    NoticeCenter::Instance().addListener(listener_tag, Broadcast::kBroadcastMediaChanged, onRegist);
}
MediaSource::Ptr MediaSource::find(
        const string &schema,
        const string &vhost_tmp,
        const string &app,
        const string &id,
        bool bMake) {
    string vhost = vhost_tmp;
    if(vhost.empty()){
        vhost = DEFAULT_VHOST;
    }

    GET_CONFIG(bool,enableVhost,General::kEnableVhost);
    if(!enableVhost){
        vhost = DEFAULT_VHOST;
    }

    lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
    MediaSource::Ptr ret;
    searchMedia(schema, vhost, app, id,
                [&](SchemaVhostAppStreamMap::iterator &it0 ,
                    VhostAppStreamMap::iterator &it1,
                    AppStreamMap::iterator &it2,
                    StreamMap::iterator &it3){
                    ret = it3->second.lock();
                    if(!ret){
                        //该对象已经销毁
                        it2->second.erase(it3);
                        eraseIfEmpty(it0,it1,it2);
                        return false;
                    }
                    return true;
                });
    if(!ret && bMake){
        //查找某一媒体源，找到后返回
        ret = MediaReader::onMakeMediaSource(schema, vhost,app,id);
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
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaChanged,
                                       true,
                                       _strSchema,
                                       _strVhost,
                                       _strApp,
                                       _strId,
                                       *this);
}
bool MediaSource::unregist() {
    //反注册该源
    lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
    return searchMedia(_strSchema, _strVhost, _strApp, _strId, [&](SchemaVhostAppStreamMap::iterator &it0 ,
                                                                       VhostAppStreamMap::iterator &it1,
                                                                       AppStreamMap::iterator &it2,
                                                                       StreamMap::iterator &it3){
        auto strongMedia = it3->second.lock();
        if(strongMedia && this != strongMedia.get()){
            //不是自己,不允许反注册
            return false;
        }
        it2->second.erase(it3);
        eraseIfEmpty(it0,it1,it2);
        unregisted();
        return true;
    });
}
void MediaSource::unregisted(){
    InfoL <<  "" <<  _strSchema << " " << _strVhost << " " << _strApp << " " << _strId;
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaChanged,
                                       false,
                                       _strSchema,
                                       _strVhost,
                                       _strApp,
                                       _strId,
                                       *this);
}

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
            _params = Parser::parseArgs(_param_strs);
            if(_params.find(VHOST_KEY) != _params.end()){
                _vhost = _params[VHOST_KEY];
            }
        } else{
            _streamid = steamid;
        }
    }

    GET_CONFIG(bool,enableVhost,General::kEnableVhost);
    if(!enableVhost || _vhost.empty() || _vhost == "localhost" || INADDR_NONE != inet_addr(_vhost.data())){
        _vhost = DEFAULT_VHOST;
    }
}

void MediaSourceEvent::onNoneReader(MediaSource &sender){
    WarnL << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId();
    //没有任何读取器消费该源，表明该源可以关闭了
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastStreamNoneReader,sender);
}


} /* namespace mediakit */