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

#ifndef ZLMEDIAKIT_FRAME_H
#define ZLMEDIAKIT_FRAME_H

#include <mutex>
#include <functional>
#include "Util/RingBuffer.h"
#include "Network/Socket.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

typedef enum {
    CodecInvalid = -1,
    CodecH264 = 0,
    CodecH265,
    CodecAAC,
    CodecMax = 0x7FFF
} CodecId;

typedef enum {
    TrackInvalid = -1,
    TrackVideo = 0,
    TrackAudio,
    TrackTitle,
    TrackMax = 0x7FFF
} TrackType;

/**
 * 编码信息的抽象接口
 */
class CodecInfo {
public:
    typedef std::shared_ptr<CodecInfo> Ptr;

    CodecInfo(){}
    virtual ~CodecInfo(){}

    /**
     * 获取音视频类型
     */
    virtual TrackType getTrackType() const  = 0;

    /**
     * 获取编解码器类型
     */
    virtual CodecId getCodecId() const = 0;
};

/**
 * 帧类型的抽象接口
 */
class Frame : public Buffer, public CodecInfo{
public:
    typedef std::shared_ptr<Frame> Ptr;
    virtual ~Frame(){}
    /**
     * 时间戳,已经废弃，请使用dts() 、pts()接口
     */
    inline uint32_t stamp() const {
        return dts();
    };


    /**
     * 返回解码时间戳，单位毫秒
     * @return
     */
    virtual uint32_t dts() const = 0;



    /**
     * 返回显示时间戳，单位毫秒
     * @return
     */
    virtual uint32_t pts() const {
        return dts();
    }

    /**
     * 前缀长度，譬如264前缀为0x00 00 00 01,那么前缀长度就是4
     * aac前缀则为7个字节
     */
    virtual uint32_t prefixSize() const = 0;

    /**
     * 返回是否为关键帧
     * @return
     */
    virtual bool keyFrame() const = 0;
};

/**
 * 循环池辅助类
 * @tparam T
 */
template <typename T>
class ResourcePoolHelper{
public:
    ResourcePoolHelper(int size = 8){
        _pool.setSize(size);
    }
    virtual ~ResourcePoolHelper(){}

    std::shared_ptr<T> obtainObj(){
        return _pool.obtain();
    }
private:
    ResourcePool<T> _pool;
};

/**
 * 写帧接口的抽闲接口
 */
class FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameWriterInterface> Ptr;

    FrameWriterInterface(){}
    virtual ~FrameWriterInterface(){}
    /**
    * 写入帧数据
    * @param frame 帧
    */
    virtual void inputFrame(const Frame::Ptr &frame) = 0;
};

/**
 * 写帧接口转function，辅助类
 */
class FrameWriterInterfaceHelper : public FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameWriterInterfaceHelper> Ptr;
    typedef std::function<void(const Frame::Ptr &frame)> onWriteFrame;

    /**
     * inputFrame后触发onWriteFrame回调
     * @param cb
     */
    FrameWriterInterfaceHelper(const onWriteFrame& cb){
        _writeCallback = cb;
    }
    virtual ~FrameWriterInterfaceHelper(){}
    /**
    * 写入帧数据
    * @param frame 帧
    */
    void inputFrame(const Frame::Ptr &frame) override {
        _writeCallback(frame);
    }
private:
    onWriteFrame _writeCallback;
};


/**
 * 帧环形缓存接口类
 */
class FrameRingInterface : public FrameWriterInterface{
public:
    typedef RingBuffer<Frame::Ptr> RingType;
    typedef std::shared_ptr<FrameRingInterface> Ptr;

    FrameRingInterface(){}
    virtual ~FrameRingInterface(){}

    /**
     * 获取帧环形缓存
     * @return
     */
    virtual RingType::Ptr getFrameRing() const = 0;

    /**
     * 设置帧环形缓存
     * @param ring
     */
    virtual void setFrameRing(const RingType::Ptr &ring)  = 0;
};

/**
 * 帧环形缓存
 */
class FrameRing : public FrameRingInterface{
public:
    typedef std::shared_ptr<FrameRing> Ptr;

    FrameRing(){
    }
    virtual ~FrameRing(){}

    /**
     * 获取帧环形缓存
     * @return
     */
    RingType::Ptr getFrameRing() const override {
        return _frameRing;
    }

    /**
     * 设置帧环形缓存
     * @param ring
     */
    void setFrameRing(const RingType::Ptr &ring) override {
        _frameRing = ring;
    }

    /**
     * 输入数据帧
     * @param frame
     */
    void inputFrame(const Frame::Ptr &frame) override{
        if(_frameRing){
            _frameRing->write(frame,frame->keyFrame());
        }
    }
protected:
    RingType::Ptr _frameRing;
};

/**
 * 支持代理转发的帧环形缓存
 */
class FrameRingInterfaceDelegate : public FrameRing {
public:
    typedef std::shared_ptr<FrameRingInterfaceDelegate> Ptr;

    FrameRingInterfaceDelegate(){}
    virtual ~FrameRingInterfaceDelegate(){}

    void addDelegate(const FrameWriterInterface::Ptr &delegate){
        lock_guard<mutex> lck(_mtx);
        _delegateMap.emplace(delegate.get(),delegate);
    }

    void delDelegate(void *ptr){
        lock_guard<mutex> lck(_mtx);
        _delegateMap.erase(ptr);
    }

    /**
     * 写入帧数据
     * @param frame 帧
     */
    void inputFrame(const Frame::Ptr &frame) override{
        FrameRing::inputFrame(frame);
        lock_guard<mutex> lck(_mtx);
        for(auto &pr : _delegateMap){
            pr.second->inputFrame(frame);
        }
    }
private:
    mutex _mtx;
    map<void *,FrameWriterInterface::Ptr>  _delegateMap;
};

class FrameNoCopyAble : public Frame{
public:
    typedef std::shared_ptr<FrameNoCopyAble> Ptr;
    char *data() const override{
        return buffer_ptr;
    }
    uint32_t size() const override {
        return buffer_size;
    }
    uint32_t dts() const override {
        return timeStamp;
    }
    uint32_t prefixSize() const override{
        return iPrefixSize;
    }
public:
    char *buffer_ptr;
    uint32_t buffer_size;
    uint32_t timeStamp;
    uint32_t iPrefixSize;
};





}//namespace mediakit

#endif //ZLMEDIAKIT_FRAME_H
