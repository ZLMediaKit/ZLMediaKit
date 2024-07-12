﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#include <mutex>
#include "Util/util.h"
#include "Util/NoticeCenter.h"
#include "Network/sockutil.h"
#include "Network/Session.h"
#include "MediaSource.h"
#include "Common/config.h"
#include "Common/Parser.h"
#include "Common/MultiMediaSourceMuxer.h"
#include "Record/MP4Reader.h"
#include "PacketCache.h"

using namespace std;
using namespace toolkit;

namespace toolkit {
    StatisticImp(mediakit::MediaSource);
}

namespace mediakit {

static recursive_mutex s_media_source_mtx;
using StreamMap = unordered_map<string/*strema_id*/, weak_ptr<MediaSource> >;
using AppStreamMap = unordered_map<string/*app*/, StreamMap>;
using VhostAppStreamMap = unordered_map<string/*vhost*/, AppStreamMap>;
using SchemaVhostAppStreamMap = unordered_map<string/*schema*/, VhostAppStreamMap>;
static SchemaVhostAppStreamMap s_media_source_map;

string getOriginTypeString(MediaOriginType type){
#define SWITCH_CASE(type) case MediaOriginType::type : return #type
    switch (type) {
        SWITCH_CASE(unknown);
        SWITCH_CASE(rtmp_push);
        SWITCH_CASE(rtsp_push);
        SWITCH_CASE(rtp_push);
        SWITCH_CASE(pull);
        SWITCH_CASE(ffmpeg_pull);
        SWITCH_CASE(mp4_vod);
        SWITCH_CASE(device_chn);
        SWITCH_CASE(rtc_push);
        SWITCH_CASE(srt_push);
        default : return "unknown";
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolOption::ProtocolOption() {
    mINI ini;
    auto &config = mINI::Instance();
    static auto sz = strlen(Protocol::kFieldName);
    for (auto it = config.lower_bound(Protocol::kFieldName); it != config.end() && start_with(it->first, Protocol::kFieldName); ++it) {
        ini.emplace(it->first.substr(sz), it->second);
    }
    load(ini);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MediaSourceNull : public MediaSource {
    MediaSourceNull() : MediaSource("schema", MediaTuple{"vhost", "app", "stream", ""}) {};
    int readerCount() override { return 0; }
};

MediaSource &MediaSource::NullMediaSource() {
    static std::shared_ptr<MediaSource> s_null = std::make_shared<MediaSourceNull>();
    return *s_null;
}

MediaSource::MediaSource(const string &schema, const MediaTuple& tuple): _tuple(tuple) {
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    if (!enableVhost || _tuple.vhost.empty()) {
        _tuple.vhost = DEFAULT_VHOST;
    }
    _schema = schema;
    _create_stamp = time(NULL);
}

MediaSource::~MediaSource() {
    try {
        unregist();
    } catch (std::exception &ex) {
        WarnL << "Exception occurred: " << ex.what();
    }
}

std::shared_ptr<void> MediaSource::getOwnership() {
    if (_owned.test_and_set()) {
        //已经被所有
        return nullptr;
    }
    weak_ptr<MediaSource> weak_self = shared_from_this();
    //确保返回的Ownership智能指针不为空，0x01无实际意义
    return std::shared_ptr<void>((void *) 0x01, [weak_self](void *ptr) {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->_owned.clear();
        }
    });
}

int MediaSource::getBytesSpeed(TrackType type){
    if(type == TrackInvalid || type == TrackMax){
        return _speed[TrackVideo].getSpeed() + _speed[TrackAudio].getSpeed();
    }
    return _speed[type].getSpeed();
}

uint64_t MediaSource::getAliveSecond() const {
    //使用Ticker对象获取存活时间的目的是防止修改系统时间导致回退
    return _ticker.createdTime() / 1000;
}

vector<Track::Ptr> MediaSource::getTracks(bool ready) const {
    auto listener = _listener.lock();
    if(!listener){
        return vector<Track::Ptr>();
    }
    return listener->getMediaTracks(const_cast<MediaSource &>(*this), ready);
}

void MediaSource::setListener(const std::weak_ptr<MediaSourceEvent> &listener){
    _listener = listener;
}

std::weak_ptr<MediaSourceEvent> MediaSource::getListener() const {
    return _listener;
}

int MediaSource::totalReaderCount(){
    auto listener = _listener.lock();
    if(!listener){
        return readerCount();
    }
    return listener->totalReaderCount(*this);
}

MediaOriginType MediaSource::getOriginType() const {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaOriginType::unknown;
    }
    return listener->getOriginType(const_cast<MediaSource &>(*this));
}

string MediaSource::getOriginUrl() const {
    auto listener = _listener.lock();
    if (!listener) {
        return getUrl();
    }
    auto ret = listener->getOriginUrl(const_cast<MediaSource &>(*this));
    if (!ret.empty()) {
        return ret;
    }
    return getUrl();
}

std::shared_ptr<SockInfo> MediaSource::getOriginSock() const {
    auto listener = _listener.lock();
    if (!listener) {
        return nullptr;
    }
    return listener->getOriginSock(const_cast<MediaSource &>(*this));
}

bool MediaSource::seekTo(uint32_t stamp) {
    auto listener = _listener.lock();
    if(!listener){
        return false;
    }
    return listener->seekTo(*this, stamp);
}

bool MediaSource::pause(bool pause) {
    auto listener = _listener.lock();
    if (!listener) {
        return false;
    }
    return listener->pause(*this, pause);
}

bool MediaSource::speed(float speed) {
    auto listener = _listener.lock();
    if (!listener) {
        return false;
    }
    return listener->speed(*this, speed);
}

bool MediaSource::close(bool force) {
    auto listener = _listener.lock();
    if (!listener) {
        return false;
    }
    if (!force && totalReaderCount()) {
        //有人观看，不强制关闭
        return false;
    }
    return listener->close(*this);
}

float MediaSource::getLossRate(mediakit::TrackType type) {
    auto listener = _listener.lock();
    if (!listener) {
        return -1;
    }
    return listener->getLossRate(*this, type);
}

toolkit::EventPoller::Ptr MediaSource::getOwnerPoller() {
    toolkit::EventPoller::Ptr ret;
    auto listener = _listener.lock();
    if (listener) {
        return listener->getOwnerPoller(*this);
    }
    throw std::runtime_error(toolkit::demangle(typeid(*this).name()) + "::getOwnerPoller failed: " + getUrl());
}

std::shared_ptr<MultiMediaSourceMuxer> MediaSource::getMuxer() const {
    auto listener = _listener.lock();
    return listener ? listener->getMuxer(const_cast<MediaSource&>(*this)) : nullptr;
}

std::shared_ptr<RtpProcess> MediaSource::getRtpProcess() const {
    auto listener = _listener.lock();
    return listener ? listener->getRtpProcess(const_cast<MediaSource&>(*this)) : nullptr;
}

void MediaSource::onReaderChanged(int size) {
    try {
        weak_ptr<MediaSource> weak_self = shared_from_this();
        getOwnerPoller()->async([weak_self, size]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            auto listener = strong_self->_listener.lock();
            if (listener) {
                listener->onReaderChanged(*strong_self, size);
            }
        });
    } catch (MediaSourceEvent::NotImplemented &ex) {
        // 未实现接口，应该打印异常
        WarnL << ex.what();
    } catch (...) {
        // getOwnerPoller()接口抛异常机制应该只对外不对内
        // 所以listener已经销毁导致获取归属线程失败的异常直接忽略
    }
}

bool MediaSource::setupRecord(Recorder::type type, bool start, const string &custom_path, size_t max_second){
    auto listener = _listener.lock();
    if (!listener) {
        WarnL << "未设置MediaSource的事件监听者，setupRecord失败:" << getUrl();
        return false;
    }
    return listener->setupRecord(*this, type, start, custom_path, max_second);
}

bool MediaSource::isRecording(Recorder::type type){
    auto listener = _listener.lock();
    if(!listener){
        return false;
    }
    return listener->isRecording(*this, type);
}

void MediaSource::startSendRtp(const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) {
    auto listener = _listener.lock();
    if (!listener) {
        cb(0, SockException(Err_other, "尚未设置事件监听器"));
        return;
    }
    return listener->startSendRtp(*this, args, cb);
}

bool MediaSource::stopSendRtp(const string &ssrc) {
    auto listener = _listener.lock();
    if (!listener) {
        return false;
    }
    return listener->stopSendRtp(*this, ssrc);
}

template<typename MAP, typename LIST, typename First, typename ...KeyTypes>
static void for_each_media_l(const MAP &map, LIST &list, const First &first, const KeyTypes &...keys) {
    if (first.empty()) {
        for (auto &pr : map) {
            for_each_media_l(pr.second, list, keys...);
        }
        return;
    }
    auto it = map.find(first);
    if (it != map.end()) {
        for_each_media_l(it->second, list, keys...);
    }
}

template<typename LIST, typename Ptr>
static void emplace_back(LIST &list, const Ptr &ptr) {
    auto src = ptr.lock();
    if (src) {
        list.emplace_back(std::move(src));
    }
}

template<typename MAP, typename LIST, typename First>
static void for_each_media_l(const MAP &map, LIST &list, const First &first) {
    if (first.empty()) {
        for (auto &pr : map) {
            emplace_back(list, pr.second);
        }
        return;
    }
    auto it = map.find(first);
    if (it != map.end()) {
        emplace_back(list, it->second);
    }
}

void MediaSource::for_each_media(const function<void(const Ptr &src)> &cb,
                                 const string &schema,
                                 const string &vhost,
                                 const string &app,
                                 const string &stream) {
    deque<Ptr> src_list;
    {
        lock_guard<recursive_mutex> lock(s_media_source_mtx);
        for_each_media_l(s_media_source_map, src_list, schema, vhost, app, stream);
    }
    for (auto &src : src_list) {
        cb(src);
    }
}

static MediaSource::Ptr find_l(const string &schema, const string &vhost_in, const string &app, const string &id, bool from_mp4) {
    string vhost = vhost_in;
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    if(vhost.empty() || !enableVhost){
        vhost = DEFAULT_VHOST;
    }

    if (app.empty() || id.empty()) {
        //如果未指定app与stream id，那么就是遍历而非查找，所以应该返回查找失败
        return nullptr;
    }

    MediaSource::Ptr ret;
    MediaSource::for_each_media([&](const MediaSource::Ptr &src) { ret = std::move(const_cast<MediaSource::Ptr &>(src)); }, schema, vhost, app, id);

    if(!ret && from_mp4 && schema != HLS_SCHEMA){
        //未找到媒体源，则读取mp4创建一个
        //播放hls不触发mp4点播(因为HLS也可以用于录像，不是纯粹的直播)
        ret = MediaSource::createFromMP4(schema, vhost, app, id);
    }
    return ret;
}

static void findAsync_l(const MediaInfo &info, const std::shared_ptr<Session> &session, bool retry,
                        const function<void(const MediaSource::Ptr &src)> &cb){
    auto src = find_l(info.schema, info.vhost, info.app, info.stream, true);
    if (src || !retry) {
        cb(src);
        return;
    }

    GET_CONFIG(int, maxWaitMS, General::kMaxStreamWaitTimeMS);
    void *listener_tag = session.get();
    auto poller = session->getPoller();
    std::shared_ptr<atomic_flag> invoked(new atomic_flag{false});
    auto cb_once = [cb, invoked](const MediaSource::Ptr &src) {
        if (invoked->test_and_set()) {
            //回调已经执行过了
            return;
        }
        cb(src);
    };

    auto on_timeout = poller->doDelayTask(maxWaitMS, [cb_once, listener_tag]() {
        // 最多等待一定时间，如在这个时间内，流还未注册上，则返回空
        NoticeCenter::Instance().delListener(listener_tag, Broadcast::kBroadcastMediaChanged);
        cb_once(nullptr);
        return 0;
    });

    auto cancel_all = [on_timeout, listener_tag]() {
        //取消延时任务，防止多次回调
        on_timeout->cancel();
        //取消媒体注册事件监听
        NoticeCenter::Instance().delListener(listener_tag, Broadcast::kBroadcastMediaChanged);
    };

    weak_ptr<Session> weak_session = session;
    auto on_register = [weak_session, info, cb_once, cancel_all, poller](BroadcastMediaChangedArgs) {
        if (!bRegist ||
            sender.getSchema() != info.schema ||
            !equalMediaTuple(sender.getMediaTuple(), info)) {
            //不是自己感兴趣的事件，忽略之
            return;
        }

        poller->async([weak_session, cancel_all, info, cb_once]() {
            cancel_all();
            if (auto strong_session = weak_session.lock()) {
                //播发器请求的流终于注册上了，切换到自己的线程再回复
                DebugL << "收到媒体注册事件,回复播放器:" << info.getUrl();
                //再找一遍媒体源，一般能找到
                findAsync_l(info, strong_session, false, cb_once);
            }
        }, false);
    };

    //监听媒体注册事件
    NoticeCenter::Instance().addListener(listener_tag, Broadcast::kBroadcastMediaChanged, on_register);

    function<void()> close_player = [cb_once, cancel_all, poller]() {
        poller->async([cancel_all, cb_once]() {
            cancel_all();
            //告诉播放器，流不存在，这样会立即断开播放器
            cb_once(nullptr);
        });
    };
    //广播未找到流,此时可以立即去拉流，这样还来得及
    NOTICE_EMIT(BroadcastNotFoundStreamArgs, Broadcast::kBroadcastNotFoundStream, info, *session, close_player);
}

void MediaSource::findAsync(const MediaInfo &info, const std::shared_ptr<Session> &session, const function<void (const Ptr &)> &cb) {
    return findAsync_l(info, session, true, cb);
}

MediaSource::Ptr MediaSource::find(const string &schema, const string &vhost, const string &app, const string &id, bool from_mp4) {
    return find_l(schema, vhost, app, id, from_mp4);
}

MediaSource::Ptr MediaSource::find(const string &vhost, const string &app, const string &stream_id, bool from_mp4) {
    auto src = MediaSource::find(RTMP_SCHEMA, vhost, app, stream_id, from_mp4);
    if (src) {
        return src;
    }
    src = MediaSource::find(RTSP_SCHEMA, vhost, app, stream_id, from_mp4);
    if (src) {
        return src;
    }
    src = MediaSource::find(TS_SCHEMA, vhost, app, stream_id, from_mp4);
    if (src) {
        return src;
    }
    src = MediaSource::find(FMP4_SCHEMA, vhost, app, stream_id, from_mp4);
    if (src) {
        return src;
    }
    src = MediaSource::find(HLS_SCHEMA, vhost, app, stream_id, from_mp4);
    if (src) {
        return src;
    }
    return MediaSource::find(HLS_FMP4_SCHEMA, vhost, app, stream_id, from_mp4);
}

void MediaSource::emitEvent(bool regist){
    auto listener = _listener.lock();
    if (listener) {
        //触发回调
        listener->onRegist(*this, regist);
    }
    //触发广播
    NOTICE_EMIT(BroadcastMediaChangedArgs, Broadcast::kBroadcastMediaChanged, regist, *this);
    InfoL << (regist ? "媒体注册:" : "媒体注销:") << getUrl();
}

void MediaSource::regist() {
    {
        //减小互斥锁临界区
        lock_guard<recursive_mutex> lock(s_media_source_mtx);
        auto &ref = s_media_source_map[_schema][_tuple.vhost][_tuple.app][_tuple.stream];
        auto src = ref.lock();
        if (src) {
            if (src.get() == this) {
                return;
            }
            //增加判断, 防止当前流已注册时再次注册
            throw std::invalid_argument("media source already existed:" + getUrl());
        }
        ref = shared_from_this();
    }
    emitEvent(true);
}

template<typename MAP, typename First, typename ...KeyTypes>
static bool erase_media_source(bool &hit, const MediaSource *thiz, MAP &map, const First &first, const KeyTypes &...keys) {
    auto it = map.find(first);
    if (it != map.end() && erase_media_source(hit, thiz, it->second, keys...)) {
        map.erase(it);
    }
    return map.empty();
}

template<typename MAP, typename First>
static bool erase_media_source(bool &hit, const MediaSource *thiz, MAP &map, const First &first) {
    auto it = map.find(first);
    if (it != map.end()) {
        auto src = it->second.lock();
        if (!src || src.get() == thiz) {
            //对象已经销毁或者对象就是自己，那么移除之
            map.erase(it);
            hit = true;
        }
    }
    return map.empty();
}

//反注册该源
bool MediaSource::unregist() {
    bool ret = false;
    {
        //减小互斥锁临界区
        lock_guard<recursive_mutex> lock(s_media_source_mtx);
        erase_media_source(ret, this, s_media_source_map, _schema, _tuple.vhost, _tuple.app, _tuple.stream);
    }

    if (ret) {
        emitEvent(false);
    }
    return ret;
}

bool equalMediaTuple(const MediaTuple& a, const MediaTuple& b) {
    return a.vhost == b.vhost && a.app == b.app && a.stream == b.stream;
}
/////////////////////////////////////MediaInfo//////////////////////////////////////

void MediaInfo::parse(const std::string &url_in){
    full_url = url_in;
    auto url = url_in;
    auto pos = url.find("?");
    if (pos != string::npos) {
        params = url.substr(pos + 1);
        url.erase(pos);
    }

    auto schema_pos = url.find("://");
    if (schema_pos != string::npos) {
        schema = url.substr(0, schema_pos);
    } else {
        schema_pos = -3;
    }
    auto split_vec = split(url.substr(schema_pos + 3), "/");
    if (split_vec.size() > 0) {
        splitUrl(split_vec[0], host, port);
        vhost = host;
         if (vhost == "localhost" || isIP(vhost.data())) {
            //如果访问的是localhost或ip，那么则为默认虚拟主机
            vhost = DEFAULT_VHOST;
        }
    }
    if (split_vec.size() > 1) {
        app = split_vec[1];
    }
    if (split_vec.size() > 2) {
        string stream_id;
        for (size_t i = 2; i < split_vec.size(); ++i) {
            stream_id.append(split_vec[i] + "/");
        }
        if (stream_id.back() == '/') {
            stream_id.pop_back();
        }
        stream = stream_id;
    }

    auto kv = Parser::parseArgs(params);
    auto it = kv.find(VHOST_KEY);
    if (it != kv.end()) {
        vhost = it->second;
    }

    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    if (!enableVhost || vhost.empty()) {
        //如果关闭虚拟主机或者虚拟主机为空，则设置虚拟主机为默认
        vhost = DEFAULT_VHOST;
    }
}

MediaSource::Ptr MediaSource::createFromMP4(const string &schema, const string &vhost, const string &app, const string &stream, const string &file_path , bool check_app){
    GET_CONFIG(string, appName, Record::kAppName);
    if (check_app && app != appName) {
        return nullptr;
    }
#ifdef ENABLE_MP4
    try {
        auto reader = std::make_shared<MP4Reader>(vhost, app, stream, file_path);
        reader->startReadMP4();
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

void MediaSourceEvent::onReaderChanged(MediaSource &sender, int size){
    GET_CONFIG(bool, enable, General::kBroadcastPlayerCountChanged);
    if (enable) {
        NOTICE_EMIT(BroadcastPlayerCountChangedArgs, Broadcast::kBroadcastPlayerCountChanged, sender.getMediaTuple(), sender.totalReaderCount());
    }
    if (size || sender.totalReaderCount()) {
        //还有人观看该视频，不触发关闭事件
        _async_close_timer = nullptr;
        return;
    }
    //没有任何人观看该视频源，表明该源可以关闭了
    GET_CONFIG(string, record_app, Record::kAppName);
    GET_CONFIG(int, stream_none_reader_delay, General::kStreamNoneReaderDelayMS);
    //如果mp4点播, 无人观看时我们强制关闭点播
    bool is_mp4_vod = sender.getMediaTuple().app == record_app;
    weak_ptr<MediaSource> weak_sender = sender.shared_from_this();

    _async_close_timer = std::make_shared<Timer>(stream_none_reader_delay / 1000.0f, [weak_sender, is_mp4_vod]() {
        auto strong_sender = weak_sender.lock();
        if (!strong_sender) {
            //对象已经销毁
            return false;
        }

        if (strong_sender->totalReaderCount()) {
            //还有人观看该视频，不触发关闭事件
            return false;
        }

        if (!is_mp4_vod) {
            auto muxer = strong_sender->getMuxer();
            if (muxer && muxer->getOption().auto_close) {
                // 此流被标记为无人观看自动关闭流
                WarnL << "Auto cloe stream when none reader: " << strong_sender->getUrl();
                strong_sender->close(false);
            } else {
                // 直播时触发无人观看事件，让开发者自行选择是否关闭
                NOTICE_EMIT(BroadcastStreamNoneReaderArgs, Broadcast::kBroadcastStreamNoneReader, *strong_sender);
            }
        } else {
            //这个是mp4点播，我们自动关闭
            WarnL << "MP4点播无人观看,自动关闭:" << strong_sender->getUrl();
            strong_sender->close(false);
        }
        return false;
    }, nullptr);
}

string MediaSourceEvent::getOriginUrl(MediaSource &sender) const {
    return sender.getUrl();
}

MediaOriginType MediaSourceEventInterceptor::getOriginType(MediaSource &sender) const {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::getOriginType(sender);
    }
    return listener->getOriginType(sender);
}

string MediaSourceEventInterceptor::getOriginUrl(MediaSource &sender) const {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::getOriginUrl(sender);
    }
    auto ret = listener->getOriginUrl(sender);
    if (!ret.empty()) {
        return ret;
    }
    return MediaSourceEvent::getOriginUrl(sender);
}

std::shared_ptr<SockInfo> MediaSourceEventInterceptor::getOriginSock(MediaSource &sender) const {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::getOriginSock(sender);
    }
    return listener->getOriginSock(sender);
}

