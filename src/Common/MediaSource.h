﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MEDIASOURCE_H
#define ZLMEDIAKIT_MEDIASOURCE_H

#include <string>
#include <atomic>
#include <memory>
#include <functional>
#include "Util/mini.h"
#include "Network/Socket.h"
#include "Extension/Track.h"
#include "Record/Recorder.h"

namespace toolkit {
class Session;
} // namespace toolkit

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
class RtpProcess;
class MultiMediaSourceMuxer;
class MediaSourceEvent {
public:
    friend class MediaSource;

    class NotImplemented : public std::runtime_error {
    public:
        template<typename ...T>
        NotImplemented(T && ...args) : std::runtime_error(std::forward<T>(args)...) {}
    };

    virtual ~MediaSourceEvent() = default;

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
    // 获取MultiMediaSourceMuxer对象
    virtual std::shared_ptr<MultiMediaSourceMuxer> getMuxer(MediaSource &sender) const { return nullptr; }
    // 获取RtpProcess对象
    virtual std::shared_ptr<RtpProcess> getRtpProcess(MediaSource &sender) const { return nullptr; }

    class SendRtpArgs {
    public:
        enum Type { kRtpRAW = 0, kRtpPS = 1, kRtpTS = 2 };
        // 是否采用udp方式发送rtp
        bool is_udp = true;
        // rtp类型
        Type type = kRtpPS;
        //发送es流时指定是否只发送纯音频流
        bool only_audio = false;
        //tcp被动方式
        bool passive = false;
        // rtp payload type
        uint8_t pt = 96;
        //是否支持同ssrc多服务器发送
        bool ssrc_multi_send = false;
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
        //tcp被动发送服务器延时关闭事件，单位毫秒；设置为0时，则使用默认值5000ms
        uint32_t tcp_passive_close_delay_ms = 0;
        //udp 发送时，rr rtcp包接收超时时间，单位毫秒
        uint32_t rtcp_timeout_ms = 30 * 1000;
        //udp 发送时，发送sr rtcp包间隔，单位毫秒
        uint32_t rtcp_send_interval_ms = 5 * 1000;

        //发送rtp同时接收，一般用于双向语言对讲, 如果不为空，说明开启接收
        std::string recv_stream_id;
    };

    // 开始发送ps-rtp
    virtual void startSendRtp(MediaSource &sender, const SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) { cb(0, toolkit::SockException(toolkit::Err_other, "not implemented"));};
    // 停止发送ps-rtp
    virtual bool stopSendRtp(MediaSource &sender, const std::string &ssrc) {return false; }

private:
    toolkit::Timer::Ptr _async_close_timer;
};


template <typename MAP, typename KEY, typename TYPE>
static void getArgsValue(const MAP &allArgs, const KEY &key, TYPE &value) {
    auto val = ((MAP &)allArgs)[key];
    if (!val.empty()) {
        value = (TYPE)val;
    }
}

template <typename KEY, typename TYPE>
static void getArgsValue(const toolkit::mINI &allArgs, const KEY &key, TYPE &value) {
    auto it = allArgs.find(key);
    if (it != allArgs.end()) {
        value = (TYPE)it->second;
    }
}

class ProtocolOption {
public:
    ProtocolOption();

    enum {
        kModifyStampOff = 0, // 采用源视频流绝对时间戳，不做任何改变
        kModifyStampSystem = 1, // 采用zlmediakit接收数据时的系统时间戳(有平滑处理)
        kModifyStampRelative = 2 // 采用源视频流时间戳相对时间戳(增长量)，有做时间戳跳跃和回退矫正
    };
    // 时间戳类型
    int modify_stamp;

    //转协议是否开启音频
    bool enable_audio;
    //添加静音音频，在关闭音频时，此开关无效
    bool add_mute_audio;
    // 无人观看时，是否直接关闭(而不是通过on_none_reader hook返回close)
    // 此配置置1时，此流如果无人观看，将不触发on_none_reader hook回调，
    // 而是将直接关闭流
    bool auto_close;

