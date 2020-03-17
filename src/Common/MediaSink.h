/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
    map<int,Track::Ptr> _track_map;
    map<int,function<void()> > _trackReadyCallback;
    bool _allTrackReady = false;
    Ticker _ticker;
    int _max_track_size = 2;
};


}//namespace mediakit

#endif //ZLMEDIAKIT_MEDIASINK_H
