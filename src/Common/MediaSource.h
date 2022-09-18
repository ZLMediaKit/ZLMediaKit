/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MEDIASOURCE_H
#define ZLMEDIAKIT_MEDIASOURCE_H

#include <mutex>
#include <string>
#include <atomic>
#include <memory>
#include <functional>
#include <unordered_map>
#include "Common/config.h"
#include "Common/Parser.h"
#include "Util/List.h"
#include "Network/Socket.h"
#include "Extension/Track.h"
#include "Record/Recorder.h"

namespace toolkit{
    class Session;
}// namespace toolkit

namespace mediakit {

enum class MediaOriginType : uint8_t {
    unknown = 0,
    rtmp_push ,
    rtsp_push,
    rtp_push,
    pull,
    ffmpeg_pull,
    mp4_vod,
    device_chn,
    rtc_push,
    srt_push
};

std::string getOriginTypeString(MediaOriginType type);

class MediaSource;
class MediaSourceEvent {
public:
    friend class MediaSource;

    class NotImplemented : public std::runtime_error {
    public:
        template<typename ...T>
        NotImplemented(T && ...args) : std::runtime_error(std::forward<T>(args)...) {}
        ~NotImplemented() override = default;
    };

    MediaSourceEvent(){};
    virtual ~MediaSourceEvent(){};

    // 获取媒体源类型
    virtual MediaOriginType getOriginType(MediaSource &sender) const { return MediaOriginType::unknown; }
    // 获取媒体源url或者文件路径
    virtual std::string getOriginUrl(MediaSource &sender) const;
    // 获取媒体源客户端相关信息
    virtual std::shared_ptr<toolkit::SockInfo> getOriginSock(MediaSource &sender) const { return nullptr; }

    // 通知拖动进度条
    virtual bool seekTo(MediaSource &sender, uint32_t stamp) { return false; }
    // 通知暂停或恢复
    virtual bool pause(MediaSource &sender, bool pause) { return false; }
    // 通知倍数
    virtual bool speed(MediaSource &sender, float speed) { return false; }
    // 通知其停止产生流
    virtual bool close(MediaSource &sender) { return false; }
    // 获取观看总人数，此函数一般强制重载
    virtual int totalReaderCount(MediaSource &sender) { throw NotImplemented(toolkit::demangle(typeid(*this).name()) + "::totalReaderCount not implemented"); }
    // 通知观看人数变化
    virtual void onReaderChanged(MediaSource &sender, int size);
    //流注册或注销事件
    virtual void onRegist(MediaSource &sender, bool regist) {}
    // 获取丢包率
    virtual float getLossRate(MediaSource &sender, TrackType type) { return -1; }
    // 获取所在线程, 此函数一般强制重载
    virtual toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) { throw NotImplemented(toolkit::demangle(typeid(*this).name()) + "::getOwnerPoller not implemented"); }

    ////////////////////////仅供MultiMediaSourceMuxer对象继承////////////////////////
    // 开启或关闭录制
    virtual bool setupRecord(MediaSource &sender, Recorder::type type, bool start, const std::string &custom_path, size_t max_second) { return false; };
    // 获取录制状态
    virtual bool isRecording(MediaSource &sender, Recorder::type type) { return false; }
    // 获取所有track相关信息
    virtual std::vector<Track::Ptr> getMediaTracks(MediaSource &sender, bool trackReady = true) const { return std::vector<Track::Ptr>(); };

    class SendRtpArgs {
    public:
        // 是否采用udp方式发送rtp
        bool is_udp = true;
        // rtp采用ps还是es方式
        bool use_ps = true;
        //发送es流时指定是否只发送纯音频流
        bool only_audio = true;
        //tcp被动方式
        bool passive = false;
        // rtp payload type
        uint8_t pt = 96;
        // 指定rtp ssrc
        std::string ssrc;
        // 指定本地发送端口
        uint16_t src_port = 0;
        // 发送目标端口
        uint16_t dst_port;
        // 发送目标主机地址，可以是ip或域名
        std::string dst_url;

        //udp发送时，是否开启rr rtcp接收超时判断
        bool udp_rtcp_timeout = false;
        //tcp被动发送服务器延时关闭事件，单位毫秒
        uint32_t tcp_passive_close_delay_ms = 5 * 1000;
        //udp 发送时，rr rtcp包接收超时时间，单位毫秒
        uint32_t rtcp_timeout_ms = 30 * 1000;
        //udp 发送时，发送sr rtcp包间隔，单位毫秒
        uint32_t rtcp_send_interval_ms = 5 * 1000;
    };

    // 开始发送ps-rtp
    virtual void startSendRtp(MediaSource &sender, const SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) { cb(0, toolkit::SockException(toolkit::Err_other, "not implemented"));};
    // 停止发送ps-rtp
    virtual bool stopSendRtp(MediaSource &sender, const std::string &ssrc) {return false; }

private:
    toolkit::Timer::Ptr _async_close_timer;
};

