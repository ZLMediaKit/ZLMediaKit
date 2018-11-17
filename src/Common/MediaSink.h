/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

/**
 * 该类的作用是等待Track ready()返回true也就是就绪后再通知派生类进行下一步的操作
 * 目的是输入Frame前由Track截取处理下，以便获取有效的信息（譬如sps pps aa_cfg）
 */
class MediaSink : public FrameWriterInterface , public std::enable_shared_from_this<MediaSink>{
public:
    typedef std::shared_ptr<MediaSink> Ptr;
    MediaSink(){}
    virtual ~MediaSink(){}

    /**
     * 输入frame
     * @param frame
     */
    void inputFrame(const Frame::Ptr &frame) override ;

    /**
     * 添加track，内部会调用Track的clone方法
     * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
     * @param track
     */
    virtual void addTrack(const Track::Ptr & track);


    /**
     * 全部Track是否都准备好了
     * @return
     */
    bool isAllTrackReady() const ;


    /**
     * 获取特定类型的Track
     * @param type track类型
	 * @param trackReady 是否获取已经准备好的Track
     * @return
     */
    Track::Ptr getTrack(TrackType type,bool trackReady = true) const ;
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
    mutable recursive_mutex _mtx;
    map<int,Track::Ptr> _track_map;
    map<int,function<void()> > _trackReadyCallback;
    bool _allTrackReady = false;
    Ticker _ticker;
};
















}//namespace mediakit

#endif //ZLMEDIAKIT_MEDIASINK_H