    //断连续推延时，单位毫秒，默认采用配置文件
    uint32_t continue_push_ms;

    // 平滑发送定时器间隔，单位毫秒，置0则关闭；开启后影响cpu性能同时增加内存
    // 该配置开启后可以解决一些流发送不平滑导致zlmediakit转发也不平滑的问题
    uint32_t paced_sender_ms;

    //是否开启转换为hls(mpegts)
    bool enable_hls;
    //是否开启转换为hls(fmp4)
    bool enable_hls_fmp4;
    //是否开启MP4录制
    bool enable_mp4;
    //是否开启转换为rtsp/webrtc
    bool enable_rtsp;
    //是否开启转换为rtmp/flv
    bool enable_rtmp;
    //是否开启转换为http-ts/ws-ts
    bool enable_ts;
    //是否开启转换为http-fmp4/ws-fmp4
    bool enable_fmp4;

    // hls协议是否按需生成，如果hls.segNum配置为0(意味着hls录制)，那么hls将一直生成(不管此开关)
    bool hls_demand;
    // rtsp[s]协议是否按需生成
    bool rtsp_demand;
    // rtmp[s]、http[s]-flv、ws[s]-flv协议是否按需生成
    bool rtmp_demand;
    // http[s]-ts协议是否按需生成
    bool ts_demand;
    // http[s]-fmp4、ws[s]-fmp4协议是否按需生成
    bool fmp4_demand;

    //是否将mp4录制当做观看者
    bool mp4_as_player;
    //mp4切片大小，单位秒
    size_t mp4_max_second;
    //mp4录制保存路径
    std::string mp4_save_path;

    //hls录制保存路径
    std::string hls_save_path;

    // 支持通过on_publish返回值替换stream_id
    std::string stream_replace;

    // 最大track数
    size_t max_track = 2;

    template <typename MAP>
    ProtocolOption(const MAP &allArgs) : ProtocolOption() {
        load(allArgs);
    }

    template <typename MAP>
    void load(const MAP &allArgs) {
#define GET_OPT_VALUE(key) getArgsValue(allArgs, #key, key)
        GET_OPT_VALUE(modify_stamp);
        GET_OPT_VALUE(enable_audio);
        GET_OPT_VALUE(add_mute_audio);
        GET_OPT_VALUE(auto_close);
        GET_OPT_VALUE(continue_push_ms);
        GET_OPT_VALUE(paced_sender_ms);

        GET_OPT_VALUE(enable_hls);
        GET_OPT_VALUE(enable_hls_fmp4);
        GET_OPT_VALUE(enable_mp4);
        GET_OPT_VALUE(enable_rtsp);
        GET_OPT_VALUE(enable_rtmp);
        GET_OPT_VALUE(enable_ts);
        GET_OPT_VALUE(enable_fmp4);

        GET_OPT_VALUE(hls_demand);
        GET_OPT_VALUE(rtsp_demand);
        GET_OPT_VALUE(rtmp_demand);
        GET_OPT_VALUE(ts_demand);
        GET_OPT_VALUE(fmp4_demand);

        GET_OPT_VALUE(mp4_max_second);
        GET_OPT_VALUE(mp4_as_player);
        GET_OPT_VALUE(mp4_save_path);

        GET_OPT_VALUE(hls_save_path);
        GET_OPT_VALUE(stream_replace);
        GET_OPT_VALUE(max_track);
    }
};

//该对象用于拦截感兴趣的MediaSourceEvent事件
class MediaSourceEventInterceptor : public MediaSourceEvent {
public:
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
    std::shared_ptr<MultiMediaSourceMuxer> getMuxer(MediaSource &sender) const override;
    std::shared_ptr<RtpProcess> getRtpProcess(MediaSource &sender) const override;

private:
    std::weak_ptr<MediaSourceEvent> _listener;
};

