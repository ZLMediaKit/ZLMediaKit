/*
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

    // 获取媒体源类型  [AUTO-TRANSLATED:34290a69]
    // Get media source type
    virtual MediaOriginType getOriginType(MediaSource &sender) const { return MediaOriginType::unknown; }
    // 获取媒体源url或者文件路径  [AUTO-TRANSLATED:fa34d795]
    // Get media source url or file path
    virtual std::string getOriginUrl(MediaSource &sender) const;
    // 获取媒体源客户端相关信息  [AUTO-TRANSLATED:037ef910]
    // Get media source client related information
    virtual std::shared_ptr<toolkit::SockInfo> getOriginSock(MediaSource &sender) const { return nullptr; }

    // 通知拖动进度条  [AUTO-TRANSLATED:561b17f7]
    // Notify drag progress bar
    virtual bool seekTo(MediaSource &sender, uint32_t stamp) { return false; }
    // 通知暂停或恢复  [AUTO-TRANSLATED:ee3c219f]
    // Notify pause or resume
    virtual bool pause(MediaSource &sender, bool pause) { return false; }
    // 通知倍数  [AUTO-TRANSLATED:8f1dab15]
    // Notify multiple times
    virtual bool speed(MediaSource &sender, float speed) { return false; }
    // 通知其停止产生流  [AUTO-TRANSLATED:62c9022c]
    // Notify it to stop generating streams
    virtual bool close(MediaSource &sender) { return false; }
    // 获取观看总人数，此函数一般强制重载  [AUTO-TRANSLATED:1da20a10]
    // Get the total number of viewers, this function is generally forced to overload
    virtual int totalReaderCount(MediaSource &sender) { throw NotImplemented(toolkit::demangle(typeid(*this).name()) + "::totalReaderCount not implemented"); }
    // 通知观看人数变化  [AUTO-TRANSLATED:bad89528]
    // Notify the change in the number of viewers
    virtual void onReaderChanged(MediaSource &sender, int size);
    // 流注册或注销事件  [AUTO-TRANSLATED:2cac8178]
    // Stream registration or deregistration event
    virtual void onRegist(MediaSource &sender, bool regist) {}
    // 获取丢包率  [AUTO-TRANSLATED:ec61b378]
    // Get packet loss rate
    virtual float getLossRate(MediaSource &sender, TrackType type) { return -1; }
    // 获取所在线程, 此函数一般强制重载  [AUTO-TRANSLATED:71c99afb]
    // Get the current thread, this function is generally forced to overload
    virtual toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) { throw NotImplemented(toolkit::demangle(typeid(*this).name()) + "::getOwnerPoller not implemented"); }

    // //////////////////////仅供MultiMediaSourceMuxer对象继承////////////////////////  [AUTO-TRANSLATED:6e810d1f]
    // //////////////////////Only for MultiMediaSourceMuxer object inheritance////////////////////////
    // 开启或关闭录制  [AUTO-TRANSLATED:3817e390]
    // Start or stop recording
    virtual bool setupRecord(MediaSource &sender, Recorder::type type, bool start, const std::string &custom_path, size_t max_second) { return false; };
    // 获取录制状态  [AUTO-TRANSLATED:a0499880]
    // Get recording status
    virtual bool isRecording(MediaSource &sender, Recorder::type type) { return false; }
    // 获取所有track相关信息  [AUTO-TRANSLATED:2141be42]
    // Get all track related information
    virtual std::vector<Track::Ptr> getMediaTracks(MediaSource &sender, bool trackReady = true) const { return std::vector<Track::Ptr>(); };
    // 获取MultiMediaSourceMuxer对象  [AUTO-TRANSLATED:2de96d44]
    // Get MultiMediaSourceMuxer object
    virtual std::shared_ptr<MultiMediaSourceMuxer> getMuxer(MediaSource &sender) const { return nullptr; }
    // 获取RtpProcess对象  [AUTO-TRANSLATED:c6b7da43]
    // Get RtpProcess object
    virtual std::shared_ptr<RtpProcess> getRtpProcess(MediaSource &sender) const { return nullptr; }

    class SendRtpArgs {
    public:
        enum DataType {
            kRtpES = 0, // 发送ES流
            kRtpPS = 1, // 发送PS流
            kRtpTS = 2 // 发送TS流
        };

        enum ConType {
            kTcpActive = 0, // tcp主动模式，tcp客户端主动连接对方并发送rtp
            kUdpActive = 1, // udp主动模式，主动发送数据给对方
            kTcpPassive = 2, // tcp被动模式，tcp服务器，等待对方连接并回复rtp
            kUdpPassive = 3, // udp被动方式，等待对方发送nat打洞包，然后回复rtp至打洞包源地址
            kVoiceTalk = 4,  // 语音对讲模式，对方必须想推流上来，通过他的推流链路再回复rtp数据
        };

        // rtp类型  [AUTO-TRANSLATED:acca40ab]
        // Rtp type
        DataType data_type = kRtpPS;
        // 连接类型  [AUTO-TRANSLATED:8ad5c881]
        // Connection type
        ConType con_type = kUdpActive;

        // 发送es流时指定是否只发送纯音频流  [AUTO-TRANSLATED:470c761e]
        // Specify whether to send only pure audio stream when sending es stream
        bool only_audio = false;
        // rtp payload type
        uint8_t pt = 96;
        // 是否支持同ssrc多服务器发送  [AUTO-TRANSLATED:9d817af2]
        // Whether to support multiple servers sending with the same ssrc
        bool ssrc_multi_send = false;
        // 指定rtp ssrc  [AUTO-TRANSLATED:7366c6f9]
        // Specify rtp ssrc
        std::string ssrc;
        // 指定本地发送端口  [AUTO-TRANSLATED:f5d92f40]
        // Specify local sending port
        uint16_t src_port = 0;
        // 发送目标端口  [AUTO-TRANSLATED:096b5574]
        // Send target port
        uint16_t dst_port;
        // 发送目标主机地址，可以是ip或域名  [AUTO-TRANSLATED:2c872f2e]
        // Send target host address, can be ip or domain name
        std::string dst_url;

        // udp发送时，是否开启rr rtcp接收超时判断  [AUTO-TRANSLATED:784982bd]
        // When sending udp, whether to enable rr rtcp receive timeout judgment
        bool udp_rtcp_timeout = false;
        // passive被动、tcp主动模式超时时间  [AUTO-TRANSLATED:8886d475]
        // Passive passive, tcp active mode timeout time
        uint32_t close_delay_ms = 0;
        // udp 发送时，rr rtcp包接收超时时间，单位毫秒  [AUTO-TRANSLATED:9f0d91d9]
        // When sending udp, rr rtcp packet receive timeout time, in milliseconds
        uint32_t rtcp_timeout_ms = 30 * 1000;
        // udp 发送时，发送sr rtcp包间隔，单位毫秒  [AUTO-TRANSLATED:c87bfed4]
        // When sending udp, send sr rtcp packet interval, in milliseconds
        uint32_t rtcp_send_interval_ms = 5 * 1000;

        // 发送rtp同时接收，一般用于双向语言对讲, 如果不为空，说明开启接收  [AUTO-TRANSLATED:f4c18084]
        // Send rtp while receiving, generally used for two-way language intercom, if not empty, it means receiving is enabled
        std::string recv_stream_id;

        std::string recv_stream_app;
        std::string recv_stream_vhost;
    };

    // 开始发送ps-rtp  [AUTO-TRANSLATED:a51796fa]
    // Start sending ps-rtp
    virtual void startSendRtp(MediaSource &sender, const SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) { cb(0, toolkit::SockException(toolkit::Err_other, "not implemented"));};
    // 停止发送ps-rtp  [AUTO-TRANSLATED:952d2b35]
    // Stop sending ps-rtp
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
    // 时间戳类型  [AUTO-TRANSLATED:7d2779e1]
    // Timestamp type
    int modify_stamp;

    // 转协议是否开启音频  [AUTO-TRANSLATED:220dddfa]
    // Whether to enable audio for protocol conversion
    bool enable_audio;
    // 添加静音音频，在关闭音频时，此开关无效  [AUTO-TRANSLATED:47c0ec8e]
    // Add mute audio, this switch is invalid when audio is closed
    bool add_mute_audio;
    // 无人观看时，是否直接关闭(而不是通过on_none_reader hook返回close)  [AUTO-TRANSLATED:dba7ab70]
    // Whether to close directly when no one is watching (instead of returning close through the on_none_reader hook)
    // 此配置置1时，此流如果无人观看，将不触发on_none_reader hook回调，  [AUTO-TRANSLATED:a5ead314]
    // When this configuration is set to 1, if no one is watching this stream, it will not trigger the on_none_reader hook callback,
    // 而是将直接关闭流  [AUTO-TRANSLATED:06887d49]
    // but will directly close the stream
    bool auto_close;

    // 断连续推延时，单位毫秒，默认采用配置文件  [AUTO-TRANSLATED:7a15b12f]
    // Delay in milliseconds for continuous pushing, default is using the configuration file
    uint32_t continue_push_ms;

    // 平滑发送定时器间隔，单位毫秒，置0则关闭；开启后影响cpu性能同时增加内存  [AUTO-TRANSLATED:ad4e306a]
    // Smooth sending timer interval, in milliseconds, set to 0 to close; enabling it will affect cpu performance and increase memory at the same time
    // 该配置开启后可以解决一些流发送不平滑导致zlmediakit转发也不平滑的问题  [AUTO-TRANSLATED:0f2b1657]
    // This configuration can solve some problems where the stream is not sent smoothly, resulting in zlmediakit forwarding not being smooth
    uint32_t paced_sender_ms;

    // 是否开启转换为hls(mpegts)  [AUTO-TRANSLATED:bfc1167a]
    // Whether to enable conversion to hls(mpegts)
    bool enable_hls;
    // 是否开启转换为hls(fmp4)  [AUTO-TRANSLATED:20548673]
    // Whether to enable conversion to hls(fmp4)
    bool enable_hls_fmp4;
    // 是否开启MP4录制  [AUTO-TRANSLATED:0157b014]
    // Whether to enable MP4 recording
    bool enable_mp4;
    // 是否开启转换为rtsp/webrtc  [AUTO-TRANSLATED:0711cb18]
    // Whether to enable conversion to rtsp/webrtc
    bool enable_rtsp;
    // 是否开启转换为rtmp/flv  [AUTO-TRANSLATED:d4774119]
    // Whether to enable conversion to rtmp/flv
    bool enable_rtmp;
    // 是否开启转换为http-ts/ws-ts  [AUTO-TRANSLATED:51acc798]
    // Whether to enable conversion to http-ts/ws-ts
    bool enable_ts;
    // 是否开启转换为http-fmp4/ws-fmp4  [AUTO-TRANSLATED:8c96e1e4]
    // Whether to enable conversion to http-fmp4/ws-fmp4
    bool enable_fmp4;

    // hls协议是否按需生成，如果hls.segNum配置为0(意味着hls录制)，那么hls将一直生成(不管此开关)  [AUTO-TRANSLATED:4653b411]
    // Whether to generate hls protocol on demand, if hls.segNum is configured to 0 (meaning hls recording), then hls will always be generated (regardless of this switch)
    bool hls_demand;
    // rtsp[s]协议是否按需生成  [AUTO-TRANSLATED:1c3237b0]
    // Whether to generate rtsp[s] protocol on demand
    bool rtsp_demand;
    // rtmp[s]、http[s]-flv、ws[s]-flv协议是否按需生成  [AUTO-TRANSLATED:09ed2c30]
    // Whether to generate rtmp[s]、http[s]-flv、ws[s]-flv protocol on demand
    bool rtmp_demand;
    // http[s]-ts协议是否按需生成  [AUTO-TRANSLATED:a0129db3]
    // Whether to generate http[s]-ts protocol on demand
    bool ts_demand;
    // http[s]-fmp4、ws[s]-fmp4协议是否按需生成  [AUTO-TRANSLATED:828d25c7]
    // Whether to generate http[s]-fmp4、ws[s]-fmp4 protocol on demand
    bool fmp4_demand;

    // 是否将mp4录制当做观看者  [AUTO-TRANSLATED:ba351230]
    // Whether to treat mp4 recording as a viewer
    bool mp4_as_player;
    // mp4切片大小，单位秒  [AUTO-TRANSLATED:c3fb8ec1]
    // MP4 slice size, in seconds
    size_t mp4_max_second;
    // mp4录制保存路径  [AUTO-TRANSLATED:6d860f27]
    // MP4 recording save path
    std::string mp4_save_path;

    // hls录制保存路径  [AUTO-TRANSLATED:cfa90719]
    // HLS recording save path
    std::string hls_save_path;

    // 支持通过on_publish返回值替换stream_id  [AUTO-TRANSLATED:2c4e4997]
    // Support replacing stream_id through the return value of on_publish
    std::string stream_replace;

    // 最大track数  [AUTO-TRANSLATED:2565fd37]
    // Maximum number of tracks
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

// 该对象用于拦截感兴趣的MediaSourceEvent事件  [AUTO-TRANSLATED:fd6d0559]
// This object is used to intercept interesting MediaSourceEvent events
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
 * Parse the url to get media information
 
 * [AUTO-TRANSLATED:3b3cfde7]
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
 * Media source, any rtsp/rtmp live stream originates from this object
 
 * [AUTO-TRANSLATED:658077ad]
 */
