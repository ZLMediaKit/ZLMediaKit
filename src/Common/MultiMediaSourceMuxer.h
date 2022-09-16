/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H

#include "Common/Stamp.h"
#include "Rtp/RtpSender.h"
#include "Record/Recorder.h"
#include "Record/HlsRecorder.h"
#include "Record/HlsMediaSource.h"
#include "Rtsp/RtspMediaSourceMuxer.h"
#include "Rtmp/RtmpMediaSourceMuxer.h"
#include "TS/TSMediaSourceMuxer.h"
#include "FMP4/FMP4MediaSourceMuxer.h"

namespace mediakit {

class ProtocolOption {
public:
    ProtocolOption();

    //是否开启转换为hls
    bool enable_hls = false;
    //是否开启MP4录制
    bool enable_mp4 = false;
    //是否将mp4录制当做观看者
    bool mp4_as_player = false;
    //是否开启转换为rtsp/webrtc
    bool enable_rtsp = true;
    //是否开启转换为rtmp/flv
    bool enable_rtmp = true;
    //是否开启转换为http-ts/ws-ts
    bool enable_ts = true;
    //是否开启转换为http-fmp4/ws-fmp4
    bool enable_fmp4 = true;

    //转协议是否开启音频
    bool enable_audio = true;
    //添加静音音频，在关闭音频时，此开关无效
    bool add_mute_audio = true;

    //mp4录制保存路径
    std::string mp4_save_path;
    //mp4切片大小，单位秒
    size_t mp4_max_second = 0;

    //hls录制保存路径
    std::string hls_save_path;

    //断连续推延时，单位毫秒，默认采用配置文件
    uint32_t continue_push_ms;
    
    //时间戳修复这一路流标志位
    bool modify_stamp;

    template <typename MAP>
    ProtocolOption(const MAP &allArgs) : ProtocolOption() {
        #define GET_OPT_VALUE(key) getArgsValue(allArgs, #key, key)
        GET_OPT_VALUE(enable_hls);
        GET_OPT_VALUE(enable_mp4);
        GET_OPT_VALUE(mp4_as_player);
        GET_OPT_VALUE(enable_rtsp);
        GET_OPT_VALUE(enable_rtmp);
        GET_OPT_VALUE(enable_ts);
        GET_OPT_VALUE(enable_fmp4);
        GET_OPT_VALUE(enable_audio);
        GET_OPT_VALUE(add_mute_audio);
        GET_OPT_VALUE(mp4_save_path);
        GET_OPT_VALUE(mp4_max_second);
        GET_OPT_VALUE(hls_save_path);
        GET_OPT_VALUE(continue_push_ms);
        GET_OPT_VALUE(modify_stamp);
    }

    ProtocolOption(const ProtocolOption &) = default;

private:
    template <typename MAP, typename KEY, typename TYPE>
    static void getArgsValue(const MAP &allArgs, const KEY &key, TYPE &value) {
        auto val = ((MAP &)allArgs)[key];
        if (!val.empty()) {
            value = (TYPE)val;
        }
    }
};

class MultiMediaSourceMuxer : public MediaSourceEventInterceptor, public MediaSink, public std::enable_shared_from_this<MultiMediaSourceMuxer>{
public:
    typedef std::shared_ptr<MultiMediaSourceMuxer> Ptr;

    class Listener{
    public:
        Listener() = default;
        virtual ~Listener() = default;
        virtual void onAllTrackReady() = 0;
    };

    MultiMediaSourceMuxer(const std::string &vhost, const std::string &app, const std::string &stream, float dur_sec = 0.0,const ProtocolOption &option = ProtocolOption());
    ~MultiMediaSourceMuxer() override = default;

    /**
     * 设置事件监听器
     * @param listener 监听器
     */
    void setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener);

     /**
      * 随着Track就绪事件监听器
      * @param listener 事件监听器
     */
    void setTrackListener(const std::weak_ptr<Listener> &listener);

    /**
     * 返回总的消费者个数
     */
    int totalReaderCount() const;

    /**
     * 判断是否生效(是否正在转其他协议)
     */
    bool isEnabled();

    /**
     * 设置MediaSource时间戳
     * @param stamp 时间戳
     */
    void setTimeStamp(uint32_t stamp);

    /**
     * 重置track
     */
    void resetTracks() override;

    /////////////////////////////////MediaSourceEvent override/////////////////////////////////

    /**
     * 观看总人数
     * @param sender 事件发送者
     * @return 观看总人数
     */
    int totalReaderCount(MediaSource &sender) override;

    /**
     * 设置录制状态
     * @param type 录制类型
     * @param start 开始或停止
     * @param custom_path 开启录制时，指定自定义路径
     * @return 是否设置成功
     */
    bool setupRecord(MediaSource &sender, Recorder::type type, bool start, const std::string &custom_path, size_t max_second) override;

    /**
     * 获取录制状态
     * @param type 录制类型
     * @return 录制状态
     */
    bool isRecording(MediaSource &sender, Recorder::type type) override;

    /**
     * 开始发送ps-rtp流
     * @param dst_url 目标ip或域名
     * @param dst_port 目标端口
     * @param ssrc rtp的ssrc
     * @param is_udp 是否为udp
     * @param cb 启动成功或失败回调
     */
    void startSendRtp(MediaSource &sender, const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) override;

    /**
     * 停止ps-rtp发送
     * @return 是否成功
     */
    bool stopSendRtp(MediaSource &sender, const std::string &ssrc) override;

    /**
     * 获取所有Track
     * @param trackReady 是否筛选过滤未就绪的track
     * @return 所有Track
     */
    std::vector<Track::Ptr> getMediaTracks(MediaSource &sender, bool trackReady = true) const override;

    /**
     * 获取所属线程
     */
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;

    const std::string& getVhost() const;
    const std::string& getApp() const;
    const std::string& getStreamId() const;

protected:
    /////////////////////////////////MediaSink override/////////////////////////////////

    /**
    * 某track已经准备好，其ready()状态返回true，
    * 此时代表可以获取其例如sps pps等相关信息了
    * @param track
    */
    bool onTrackReady(const Track::Ptr & track) override;

    /**
     * 所有Track已经准备好，
     */
    void onAllTrackReady() override;

    /**
     * 某Track输出frame，在onAllTrackReady触发后才会调用此方法
     * @param frame
     */
    bool onTrackFrame(const Frame::Ptr &frame) override;

private:
    bool _is_enable = false;
    std::string _vhost;
    std::string _app;
    std::string _stream_id;
    ProtocolOption _option;
    toolkit::Ticker _last_check;
    Stamp _stamp[2];
    std::weak_ptr<Listener> _track_listener;
    std::function<std::string()> _get_origin_url;
#if defined(ENABLE_RTPPROXY)
    std::unordered_map<std::string, RtpSender::Ptr> _rtp_sender;
#endif //ENABLE_RTPPROXY

#if defined(ENABLE_MP4)
    FMP4MediaSourceMuxer::Ptr _fmp4;
#endif
    RtmpMediaSourceMuxer::Ptr _rtmp;
    RtspMediaSourceMuxer::Ptr _rtsp;
    TSMediaSourceMuxer::Ptr _ts;
    MediaSinkInterface::Ptr _mp4;
    HlsRecorder::Ptr _hls;
    toolkit::EventPoller::Ptr _poller;

    //对象个数统计
    toolkit::ObjectStatistic<MultiMediaSourceMuxer> _statistic;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