bool MediaSourceEventInterceptor::seekTo(MediaSource &sender, uint32_t stamp) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::seekTo(sender, stamp);
    }
    return listener->seekTo(sender, stamp);
}

bool MediaSourceEventInterceptor::pause(MediaSource &sender, bool pause) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::pause(sender, pause);
    }
    return listener->pause(sender, pause);
}

bool MediaSourceEventInterceptor::speed(MediaSource &sender, float speed) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::speed(sender, speed);
    }
    return listener->speed(sender, speed);
}

bool MediaSourceEventInterceptor::close(MediaSource &sender) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::close(sender);
    }
    return listener->close(sender);
}

int MediaSourceEventInterceptor::totalReaderCount(MediaSource &sender) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::totalReaderCount(sender);
    }
    return listener->totalReaderCount(sender);
}

void MediaSourceEventInterceptor::onReaderChanged(MediaSource &sender, int size) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::onReaderChanged(sender, size);
    }
    listener->onReaderChanged(sender, size);
}

void MediaSourceEventInterceptor::onRegist(MediaSource &sender, bool regist) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::onRegist(sender, regist);
    }
    listener->onRegist(sender, regist);
}

float MediaSourceEventInterceptor::getLossRate(MediaSource &sender, TrackType type) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::getLossRate(sender, type);
    }
    return listener->getLossRate(sender, type);
}