class MediaSource: public TrackSource, public std::enable_shared_from_this<MediaSource> {
public:
    static MediaSource& NullMediaSource();
    using Ptr = std::shared_ptr<MediaSource>;

    MediaSource(const std::string &schema, const MediaTuple& tuple);
    virtual ~MediaSource();

    // //////////////获取MediaSource相关信息////////////////  [AUTO-TRANSLATED:4a520f1f]
    // //////////////Get MediaSource information////////////////

    // 获取协议类型  [AUTO-TRANSLATED:d6b50c14]
    // Get protocol type
    const std::string& getSchema() const {
        return _schema;
    }

    const MediaTuple& getMediaTuple() const {
        return _tuple;
    }

    std::string getUrl() const { return _schema + "://" + _tuple.shortUrl(); }

    // 获取对象所有权  [AUTO-TRANSLATED:84fb43cd]
    // Get object ownership
    std::shared_ptr<void> getOwnership();

    // 获取所有Track  [AUTO-TRANSLATED:59f1c570]
    // Get all Tracks
    std::vector<Track::Ptr> getTracks(bool ready = true) const override;

    // 获取流当前时间戳  [AUTO-TRANSLATED:f65f560a]
    // Get the current timestamp of the stream
    virtual uint32_t getTimeStamp(TrackType type) { return 0; };
    // 设置时间戳  [AUTO-TRANSLATED:2bfce32f]
    // Set timestamp
    virtual void setTimeStamp(uint32_t stamp) {};

