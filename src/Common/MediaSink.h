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

class MediaSinkInterface : public FrameWriterInterface, public TrackListener {
public:
    typedef std::shared_ptr<MediaSinkInterface> Ptr;

    MediaSinkInterface() = default;
    ~MediaSinkInterface() override = default;
};

/**
 * aac静音音频添加器
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
 * 该类的作用是等待Track ready()返回true也就是就绪后再通知派生类进行下一步的操作
 * 目的是输入Frame前由Track截取处理下，以便获取有效的信息（譬如sps pps aa_cfg）
 */
class MediaSink : public MediaSinkInterface, public TrackSource{
public:
    typedef std::shared_ptr<MediaSink> Ptr;
    MediaSink() = default;
    ~MediaSink() override = default;

    /**
     * 输入frame
     * @param frame
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 添加track，内部会调用Track的clone方法
     * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
     * @param track
     */
    bool addTrack(const Track::Ptr & track) override;

    /**
     * 添加Track完毕，如果是单Track，会最多等待3秒才会触发onAllTrackReady
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
     */
    void emitAllTrackReady();

    /**
     * 检查track是否准备完毕
     */
    void checkTrackIfReady();
    void onAllTrackReady_l();
    /**
     * 添加aac静音轨道
     */
    bool addMuteAudioTrack();

private:
    bool _enable_audio = true;
    bool _add_mute_audio = true;
    bool _all_track_ready = false;
    size_t _max_track_size = 2;
    std::unordered_map<int, std::pair<Track::Ptr, bool/*got frame*/> > _track_map;
    std::unordered_map<int, toolkit::List<Frame::Ptr> > _frame_unread;
    std::unordered_map<int, std::function<void()> > _track_ready_callback;
    toolkit::Ticker _ticker;
    MuteAudioMaker::Ptr _mute_audio_maker;
};


}//namespace mediakit

#endif //ZLMEDIAKIT_MEDIASINK_H