toolkit::EventPoller::Ptr MediaSourceEventInterceptor::getOwnerPoller(MediaSource &sender) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::getOwnerPoller(sender);
    }
    return listener->getOwnerPoller(sender);
}

std::shared_ptr<MultiMediaSourceMuxer> MediaSourceEventInterceptor::getMuxer(MediaSource &sender) const {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::getMuxer(sender);
    }
    return listener->getMuxer(sender);
}

std::shared_ptr<RtpProcess> MediaSourceEventInterceptor::getRtpProcess(MediaSource &sender) const {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::getRtpProcess(sender);
    }
    return listener->getRtpProcess(sender);
}

bool MediaSourceEventInterceptor::setupRecord(MediaSource &sender, Recorder::type type, bool start, const string &custom_path, size_t max_second) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::setupRecord(sender, type, start, custom_path, max_second);
    }
    return listener->setupRecord(sender, type, start, custom_path, max_second);
}

bool MediaSourceEventInterceptor::isRecording(MediaSource &sender, Recorder::type type) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::isRecording(sender, type);
    }
    return listener->isRecording(sender, type);
}

vector<Track::Ptr> MediaSourceEventInterceptor::getMediaTracks(MediaSource &sender, bool trackReady) const {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::getMediaTracks(sender, trackReady);
    }
    return listener->getMediaTracks(sender, trackReady);
}

