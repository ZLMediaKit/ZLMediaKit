/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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

using namespace std;
using namespace toolkit;

namespace mediakit{

class MediaSinkInterface : public FrameWriterInterface {
public:
    typedef std::shared_ptr<MediaSinkInterface> Ptr;

    MediaSinkInterface(){};
    virtual ~MediaSinkInterface(){};

    /**
     * 添加track，内部会调用Track的clone方法
     * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
     * @param track
     */
    virtual void addTrack(const Track::Ptr & track) = 0;

    /**
     * 重置track
     */
    virtual void resetTracks() = 0;
};

/**
 * 该类的作用是等待Track ready()返回true也就是就绪后再通知派生类进行下一步的操作
 * 目的是输入Frame前由Track截取处理下，以便获取有效的信息（譬如sps pps aa_cfg）
 */
class MediaSink : public MediaSinkInterface , public TrackSource{
public:
    typedef std::shared_ptr<MediaSink> Ptr;
    MediaSink(){}
    virtual ~MediaSink(){}

    /**
     * 输入frame
     * @param frame
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 添加track，内部会调用Track的clone方法
     * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
     * @param track
     */
    void addTrack(const Track::Ptr & track) override;

    /**
     * 添加Track完毕，如果是单Track，会最多等待3秒才会触发onAllTrackReady
     * 这样会增加生成流的延时，如果添加了音视频双Track，那么可以不调用此方法
     * 否则为了降低流注册延时，请手动调用此方法
     */
    void addTrackCompleted();

    /**
     * 重置track
     */
    void resetTracks() override;

    /**
     * 获取所有Track
     * @param trackReady 是否获取已经准备好的Track
     */
    vector<Track::Ptr> getTracks(bool trackReady = true) const override ;
protected:
    /**
     * 某track已经准备好，其ready()状态返回true，
     * 此时代表可以获取其例如sps pps等相关信息了
     * @param track
     */
    virtual void onTrackReady(const Track::Ptr & track) {};

    /**
     * 所有Track已经准备好，
     */
    virtual void onAllTrackReady() {};

    /**
     * 某Track输出frame，在onAllTrackReady触发后才会调用此方法
     * @param frame
     */
    virtual void onTrackFrame(const Frame::Ptr &frame) {};
private:
    /**
     * 触发onAllTrackReady事件
     */
    void emitAllTrackReady();

    /**
     * 检查track是否准备完毕
     */
    void checkTrackIfReady(const Track::Ptr &track);
    void checkTrackIfReady_l(const Track::Ptr &track);
private:
    mutable recursive_mutex _mtx;
    unordered_map<int,Track::Ptr> _track_map;
    unordered_map<int,List<Frame::Ptr> > _frame_unread;
    unordered_map<int,function<void()> > _track_ready_callback;
    bool _all_track_ready = false;
    Ticker _ticker;
    int _max_track_size = 2;
};


}//namespace mediakit

#endif //ZLMEDIAKIT_MEDIASINK_H