    // 获取数据速率，单位bytes/s  [AUTO-TRANSLATED:c70465c1]
    // Get data rate, unit bytes/s
    int getBytesSpeed(TrackType type = TrackInvalid);
    // 获取流创建GMT unix时间戳，单位秒  [AUTO-TRANSLATED:0bbe145e]
    // Get the stream creation GMT unix timestamp, unit seconds
    uint64_t getCreateStamp() const { return _create_stamp; }
    // 获取流上线时间，单位秒  [AUTO-TRANSLATED:a087d56a]
    // Get the stream online time, unit seconds
    uint64_t getAliveSecond() const;

    // //////////////MediaSourceEvent相关接口实现////////////////  [AUTO-TRANSLATED:aa63d949]
    // //////////////MediaSourceEvent related interface implementation////////////////

    // 设置监听者  [AUTO-TRANSLATED:b9b90b57]
    // Set listener
    virtual void setListener(const std::weak_ptr<MediaSourceEvent> &listener);
    // 获取监听者  [AUTO-TRANSLATED:5c9cbb82]
    // Get listener
    std::weak_ptr<MediaSourceEvent> getListener() const;

    // 本协议获取观看者个数，可能返回本协议的观看人数，也可能返回总人数  [AUTO-TRANSLATED:0874fa7c]
    // This protocol gets the number of viewers, it may return the number of viewers of this protocol, or it may return the total number of viewers
    virtual int readerCount() = 0;
    // 观看者个数，包括(hls/rtsp/rtmp)  [AUTO-TRANSLATED:6604020f]
    // Number of viewers, including (hls/rtsp/rtmp)
    virtual int totalReaderCount();
    // 获取播放器列表  [AUTO-TRANSLATED:e7691d2b]
    // Get the player list
    virtual void getPlayerList(const std::function<void(const std::list<toolkit::Any> &info_list)> &cb,
                               const std::function<toolkit::Any(toolkit::Any &&info)> &on_change) {
        assert(cb);
        cb(std::list<toolkit::Any>());
    }

