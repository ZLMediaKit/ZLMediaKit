/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
    CodecG711A,
    CodecG711U,
    CodecMax = 0x7FFF
} CodecId;

typedef enum {
    TrackInvalid = -1,
    TrackVideo = 0,
    TrackAudio,
    TrackTitle,
    TrackMax = 3
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

    /**
     * 获取编码器名称
     * @return 编码器名称
     */
    const char *getCodecName();
};

/**
 * 帧类型的抽象接口
 */
class Frame : public Buffer, public CodecInfo {
public:
    typedef std::shared_ptr<Frame> Ptr;
    virtual ~Frame(){}

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

    /**
     * 是否为配置帧，譬如sps pps vps
     * @return
     */
    virtual bool configFrame() const = 0;

    /**
     * 是否可以缓存
     */
    virtual bool cacheAble() const { return true; }

    /**
     * 返回可缓存的frame
     * @return
     */
    static Ptr getCacheAbleFrame(const Ptr &frame);
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
 * 支持代理转发的帧环形缓存
 */
class FrameDispatcher : public FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameDispatcher> Ptr;

    FrameDispatcher(){}
    virtual ~FrameDispatcher(){}

    void addDelegate(const FrameWriterInterface::Ptr &delegate){
        //_delegates_write可能多线程同时操作
        lock_guard<mutex> lck(_mtx);
        _delegates_write.emplace(delegate.get(),delegate);
        _need_update = true;
    }

    void delDelegate(void *ptr){
        //_delegates_write可能多线程同时操作
        lock_guard<mutex> lck(_mtx);
        _delegates_write.erase(ptr);
        _need_update = true;
    }

    /**
     * 写入帧数据
     * @param frame 帧
     */
    void inputFrame(const Frame::Ptr &frame) override{
        if(_need_update){
            //发现代理列表发生变化了，这里同步一次
            lock_guard<mutex> lck(_mtx);
            _delegates_read = _delegates_write;
            _need_update = false;
        }

        //_delegates_read能确保是单线程操作的
        for(auto &pr : _delegates_read){
            pr.second->inputFrame(frame);
        }

    }
private:
    mutex _mtx;
    map<void *,FrameWriterInterface::Ptr>  _delegates_read;
    map<void *,FrameWriterInterface::Ptr>  _delegates_write;
    bool _need_update = false;
};

/**
 * 通过Frame接口包装指针，方便使用者把自己的数据快速接入ZLMediaKit
 */
class FrameFromPtr : public Frame{
public:
    typedef std::shared_ptr<FrameFromPtr> Ptr;
    char *data() const override{
        return _ptr;
    }
    uint32_t size() const override {
        return _size;
    }

    uint32_t dts() const override {
        return _dts;
    }

    uint32_t pts() const override{
        if(_pts){
            return _pts;
        }
        return dts();
    }

    uint32_t prefixSize() const override{
        return _prefixSize;
    }
protected:
    char *_ptr;
    uint32_t _size;
    uint32_t _dts;
    uint32_t _pts = 0;
    uint32_t _prefixSize;
};

/**
 * 不可缓存的帧，在DevChannel类中有用到。
 * 该帧类型用于防止内存拷贝，直接使用指针传递数据
 * 在大多数情况下，ZLMediaKit是同步对帧数据进行使用和处理的
 * 所以提供此类型的帧很有必要，但是有时又无法避免缓存帧做后续处理
 * 所以可以通过Frame::getCacheAbleFrame方法拷贝一个可缓存的帧
 */
class FrameNoCacheAble : public FrameFromPtr{
public:
    typedef std::shared_ptr<FrameNoCacheAble> Ptr;

    /**
     * 该帧不可缓存
     * @return
     */
    bool cacheAble() const override {
        return false;
    }
};

/**
 * 该对象的功能是把一个不可缓存的帧转换成可缓存的帧
 * @see FrameNoCacheAble
 */
class FrameCacheAble : public FrameFromPtr {
public:
    typedef std::shared_ptr<FrameCacheAble> Ptr;

    FrameCacheAble(const Frame::Ptr &frame){
        if(frame->cacheAble()){
            _frame = frame;
            _ptr = frame->data();
        }else{
            _buffer = std::make_shared<BufferRaw>();
            _buffer->assign(frame->data(),frame->size());
            _ptr = _buffer->data();
        }
        _size = frame->size();
        _dts = frame->dts();
        _pts = frame->pts();
        _prefixSize = frame->prefixSize();
        _trackType = frame->getTrackType();
        _codec = frame->getCodecId();
        _key = frame->keyFrame();
        _config = frame->configFrame();
    }

    virtual ~FrameCacheAble() = default;

    /**
     * 可以被缓存
     * @return
     */
    bool cacheAble() const override {
        return true;
    }

    TrackType getTrackType() const override{
        return _trackType;
    }

    CodecId getCodecId() const override{
        return _codec;
    }

    bool keyFrame() const override{
        return _key;
    }

    bool configFrame() const override{
        return _config;
    }
private:
    Frame::Ptr _frame;
    BufferRaw::Ptr _buffer;
    TrackType _trackType;
    CodecId _codec;
    bool _key;
    bool _config;
};


}//namespace mediakit

#endif //ZLMEDIAKIT_FRAME_H
