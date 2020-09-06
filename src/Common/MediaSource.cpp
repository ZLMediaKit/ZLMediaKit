/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include "MediaSource.h"
#include "Record/MP4Reader.h"
#include "Util/util.h"
#include "Network/sockutil.h"
#include "Network/TcpSession.h"
using namespace toolkit;
namespace mediakit {

recursive_mutex s_media_source_mtx;
MediaSource::SchemaVhostAppStreamMap s_media_source_map;

MediaSource::MediaSource(const string &schema, const string &vhost, const string &app, const string &stream_id){
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    if (!enableVhost) {
        _vhost = DEFAULT_VHOST;
    } else {
        _vhost = vhost.empty() ? DEFAULT_VHOST : vhost;
    }
    _schema = schema;
    _app = app;
    _stream_id = stream_id;
}

MediaSource::~MediaSource() {
    unregist();
}

const string& MediaSource::getSchema() const {
    return _schema;
}

const string& MediaSource::getVhost() const {
    return _vhost;
}

const string& MediaSource::getApp() const {
    //获取该源的id
    return _app;
}

const string& MediaSource::getId() const {
    return _stream_id;
}

vector<Track::Ptr> MediaSource::getTracks(bool ready) const {
    auto listener = _listener.lock();
    if(!listener){
        return vector<Track::Ptr>();
    }
    return listener->getTracks(const_cast<MediaSource &>(*this), ready);
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

bool MediaSource::seekTo(uint32_t stamp) {
    auto listener = _listener.lock();
    if(!listener){
        return false;
    }
    return listener->seekTo(*this, stamp);
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

bool MediaSource::setupRecord(Recorder::type type, bool start, const string &custom_path){
    auto listener = _listener.lock();
    if (!listener) {
        WarnL << "未设置MediaSource的事件监听者，setupRecord失败:" << getSchema() << "/" << getVhost() << "/" << getApp() << "/" << getId();
        return false;
    }
    return listener->setupRecord(*this, type, start, custom_path);
}

bool MediaSource::isRecording(Recorder::type type){
    auto listener = _listener.lock();
    if(!listener){
        return false;
    }
    return listener->isRecording(*this, type);
}

void MediaSource::startSendRtp(const string &dst_url, uint16_t dst_port, uint32_t ssrc, bool is_udp, const function<void(const SockException &ex)> &cb){
    auto listener = _listener.lock();
    if (!listener) {
        cb(SockException(Err_other, "尚未设置事件监听器"));
        return;
    }
    return listener->startSendRtp(*this, dst_url, dst_port, ssrc, is_udp, cb);
}

bool MediaSource::stopSendRtp() {
    auto listener = _listener.lock();
    if (!listener) {
        return false;
    }
    return listener->stopSendRtp(*this);
}

void MediaSource::for_each_media(const function<void(const MediaSource::Ptr &src)> &cb) {
    decltype(s_media_source_map) copy;
    {
        //拷贝s_media_source_map后再遍历，考虑到是高频使用的全局单例锁，并且在上锁时会执行回调代码
        //很容易导致多个锁交叉死锁的情况，而且该函数使用频率不高，拷贝开销相对来说是可以接受的
        lock_guard<recursive_mutex> lock(s_media_source_mtx);
        copy = s_media_source_map;
    }

    for (auto &pr0 : copy) {
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
}

static MediaSource::Ptr find_l(const string &schema, const string &vhost_in, const string &app, const string &id, bool create_new) {
    string vhost = vhost_in;
    GET_CONFIG(bool,enableVhost,General::kEnableVhost);
    if(vhost.empty() || !enableVhost){
        vhost = DEFAULT_VHOST;
    }

    MediaSource::Ptr ret;
    {
        lock_guard<recursive_mutex> lock(s_media_source_mtx);
        //查找某一媒体源，找到后返回
        searchMedia(s_media_source_map, schema, vhost, app, id,
                    [&](MediaSource::SchemaVhostAppStreamMap::iterator &it0, MediaSource::VhostAppStreamMap::iterator &it1,
                        MediaSource::AppStreamMap::iterator &it2, MediaSource::StreamMap::iterator &it3) {
            ret = it3->second.lock();
            if (!ret) {
                //该对象已经销毁
                it2->second.erase(it3);
                eraseIfEmpty(s_media_source_map, it0, it1, it2);
                return false;
            }
            return true;
        });
    }

    if(!ret && create_new){
        //未查找媒体源，则创建一个
        ret = MediaSource::createFromMP4(schema, vhost, app, id);
    }
    return ret;
}

static void findAsync_l(const MediaInfo &info, const std::shared_ptr<TcpSession> &session, bool retry,
                        const function<void(const MediaSource::Ptr &src)> &cb){
    auto src = find_l(info._schema, info._vhost, info._app, info._streamid, true);
    if (src || !retry) {
        cb(src);
        return;
    }

    void *listener_tag = session.get();
    weak_ptr<TcpSession> weak_session = session;

    GET_CONFIG(int, maxWaitMS, General::kMaxStreamWaitTimeMS);
    auto on_timeout = session->getPoller()->doDelayTask(maxWaitMS, [cb, listener_tag]() {
        //最多等待一定时间，如果这个时间内，流未注册上，那么返回未找到流
        NoticeCenter::Instance().delListener(listener_tag, Broadcast::kBroadcastMediaChanged);
        cb(nullptr);
        return 0;
    });

    auto cancel_all = [on_timeout, listener_tag]() {
        //取消延时任务，防止多次回调
        on_timeout->cancel();
        //取消媒体注册事件监听
        NoticeCenter::Instance().delListener(listener_tag, Broadcast::kBroadcastMediaChanged);
    };

    function<void()> close_player = [cb, cancel_all]() {
        cancel_all();
        //告诉播放器，流不存在，这样会立即断开播放器
        cb(nullptr);
    };

    auto on_regist = [weak_session, info, cb, cancel_all](BroadcastMediaChangedArgs) {
        auto strong_session = weak_session.lock();
        if (!strong_session) {
            //自己已经销毁
            cancel_all();
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

        cancel_all();

        //播发器请求的流终于注册上了，切换到自己的线程再回复
        strong_session->async([weak_session, info, cb]() {
            auto strongSession = weak_session.lock();
            if (!strongSession) {
                return;
            }
            DebugL << "收到媒体注册事件,回复播放器:" << info._schema << "/" << info._vhost << "/" << info._app << "/" << info._streamid;
            //再找一遍媒体源，一般能找到
            findAsync_l(info, strongSession, false, cb);
        }, false);
    };

    //监听媒体注册事件
    NoticeCenter::Instance().addListener(listener_tag, Broadcast::kBroadcastMediaChanged, on_regist);
    //广播未找到流,此时可以立即去拉流，这样还来得及
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastNotFoundStream, info, static_cast<SockInfo &>(*session), close_player);
}

void MediaSource::findAsync(const MediaInfo &info, const std::shared_ptr<TcpSession> &session,const function<void(const Ptr &src)> &cb){
    return findAsync_l(info, session, true, cb);
}

MediaSource::Ptr MediaSource::find(const string &schema, const string &vhost, const string &app, const string &id) {
    return find_l(schema, vhost, app, id, false);
}

MediaSource::Ptr MediaSource::find(const string &vhost, const string &app, const string &stream_id){
    auto src = MediaSource::find(RTMP_SCHEMA, vhost, app, stream_id);
    if (src) {
        return src;
    }
    src = MediaSource::find(RTSP_SCHEMA, vhost, app, stream_id);
    if (src) {
        return src;
    }
    return MediaSource::find(HLS_SCHEMA, vhost, app, stream_id);
}

static string getTrackInfoStr(const TrackSource *track_src){
    _StrPrinter codec_info;
    auto tracks = track_src->getTracks(true);
    for (auto &track : tracks) {
        auto codec_type = track->getTrackType();
        codec_info << track->getCodecName();
        switch (codec_type) {
            case TrackAudio : {
                auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
                codec_info << "["
                           << audio_track->getAudioSampleRate() << "/"
                           << audio_track->getAudioChannel() << "/"
                           << audio_track->getAudioSampleBit() << "] ";
                break;
            }
            case TrackVideo : {
                auto video_track = dynamic_pointer_cast<VideoTrack>(track);
                codec_info << "["
                           << video_track->getVideoWidth() << "/"
                           << video_track->getVideoHeight() << "/"
                           << round(video_track->getVideoFps()) << "] ";
                break;
            }
            default:
                break;
        }
    }
    return codec_info;
}

void MediaSource::emitEvent(bool regist){
    auto listener = _listener.lock();
    if (listener) {
        //触发回调
        listener->onRegist(*this, regist);
    }
    //触发广播
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaChanged, regist, *this);
    InfoL << (regist ? "媒体注册:" : "媒体注销:") << _schema << " " << _vhost << " " << _app << " " << _stream_id << " " << getTrackInfoStr(this);
}

void MediaSource::regist() {
    {
        //减小互斥锁临界区
        lock_guard<recursive_mutex> lock(s_media_source_mtx);
        s_media_source_map[_schema][_vhost][_app][_stream_id] = shared_from_this();
    }
    emitEvent(true);
}

//反注册该源
bool MediaSource::unregist() {
    bool ret;
    {
        //减小互斥锁临界区
        lock_guard<recursive_mutex> lock(s_media_source_mtx);
        ret = searchMedia(s_media_source_map, _schema, _vhost, _app, _stream_id,
                          [&](SchemaVhostAppStreamMap::iterator &it0, VhostAppStreamMap::iterator &it1,
                              AppStreamMap::iterator &it2, StreamMap::iterator &it3) {
          auto strong_self = it3->second.lock();
          if (strong_self && this != strong_self.get()) {
              //不是自己,不允许反注册
              return false;
          }
          it2->second.erase(it3);
          eraseIfEmpty(s_media_source_map, it0, it1, it2);
          return true;
      });
    }

    if (ret) {
        emitEvent(false);
    }
    return ret;
}

/////////////////////////////////////MediaInfo//////////////////////////////////////

void MediaInfo::parse(const string &url){
    //string url = "rtsp://127.0.0.1:8554/live/id?key=val&a=1&&b=2&vhost=vhost.com";
    auto schema_pos = url.find("://");
    if (schema_pos != string::npos) {
        _schema = url.substr(0, schema_pos);
    } else {
        schema_pos = -3;
    }
    auto split_vec = split(url.substr(schema_pos + 3), "/");
    if (split_vec.size() > 0) {
        auto vhost = split_vec[0];
        auto pos = vhost.find(":");
        if (pos != string::npos) {
            _host = _vhost = vhost.substr(0, pos);
            _port = vhost.substr(pos + 1);
        } else {
            _host = _vhost = vhost;
        }

        if (_vhost == "localhost" || INADDR_NONE != inet_addr(_vhost.data())) {
            //如果访问的是localhost或ip，那么则为默认虚拟主机
            _vhost = DEFAULT_VHOST;
        }

    }
    if (split_vec.size() > 1) {
        _app = split_vec[1];
    }
    if (split_vec.size() > 2) {
        string stream_id;
        for (int i = 2; i < split_vec.size(); ++i) {
            stream_id.append(split_vec[i] + "/");
        }
        if (stream_id.back() == '/') {
            stream_id.pop_back();
        }
        auto pos = stream_id.find("?");
        if (pos != string::npos) {
            _streamid = stream_id.substr(0, pos);
            _param_strs = stream_id.substr(pos + 1);
            auto params = Parser::parseArgs(_param_strs);
            if (params.find(VHOST_KEY) != params.end()) {
                _vhost = params[VHOST_KEY];
            }
        } else {
            _streamid = stream_id;
        }
    }

    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    if (!enableVhost || _vhost.empty()) {
        //如果关闭虚拟主机或者虚拟主机为空，则设置虚拟主机为默认
        _vhost = DEFAULT_VHOST;
    }
}

MediaSource::Ptr MediaSource::createFromMP4(const string &schema, const string &vhost, const string &app, const string &stream, const string &file_path , bool check_app){
    GET_CONFIG(string, appName, Record::kAppName);
    if (check_app && app != appName) {
        return nullptr;
    }
#ifdef ENABLE_MP4
    try {
        MP4Reader::Ptr pReader(new MP4Reader(vhost, app, stream, file_path));
        pReader->startReadMP4();
        return MediaSource::find(schema, vhost, app, stream);
    } catch (std::exception &ex) {
        WarnL << ex.what();
        return nullptr;
    }
#else
    WarnL << "创建MP4点播失败，请编译时打开\"ENABLE_MP4\"选项";
    return nullptr;
#endif //ENABLE_MP4
}

/////////////////////////////////////MediaSourceEvent//////////////////////////////////////

void MediaSourceEvent::onNoneReader(MediaSource &sender){
    GET_CONFIG(string, record_app, Record::kAppName);
    GET_CONFIG(int, stream_none_reader_delay, General::kStreamNoneReaderDelayMS);

    //如果mp4点播, 无人观看时我们强制关闭点播
    bool is_mp4_vod = sender.getApp() == record_app;

    //没有任何人观看该视频源，表明该源可以关闭了
    weak_ptr<MediaSource> weakSender = sender.shared_from_this();
    _async_close_timer = std::make_shared<Timer>(stream_none_reader_delay / 1000.0, [weakSender,is_mp4_vod]() {
        auto strongSender = weakSender.lock();
        if (!strongSender) {
            //对象已经销毁
            return false;
        }

        if (strongSender->totalReaderCount() != 0) {
            //还有人消费
            return false;
        }

        if(!is_mp4_vod){
            //直播时触发无人观看事件，让开发者自行选择是否关闭
            WarnL << "无人观看事件:"
                  << strongSender->getSchema() << "/"
                  << strongSender->getVhost() << "/"
                  << strongSender->getApp() << "/"
                  << strongSender->getId();
            NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastStreamNoneReader, *strongSender);
        }else{
            //这个是mp4点播，我们自动关闭
            WarnL << "MP4点播无人观看,自动关闭:"
                  << strongSender->getSchema() << "/"
                  << strongSender->getVhost() << "/"
                  << strongSender->getApp() << "/"
                  << strongSender->getId();
            strongSender->close(false);
        }

        return false;
    }, nullptr);
}

bool MediaSourceEventInterceptor::seekTo(MediaSource &sender, uint32_t stamp) {
    auto listener = _listener.lock();
    if (!listener) {
        return false;
    }
    return listener->seekTo(sender, stamp);
}

bool MediaSourceEventInterceptor::close(MediaSource &sender, bool force) {
    auto listener = _listener.lock();
    if (!listener) {
        return false;
    }
    return listener->close(sender, force);
}

int MediaSourceEventInterceptor::totalReaderCount(MediaSource &sender) {
    auto listener = _listener.lock();
    if (!listener) {
        return sender.readerCount();
    }
    return listener->totalReaderCount(sender);
}

void MediaSourceEventInterceptor::onNoneReader(MediaSource &sender) {
    auto listener = _listener.lock();
    if (!listener) {
        MediaSourceEvent::onNoneReader(sender);
        return;
    }
    listener->onNoneReader(sender);
}

void MediaSourceEventInterceptor::onRegist(MediaSource &sender, bool regist) {
    auto listener = _listener.lock();
    if (listener) {
        listener->onRegist(sender, regist);
    }
}

bool MediaSourceEventInterceptor::setupRecord(MediaSource &sender, Recorder::type type, bool start, const string &custom_path) {
    auto listener = _listener.lock();
    if (!listener) {
        return false;
    }
    return listener->setupRecord(sender, type, start, custom_path);
}

bool MediaSourceEventInterceptor::isRecording(MediaSource &sender, Recorder::type type) {
    auto listener = _listener.lock();
    if (!listener) {
        return false;
    }
    return listener->isRecording(sender, type);
}

vector<Track::Ptr> MediaSourceEventInterceptor::getTracks(MediaSource &sender, bool trackReady) const {
    auto listener = _listener.lock();
    if (!listener) {
        return vector<Track::Ptr>();
    }
    return listener->getTracks(sender, trackReady);
}

void MediaSourceEventInterceptor::startSendRtp(MediaSource &sender, const string &dst_url, uint16_t dst_port, uint32_t ssrc, bool is_udp, const function<void(const SockException &ex)> &cb){
    auto listener = _listener.lock();
    if (listener) {
        listener->startSendRtp(sender, dst_url, dst_port, ssrc, is_udp, cb);
    } else {
        MediaSourceEvent::startSendRtp(sender, dst_url, dst_port, ssrc, is_udp, cb);
    }
}

bool MediaSourceEventInterceptor::stopSendRtp(MediaSource &sender){
    auto listener = _listener.lock();
    if (listener) {
        return listener->stopSendRtp(sender);
    }
    return false;
}

/////////////////////////////////////FlushPolicy//////////////////////////////////////

static bool isFlushAble_default(bool is_video, uint32_t last_stamp, uint32_t new_stamp, int cache_size) {
    if (new_stamp + 500 < last_stamp) {
        //时间戳回退比较大(可能seek中)，由于rtp中时间戳是pts，是可能存在一定程度的回退的
        return true;
    }

    //时间戳发送变化或者缓存超过1024个,sendmsg接口一般最多只能发送1024个数据包
    return last_stamp != new_stamp || cache_size >= 1024;
}

static bool isFlushAble_merge(bool is_video, uint32_t last_stamp, uint32_t new_stamp, int cache_size, int merge_ms) {
    if (new_stamp + 500 < last_stamp) {
        //时间戳回退比较大(可能seek中)，由于rtp中时间戳是pts，是可能存在一定程度的回退的
        return true;
    }

    if (new_stamp > last_stamp + merge_ms) {
        //时间戳增量超过合并写阈值
        return true;
    }

    //缓存数超过1024个,这个逻辑用于避免时间戳异常的流导致的内存暴增问题
    //而且sendmsg接口一般最多只能发送1024个数据包
    return cache_size >= 1024;
}

bool FlushPolicy::isFlushAble(bool is_video, bool is_key, uint32_t new_stamp, int cache_size) {
    bool flush_flag = false;
    if (is_key && is_video) {
        //遇到关键帧flush掉前面的数据，确保关键帧为该组数据的第一帧，确保GOP缓存有效
        flush_flag = true;
    } else {
        GET_CONFIG(int, mergeWriteMS, General::kMergeWriteMS);
        if (mergeWriteMS <= 0) {
            //关闭了合并写或者合并写阈值小于等于0
            flush_flag = isFlushAble_default(is_video, _last_stamp[is_video], new_stamp, cache_size);
        } else {
            flush_flag = isFlushAble_merge(is_video, _last_stamp[is_video], new_stamp, cache_size, mergeWriteMS);
        }
    }

    if (flush_flag) {
        _last_stamp[is_video] = new_stamp;
    }
    return flush_flag;
}

} /* namespace mediakit */