    virtual bool broadcastMessage(const toolkit::Any &data) { return false; }

    // 获取媒体源类型  [AUTO-TRANSLATED:34290a69]
    // Get the media source type
    MediaOriginType getOriginType() const;
    // 获取媒体源url或者文件路径  [AUTO-TRANSLATED:fa34d795]
    // Get the media source url or file path
    std::string getOriginUrl() const;
    // 获取媒体源客户端相关信息  [AUTO-TRANSLATED:037ef910]
    // Get the media source client information
    std::shared_ptr<toolkit::SockInfo> getOriginSock() const;

    // 拖动进度条  [AUTO-TRANSLATED:65ee8a83]
    // Drag the progress bar
    bool seekTo(uint32_t stamp);
    // 暂停  [AUTO-TRANSLATED:ffd21ae7]
    // Pause
    bool pause(bool pause);
    // 倍数播放  [AUTO-TRANSLATED:a5e3c1c9]
    // Playback speed
    bool speed(float speed);
    // 关闭该流  [AUTO-TRANSLATED:b3867b98]
    // Close the stream
    bool close(bool force);
    // 该流观看人数变化  [AUTO-TRANSLATED:8e583993]
    // The number of viewers of this stream changes
    void onReaderChanged(int size);
    // 开启或关闭录制  [AUTO-TRANSLATED:3817e390]
    // Turn recording on or off
    bool setupRecord(Recorder::type type, bool start, const std::string &custom_path, size_t max_second);
    // 获取录制状态  [AUTO-TRANSLATED:a0499880]
    // Get recording status
    bool isRecording(Recorder::type type);
    // 开始发送ps-rtp  [AUTO-TRANSLATED:a51796fa]
    // Start sending ps-rtp
    void startSendRtp(const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb);
    // 停止发送ps-rtp  [AUTO-TRANSLATED:952d2b35]
    // Stop sending ps-rtp
    bool stopSendRtp(const std::string &ssrc);
    // 获取丢包率  [AUTO-TRANSLATED:ec61b378]
    // Get packet loss rate
    float getLossRate(mediakit::TrackType type);
    // 获取所在线程  [AUTO-TRANSLATED:75662eb8]
    // Get the thread where it is running
    toolkit::EventPoller::Ptr getOwnerPoller();
    // 获取MultiMediaSourceMuxer对象  [AUTO-TRANSLATED:2de96d44]
    // Get the MultiMediaSourceMuxer object
    std::shared_ptr<MultiMediaSourceMuxer> getMuxer() const;
    // 获取RtpProcess对象  [AUTO-TRANSLATED:c6b7da43]
    // Get the RtpProcess object
    std::shared_ptr<RtpProcess> getRtpProcess() const;

