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

namespace mediakit{

class MultiMuxerPrivate : public MediaSink, public std::enable_shared_from_this<MultiMuxerPrivate>{
public:
    friend class MultiMediaSourceMuxer;
    typedef std::shared_ptr<MultiMuxerPrivate> Ptr;
    class Listener{
    public:
        Listener() = default;
        virtual ~Listener() = default;
        virtual void onAllTrackReady() = 0;
    };

    ~MultiMuxerPrivate() override;

private:
    MultiMuxerPrivate(const string &vhost,const string &app, const string &stream,float dur_sec,
                      bool enable_rtsp, bool enable_rtmp, bool enable_hls, bool enable_mp4);
    void resetTracks() override;
    void setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener);
    int totalReaderCount() const;
    void setTimeStamp(uint32_t stamp);
    void setTrackListener(Listener *listener);
    bool setupRecord(MediaSource &sender, Recorder::type type, bool start, const string &custom_path, size_t max_second);
    bool isRecording(MediaSource &sender, Recorder::type type);
    bool isEnabled();
    void onTrackReady(const Track::Ptr & track) override;
    void onTrackFrame(const Frame::Ptr &frame) override;
    void onAllTrackReady() override;

private:
    string _stream_url;
    Listener *_track_listener = nullptr;
    RtmpMediaSourceMuxer::Ptr _rtmp;
    RtspMediaSourceMuxer::Ptr _rtsp;
    HlsRecorder::Ptr _hls;
    MediaSinkInterface::Ptr _mp4;
    TSMediaSourceMuxer::Ptr _ts;
#if defined(ENABLE_MP4)
    FMP4MediaSourceMuxer::Ptr _fmp4;
#endif
    std::weak_ptr<MediaSourceEvent> _listener;
};

class MultiMediaSourceMuxer : public MediaSourceEventInterceptor, public MediaSinkInterface, public MultiMuxerPrivate::Listener, public std::enable_shared_from_this<MultiMediaSourceMuxer>{
public:
    typedef MultiMuxerPrivate::Listener Listener;
    typedef std::shared_ptr<MultiMediaSourceMuxer> Ptr;

    ~MultiMediaSourceMuxer() override;
    MultiMediaSourceMuxer(const string &vhost, const string &app, const string &stream, float dur_sec = 0.0,
                          bool enable_rtsp = true, bool enable_rtmp = true, bool enable_hls = true, bool enable_mp4 = false);

    /**
     * 设置事件监听器
     * @param listener 监听器
     */
    void setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener);

     /**
      * 随着Track就绪事件监听器
      * @param listener 事件监听器
     */
    void setTrackListener(const std::weak_ptr<MultiMuxerPrivate::Listener> &listener);

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

    /////////////////////////////////MediaSourceEvent override/////////////////////////////////

    /**
     * 获取所有Track
     * @param trackReady 是否筛选过滤未就绪的track
     * @return 所有Track
     */
    vector<Track::Ptr> getTracks(MediaSource &sender, bool trackReady = true) const override;

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
    bool setupRecord(MediaSource &sender, Recorder::type type, bool start, const string &custom_path, size_t max_second) override;

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
    void startSendRtp(MediaSource &sender, const string &dst_url, uint16_t dst_port, const string &ssrc, bool is_udp, uint16_t src_port, const function<void(uint16_t local_port, const SockException &ex)> &cb) override;

    /**
     * 停止ps-rtp发送
     * @return 是否成功
     */
    bool stopSendRtp(MediaSource &sender, const string &ssrc) override;

    /////////////////////////////////MediaSinkInterface override/////////////////////////////////

    /**
    * 添加track，内部会调用Track的clone方法
    * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
    * @param track 添加音频或视频轨道
    */
    void addTrack(const Track::Ptr &track) override;

    /**
     * 添加track完毕
     */
    void addTrackCompleted() override;

    /**
     * 重置track
     */
    void resetTracks() override;

    /**
     * 写入帧数据
     * @param frame 帧
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /////////////////////////////////MultiMuxerPrivate::Listener override/////////////////////////////////

    /**
     * 所有track全部就绪
     */
    void onAllTrackReady() override;

private:
    bool _is_enable = false;
    Ticker _last_check;
    Stamp _stamp[2];
    MultiMuxerPrivate::Ptr _muxer;
    std::weak_ptr<MultiMuxerPrivate::Listener> _track_listener;
#if defined(ENABLE_RTPPROXY)
    mutex _rtp_sender_mtx;
	unordered_map<string, RtpSender::Ptr> _rtp_sender;
#endif //ENABLE_RTPPROXY
    //对象个数统计
    ObjectStatistic<MultiMediaSourceMuxer> _statistic;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