//该对象用于拦截感兴趣的MediaSourceEvent事件
class MediaSourceEventInterceptor : public MediaSourceEvent{
public:
    MediaSourceEventInterceptor(){}
    ~MediaSourceEventInterceptor() override {}

    void setDelegate(const std::weak_ptr<MediaSourceEvent> &listener);
    std::shared_ptr<MediaSourceEvent> getDelegate() const;

    MediaOriginType getOriginType(MediaSource &sender) const override;
    std::string getOriginUrl(MediaSource &sender) const override;
    std::shared_ptr<toolkit::SockInfo> getOriginSock(MediaSource &sender) const override;

    bool seekTo(MediaSource &sender, uint32_t stamp) override;
    bool pause(MediaSource &sender,  bool pause) override;
    bool speed(MediaSource &sender, float speed) override;
    bool close(MediaSource &sender) override;
    int totalReaderCount(MediaSource &sender) override;
    void onReaderChanged(MediaSource &sender, int size) override;
    void onRegist(MediaSource &sender, bool regist) override;
    bool setupRecord(MediaSource &sender, Recorder::type type, bool start, const std::string &custom_path, size_t max_second) override;
    bool isRecording(MediaSource &sender, Recorder::type type) override;
    std::vector<Track::Ptr> getMediaTracks(MediaSource &sender, bool trackReady = true) const override;
    void startSendRtp(MediaSource &sender, const SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) override;
    bool stopSendRtp(MediaSource &sender, const std::string &ssrc) override;
    float getLossRate(MediaSource &sender, TrackType type) override;
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;

private:
    std::weak_ptr<MediaSourceEvent> _listener;
};

/**
 * 解析url获取媒体相关信息
 */
class MediaInfo{
public:
    ~MediaInfo() {}
    MediaInfo() {}
    MediaInfo(const std::string &url) { parse(url); }
    void parse(const std::string &url);
    std::string shortUrl() const {
        return _vhost + "/" + _app + "/" + _streamid;
    }
    std::string getUrl() const {
        return _schema + "://" + shortUrl();
    }
public:
    std::string _full_url;
    std::string _schema;
    std::string _host;
    uint16_t _port = 0;
    std::string _vhost;
    std::string _app;
    std::string _streamid;
    std::string _param_strs;
};

/**
 * 媒体源，任何rtsp/rtmp的直播流都源自该对象
 */
class MediaSource: public TrackSource, public std::enable_shared_from_this<MediaSource> {
public:
    static MediaSource& NullMediaSource();
    using Ptr = std::shared_ptr<MediaSource>;

    MediaSource(const std::string &schema, const std::string &vhost, const std::string &app, const std::string &stream_id) ;
    virtual ~MediaSource();

    ////////////////获取MediaSource相关信息////////////////

    // 获取协议类型
    const std::string& getSchema() const;
    // 虚拟主机
    const std::string& getVhost() const;
    // 应用名
    const std::string& getApp() const;
    // 流id
    const std::string& getId() const;

    std::string shortUrl() const {
        return  _vhost + "/" + _app + "/" + _stream_id;
    }
    std::string getUrl() const {
        return _schema + "://" + shortUrl();
    }
    
    //获取对象所有权
    std::shared_ptr<void> getOwnership();

    // 获取所有Track
    std::vector<Track::Ptr> getTracks(bool ready = true) const override;

    // 获取流当前时间戳
    virtual uint32_t getTimeStamp(TrackType type) { return 0; };
    // 设置时间戳
    virtual void setTimeStamp(uint32_t stamp) {};

    // 获取数据速率，单位bytes/s
    int getBytesSpeed(TrackType type = TrackInvalid);
    // 获取流创建GMT unix时间戳，单位秒
    uint64_t getCreateStamp() const {return _create_stamp;}
    // 获取流上线时间，单位秒
    uint64_t getAliveSecond() const;

    ////////////////MediaSourceEvent相关接口实现////////////////

    // 设置监听者
    virtual void setListener(const std::weak_ptr<MediaSourceEvent> &listener);
    // 获取监听者
    std::weak_ptr<MediaSourceEvent> getListener(bool next = false) const;

    // 本协议获取观看者个数，可能返回本协议的观看人数，也可能返回总人数
    virtual int readerCount() = 0;
    // 观看者个数，包括(hls/rtsp/rtmp)
    virtual int totalReaderCount();
    // 获取播放器列表
    virtual void getPlayerList(const std::function<void(const std::list<std::shared_ptr<void>> &info_list)> &cb,
                               const std::function<std::shared_ptr<void>(std::shared_ptr<void> &&info)> &on_change) {
        assert(cb);
        cb(std::list<std::shared_ptr<void>>());
    }

    // 获取媒体源类型
    MediaOriginType getOriginType() const;
    // 获取媒体源url或者文件路径
    std::string getOriginUrl() const;
    // 获取媒体源客户端相关信息
    std::shared_ptr<toolkit::SockInfo> getOriginSock() const;