void MediaSourceEventInterceptor::startSendRtp(MediaSource &sender, const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::startSendRtp(sender, args, cb);
    }
    listener->startSendRtp(sender, args, cb);
}

bool MediaSourceEventInterceptor::stopSendRtp(MediaSource &sender, const string &ssrc) {
    auto listener = _listener.lock();
    if (!listener) {
        return MediaSourceEvent::stopSendRtp(sender, ssrc);
    }
    return listener->stopSendRtp(sender, ssrc);
}

void MediaSourceEventInterceptor::setDelegate(const std::weak_ptr<MediaSourceEvent> &listener) {
    if (listener.lock().get() == this) {
        throw std::invalid_argument("can not set self as a delegate");
    }
    _listener = listener;
}

std::shared_ptr<MediaSourceEvent> MediaSourceEventInterceptor::getDelegate() const {
    return _listener.lock();
}

/////////////////////////////////////FlushPolicy//////////////////////////////////////

static bool isFlushAble_default(bool is_video, uint64_t last_stamp, uint64_t new_stamp, size_t cache_size) {
    if (new_stamp + 500 < last_stamp) {
        //时间戳回退比较大(可能seek中)，由于rtp中时间戳是pts，是可能存在一定程度的回退的
        return true;
    }

    //时间戳发送变化或者缓存超过1024个,sendmsg接口一般最多只能发送1024个数据包
    return last_stamp != new_stamp || cache_size >= 1024;
}

static bool isFlushAble_merge(bool is_video, uint64_t last_stamp, uint64_t new_stamp, size_t cache_size, int merge_ms) {
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

bool FlushPolicy::isFlushAble(bool is_video, bool is_key, uint64_t new_stamp, size_t cache_size) {
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