/**
 * 解析url获取媒体相关信息
 */
class MediaInfo: public MediaTuple {
public:
    MediaInfo() = default;
    MediaInfo(const std::string &url) { parse(url); }
    void parse(const std::string &url);
    std::string getUrl() const { return schema + "://" + shortUrl(); }

public:
    uint16_t port = 0;
    std::string full_url;
    std::string schema;
    std::string host;
};

bool equalMediaTuple(const MediaTuple& a, const MediaTuple& b);

/**
 * 媒体源，任何rtsp/rtmp的直播流都源自该对象
 */
class MediaSource: public TrackSource, public std::enable_shared_from_this<MediaSource> {
public:
    static MediaSource& NullMediaSource();
    using Ptr = std::shared_ptr<MediaSource>;

    MediaSource(const std::string &schema, const MediaTuple& tuple);
    virtual ~MediaSource();

    ////////////////获取MediaSource相关信息////////////////

    // 获取协议类型
    const std::string& getSchema() const {
        return _schema;
    }

    const MediaTuple& getMediaTuple() const {
        return _tuple;
    }

    std::string getUrl() const { return _schema + "://" + _tuple.shortUrl(); }

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
    uint64_t getCreateStamp() const { return _create_stamp; }
    // 获取流上线时间，单位秒
    uint64_t getAliveSecond() const;

    ////////////////MediaSourceEvent相关接口实现////////////////

    // 设置监听者
    virtual void setListener(const std::weak_ptr<MediaSourceEvent> &listener);
    // 获取监听者
    std::weak_ptr<MediaSourceEvent> getListener() const;

    // 本协议获取观看者个数，可能返回本协议的观看人数，也可能返回总人数
    virtual int readerCount() = 0;
    // 观看者个数，包括(hls/rtsp/rtmp)
    virtual int totalReaderCount();
    // 获取播放器列表
    virtual void getPlayerList(const std::function<void(const std::list<toolkit::Any> &info_list)> &cb,
                               const std::function<toolkit::Any(toolkit::Any &&info)> &on_change) {
        assert(cb);
        cb(std::list<toolkit::Any>());
    }

    virtual bool broadcastMessage(const toolkit::Any &data) { return false; }

    // 获取媒体源类型
    MediaOriginType getOriginType() const;
    // 获取媒体源url或者文件路径
    std::string getOriginUrl() const;
    // 获取媒体源客户端相关信息
    std::shared_ptr<toolkit::SockInfo> getOriginSock() const;

    // 拖动进度条
    bool seekTo(uint32_t stamp);
    // 暂停
    bool pause(bool pause);
    // 倍数播放
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
    // 获取MultiMediaSourceMuxer对象
    std::shared_ptr<MultiMediaSourceMuxer> getMuxer() const;
    // 获取RtpProcess对象
    std::shared_ptr<RtpProcess> getRtpProcess() const;

    ////////////////static方法，查找或生成MediaSource////////////////

    // 同步查找流
    static Ptr find(const std::string &schema, const std::string &vhost, const std::string &app, const std::string &id, bool from_mp4 = false);
    static Ptr find(const MediaInfo &info, bool from_mp4 = false) {
        return find(info.schema, info.vhost, info.app, info.stream, from_mp4);
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
    // 媒体注销
    bool unregist();
    // 触发媒体事件
    void emitEvent(bool regist);

protected:
    toolkit::BytesSpeed _speed[TrackMax];
    MediaTuple _tuple;

private:
    std::atomic_flag _owned { false };
    time_t _create_stamp;
    toolkit::Ticker _ticker;
    std::string _schema;
    std::weak_ptr<MediaSourceEvent> _listener;
    // 对象个数统计
    toolkit::ObjectStatistic<MediaSource> _statistic;
};

} /* namespace mediakit */
#endif //ZLMEDIAKIT_MEDIASOURCE_H