    // 拖动进度条
    bool seekTo(uint32_t stamp);
    //暂停
    bool pause(bool pause);
    //倍数播放
    bool speed(float speed);
    // 关闭该流
    bool close(bool force);
    // 该流观看人数变化
    void onReaderChanged(int size);
    // 开启或关闭录制
    bool setupRecord(Recorder::type type, bool start, const std::string &custom_path, size_t max_second);
    // 获取录制状态
    bool isRecording(Recorder::type type);
    // 开始发送ps-rtp
    void startSendRtp(const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb);
    // 停止发送ps-rtp
    bool stopSendRtp(const std::string &ssrc);
    // 获取丢包率
    float getLossRate(mediakit::TrackType type);
    // 获取所在线程
    toolkit::EventPoller::Ptr getOwnerPoller();

    ////////////////static方法，查找或生成MediaSource////////////////

    // 同步查找流
    static Ptr find(const std::string &schema, const std::string &vhost, const std::string &app, const std::string &id, bool from_mp4 = false);
    static Ptr find(const MediaInfo &info, bool from_mp4 = false) {
        return find(info._schema, info._vhost, info._app, info._streamid, from_mp4);
    }

    // 忽略schema，同步查找流，可能返回rtmp/rtsp/hls类型
    static Ptr find(const std::string &vhost, const std::string &app, const std::string &stream_id, bool from_mp4 = false);

    // 异步查找流
    static void findAsync(const MediaInfo &info, const std::shared_ptr<toolkit::Session> &session, const std::function<void(const Ptr &src)> &cb);
    // 遍历所有流
    static void for_each_media(const std::function<void(const Ptr &src)> &cb, const std::string &schema = "", const std::string &vhost = "", const std::string &app = "", const std::string &stream = "");
    // 从mp4文件生成MediaSource
    static MediaSource::Ptr createFromMP4(const std::string &schema, const std::string &vhost, const std::string &app, const std::string &stream, const std::string &file_path = "", bool check_app = true);

protected:
    //媒体注册
    void regist();

private:
    //媒体注销
    bool unregist();
    //触发媒体事件
    void emitEvent(bool regist);

protected:
    toolkit::BytesSpeed _speed[TrackMax];

private:
    std::atomic_flag _owned { false };
    time_t _create_stamp;
    toolkit::Ticker _ticker;
    std::string _schema;
    std::string _vhost;
    std::string _app;
    std::string _stream_id;
    std::weak_ptr<MediaSourceEvent> _listener;
    toolkit::EventPoller::Ptr _default_poller;
    //对象个数统计
    toolkit::ObjectStatistic<MediaSource> _statistic;
};

///缓存刷新策略类
class FlushPolicy {
public:
    FlushPolicy() = default;
    ~FlushPolicy() = default;

    bool isFlushAble(bool is_video, bool is_key, uint64_t new_stamp, size_t cache_size);

private:
    // 音视频的最后时间戳
    uint64_t _last_stamp[2] = {0, 0};
};

/// 合并写缓存模板
/// \tparam packet 包类型
/// \tparam policy 刷新缓存策略
/// \tparam packet_list 包缓存类型
template<typename packet, typename policy = FlushPolicy, typename packet_list = toolkit::List<std::shared_ptr<packet> > >
class PacketCache {
public:
    PacketCache(){
        _cache = std::make_shared<packet_list>();
    }

    virtual ~PacketCache() = default;

    void inputPacket(uint64_t stamp, bool is_video, std::shared_ptr<packet> pkt, bool key_pos) {
        bool flush = flushImmediatelyWhenCloseMerge();
        if (!flush && _policy.isFlushAble(is_video, key_pos, stamp, _cache->size())) {
            flushAll();
        }

        //追加数据到最后
        _cache->emplace_back(std::move(pkt));
        if (key_pos) {
            _key_pos = key_pos;
        }

        if (flush) {
            flushAll();
        }
    }

    virtual void clearCache() {
        _cache->clear();
    }

    virtual void onFlush(std::shared_ptr<packet_list>, bool key_pos) = 0;

private:
    void flushAll() {
        if (_cache->empty()) {
            return;
        }
        onFlush(std::move(_cache), _key_pos);
        _cache = std::make_shared<packet_list>();
        _key_pos = false;
    }

    bool flushImmediatelyWhenCloseMerge() {
        //一般的协议关闭合并写时，立即刷新缓存，这样可以减少一帧的延时，但是rtp例外
        //因为rtp的包很小，一个RtpPacket包中也不是完整的一帧图像，所以在关闭合并写时，
        //还是有必要缓冲一帧的rtp(也就是时间戳相同的rtp)再输出，这样虽然会增加一帧的延时
        //但是却对性能提升很大，这样做还是比较划算的

        GET_CONFIG(int, mergeWriteMS, General::kMergeWriteMS);
        return std::is_same<packet, RtpPacket>::value ? false : (mergeWriteMS <= 0);
    }

private:
    bool _key_pos = false;
    policy _policy;
    std::shared_ptr<packet_list> _cache;
};

} /* namespace mediakit */
#endif //ZLMEDIAKIT_MEDIASOURCE_H
