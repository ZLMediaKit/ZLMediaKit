/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MEDIASINK_H
#define ZLMEDIAKIT_MEDIASINK_H

#include <mutex>
#include <memory>
#include "Util/TimeTicker.h"
#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit{

class TrackListener {
public:
    TrackListener() = default;
    virtual ~TrackListener() = default;

    /**
     * 添加track，内部会调用Track的clone方法
     * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
     * @param track
     */
    virtual bool addTrack(const Track::Ptr & track) = 0;

    /**
     * 添加track完毕
     */
    virtual void addTrackCompleted() {};

    /**
     * 重置track
     */
    virtual void resetTracks() {};
};

/**
MediaSinkInterface抽象.
可inputFrame, 可加Track
*/
class MediaSinkInterface : public FrameWriterInterface, public TrackListener {
public:
    typedef std::shared_ptr<MediaSinkInterface> Ptr;

    MediaSinkInterface() = default;
    ~MediaSinkInterface() override = default;
};

/**
 * aac静音生成器
 * 接收视频帧，根据时间戳，同步伪造aac静音帧
 */
class MuteAudioMaker : public FrameDispatcher {
public:
    typedef std::shared_ptr<MuteAudioMaker> Ptr;

    MuteAudioMaker() = default;
    ~MuteAudioMaker() override = default;

    bool inputFrame(const Frame::Ptr &frame) override;

private:
    uint32_t _audio_idx = 0;
};

/**
 * 该类的作用是等待Track就绪后(ready=true)，再通知派生类进行下一步的操作
 * 目的是在输入Frame前，由Track截取处理下，以便获取配置信息（譬如sps pps aa_cfg）
 */
class MediaSink : public MediaSinkInterface, public TrackSource{
public:
    typedef std::shared_ptr<MediaSink> Ptr;
    MediaSink() = default;
    ~MediaSink() override = default;

    /**
     * 输入frame
     * @param frame
     frame -> Track::inputFrame -> delegate 
       if[_all_track_ready] 
         onTrackFrame 
       else
         cacheFrame
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 添加track，内部会调用Track的clone方法
     * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
     * @param track
     */
    bool addTrack(const Track::Ptr & track) override;

    /**
     * 添加Track完毕，更新_max_track_size，然后checkTrackIfReady.
     * 如果是单Track，会最多等待3秒才会触发onAllTrackReady(_max_track_size=2)
     * 这样会增加生成流的延时，如果添加了音视频双Track，那么可以不调用此方法
     * 否则为了降低流注册延时，请手动调用此方法
     */
    void addTrackCompleted() override;

    /**
     * 重置track
     */
    void resetTracks() override;

    /**
     * 获取所有Track
     * @param trackReady 是否获取已经准备好的Track
     */
    std::vector<Track::Ptr> getTracks(bool trackReady = true) const override;

    /**
     * 返回是否所有track已经准备完成
     */
    bool isAllTrackReady() const;

    /**
     * 设置是否开启音频
     */
    void enableAudio(bool flag);

    /**
     * 设置是否开启添加静音音频
     */
    void enableMuteAudio(bool flag);

protected:
    // 将TrackListener的事件消化，转换成如下事件

    /**
     * 某track已经准备好，其ready()状态返回true，
     * 此时代表可以获取其例如sps pps等相关信息了
     * @param track
     */
    virtual bool onTrackReady(const Track::Ptr & track) { return false; };

    /**
     * 所有Track已经准备好，
     */
    virtual void onAllTrackReady() {};

    /**
     * 某Track输出frame，在onAllTrackReady触发后才会调用此方法
     * @param frame
     */
    virtual bool onTrackFrame(const Frame::Ptr &frame) { return false; };

private:
    /**
     * 触发onAllTrackReady事件
     - 删除未准备好的track
     - 调用onAllTrackReady_l
     - 回调并清空 cacheFrame
     */
    void emitAllTrackReady();

    /**
     * 检查track是否准备完毕
     */
    void checkTrackIfReady();
    /*
     - 必要时创建_mute_audio_maker，来伪造静音
     - 触发onAllTrackReady回调
    */
    void onAllTrackReady_l();
    /**
     * 添加aac静音轨道
     */
    bool addMuteAudioTrack();

private:
    bool _enable_audio = true;
    bool _add_mute_audio = true;
    bool _all_track_ready = false;
    // 默认假设只有音视频track都有，addTrackCompleted中可改此值
    size_t _max_track_size = 2;
    // trackType -> (TrackPtr, got_frame)
    std::unordered_map<int, std::pair<Track::Ptr, bool/*got frame*/> > _track_map;
    // 帧缓存  trackType -> FrameList
    std::unordered_map<int, toolkit::List<Frame::Ptr> > _frame_unread;
    // 等待ready回调的track列表 trackType -> TrackPtr
    std::unordered_map<int, Track::Ptr> _track_ready_callback;
    toolkit::Ticker _ticker;
    // 静音伪造器
    MuteAudioMaker::Ptr _mute_audio_maker;
};

class MediaSinkDelegate : public MediaSink {
public:
    MediaSinkDelegate() = default;
    ~MediaSinkDelegate() override = default;

    /**
     * 设置track监听器
     */
    void setTrackListener(TrackListener *listener);

protected:
    /*
    MediaSink将自身的TrackListener转成，如下三回调;
    此类又将这三回调分装成_listener(TrackListener)回调了
    */
    void resetTracks() override;
    bool onTrackReady(const Track::Ptr & track) override;
    void onAllTrackReady() override;

private:
    TrackListener *_listener = nullptr;
};

class Demuxer : protected TrackListener, public TrackSource {
public:
    Demuxer() = default;
    ~Demuxer() override = default;

    void setTrackListener(TrackListener *listener, bool wait_track_ready = false);
    std::vector<Track::Ptr> getTracks(bool trackReady = true) const override;

protected:
    bool addTrack(const Track::Ptr &track) override;
    void addTrackCompleted() override;
    void resetTracks() override;

private:
    // 实际上是MediaSinkDelegate类型的，当wait_track_ready=true时创建
    MediaSink::Ptr _sink;
    TrackListener *_listener = nullptr;
    std::vector<Track::Ptr> _origin_track;
};
}//namespace mediakit

#endif //ZLMEDIAKIT_MEDIASINK_H
