/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
#include "Rtsp/RtspMediaSourceMuxer.h"
#include "Rtmp/RtmpMediaSourceMuxer.h"
#include "Record/Recorder.h"
#include "Record/HlsMediaSource.h"
#include "Record/HlsRecorder.h"
namespace mediakit{

class MultiMuxerPrivate : public MediaSink , public std::enable_shared_from_this<MultiMuxerPrivate>{
public:
    friend class MultiMediaSourceMuxer;
    typedef std::shared_ptr<MultiMuxerPrivate> Ptr;
    class Listener{
    public:
        Listener() = default;
        virtual ~Listener() = default;
        virtual void onAllTrackReady() = 0;
    };
    ~MultiMuxerPrivate() override ;
private:
    MultiMuxerPrivate(const string &vhost,
                      const string &app,
                      const string &stream,
                      float dur_sec,
                      bool enable_rtsp,
                      bool enable_rtmp,
                      bool enable_hls,
                      bool enable_mp4);

    void resetTracks() override;
    void setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener);
    int totalReaderCount() const;
    void setTimeStamp(uint32_t stamp);
    void setTrackListener(Listener *listener);
    bool setupRecord(MediaSource &sender, Recorder::type type, bool start, const string &custom_path);
    bool isRecording(MediaSource &sender, Recorder::type type);
    bool isEnabled();
private:
    void onTrackReady(const Track::Ptr & track) override;
    void onTrackFrame(const Frame::Ptr &frame) override;
    void onAllTrackReady() override;
    MediaSource::Ptr getHlsMediaSource() const;
private:
    RtmpMediaSourceMuxer::Ptr _rtmp;
    RtspMediaSourceMuxer::Ptr _rtsp;
    MediaSinkInterface::Ptr _hls;
    MediaSinkInterface::Ptr _mp4;
    Listener *_listener = nullptr;
    std::weak_ptr<MediaSourceEvent> _meida_listener;
    bool _enable_rtxp = false;
    bool _enable_record = false;
};

class MultiMediaSourceMuxer : public MediaSourceEvent, public MediaSinkInterface, public TrackSource, public std::enable_shared_from_this<MultiMediaSourceMuxer>{
public:
    typedef MultiMuxerPrivate::Listener Listener;
    typedef std::shared_ptr<MultiMediaSourceMuxer> Ptr;

    ~MultiMediaSourceMuxer() override;
    MultiMediaSourceMuxer(const string &vhost,
                          const string &app,
                          const string &stream,
                          float dur_sec = 0.0,
                          bool enable_rtsp = true,
                          bool enable_rtmp = true,
                          bool enable_hls = true,
                          bool enable_mp4 = false);

    /**
     * 设置事件监听器
     * @param listener
     */
    void setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener);

    /**
     * 返回总的消费者个数
     * @return
     */
    int totalReaderCount() const;

    /**
     * 设置MediaSource时间戳
     * @param stamp 时间戳
     */
    void setTimeStamp(uint32_t stamp);

    /**
     * 随着Track就绪事件监听器
     * @param listener 事件监听器
     */
    void setTrackListener(Listener *listener);

    /**
     * 获取所有Track
     * @param trackReady 是否筛选过滤未就绪的track
     * @return 所有Track
     */
    vector<Track::Ptr> getTracks(bool trackReady = true) const override;

    /**
     * 通知拖动进度条
     * @param sender 事件发送者
     * @param ui32Stamp 目标时间戳
     * @return 是否成功
     */
    bool seekTo(MediaSource &sender,uint32_t ui32Stamp) override;

    /**
     * 通知停止流生成
     * @param sender 事件发送者
     * @param force 是否强制关闭
     * @return 成功与否
     */
    bool close(MediaSource &sender,bool force) override;

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
    bool setupRecord(MediaSource &sender, Recorder::type type, bool start, const string &custom_path) override;

    /**
     * 获取录制状态
     * @param type 录制类型
     * @return 录制状态
     */
    bool isRecording(MediaSource &sender, Recorder::type type) override;

    /**
    * 添加track，内部会调用Track的clone方法
    * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
    * @param track
    */
    void addTrack(const Track::Ptr & track) override;

    /**
     * 添加track完毕
     * @param track
     */
    void addTrackCompleted();

    /**
     * 重置track
     */
    void resetTracks() override;

    /**
     * 写入帧数据
     * @param frame 帧
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 判断是否生效(是否正在转其他协议)
     */
    bool isEnabled();
private:
    MultiMuxerPrivate::Ptr _muxer;
    std::weak_ptr<MediaSourceEvent> _listener;
    Stamp _stamp[2];
};

}//namespace mediakit
#endif //ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