    // //////////////static方法，查找或生成MediaSource////////////////  [AUTO-TRANSLATED:c3950036]
    // //////////////static methods, find or generate MediaSource////////////////

    // 同步查找流  [AUTO-TRANSLATED:97048f1e]
    // Synchronously find the stream
    static Ptr find(const std::string &schema, const std::string &vhost, const std::string &app, const std::string &id, bool from_mp4 = false);
    static Ptr find(const MediaInfo &info, bool from_mp4 = false) {
        return find(info.schema, info.vhost, info.app, info.stream, from_mp4);
    }

    // 忽略schema，同步查找流，可能返回rtmp/rtsp/hls类型  [AUTO-TRANSLATED:8c061cac]
    // Ignore schema, synchronously find the stream, may return rtmp/rtsp/hls type
    static Ptr find(const std::string &vhost, const std::string &app, const std::string &stream_id, bool from_mp4 = false);

    // 异步查找流  [AUTO-TRANSLATED:4decf738]
    // Asynchronously find the stream
    static void findAsync(const MediaInfo &info, const std::shared_ptr<toolkit::Session> &session, const std::function<void(const Ptr &src)> &cb);
    // 遍历所有流  [AUTO-TRANSLATED:a39b2399]
    // Traverse all streams
    static void for_each_media(const std::function<void(const Ptr &src)> &cb, const std::string &schema = "", const std::string &vhost = "", const std::string &app = "", const std::string &stream = "");
    // 从mp4文件生成MediaSource  [AUTO-TRANSLATED:7df9762e]
    // Generate MediaSource from mp4 file
    static MediaSource::Ptr createFromMP4(const std::string &schema, const std::string &vhost, const std::string &app, const std::string &stream, const std::string &file_path = "", bool check_app = true);

protected:
    // 媒体注册  [AUTO-TRANSLATED:dbf5c730]
    // Media registration
    void regist();

private:
    // 媒体注销  [AUTO-TRANSLATED:06a0630a]
    // Media unregistration
    bool unregist();
    // 触发媒体事件  [AUTO-TRANSLATED:0c2f9ae6]
    // Trigger media events
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
    // 对象个数统计  [AUTO-TRANSLATED:f4a012d0]
    // Object count statistics
    toolkit::ObjectStatistic<MediaSource> _statistic;
};

} /* namespace mediakit */
#endif //ZLMEDIAKIT_MEDIASOURCE_H
