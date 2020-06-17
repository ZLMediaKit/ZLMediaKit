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
    CodecOpus,
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
 * 获取编码器名称
 */
const char *getCodecName(CodecId codecId);

/**
 * 获取音视频类型
 */
TrackType getTrackType(CodecId codecId);

/**
 * 编码信息的抽象接口
 */
class CodecInfo {
public:
    typedef std::shared_ptr<CodecInfo> Ptr;

    CodecInfo(){}
    virtual ~CodecInfo(){}

    /**
     * 获取编解码器类型
     */
    virtual CodecId getCodecId() const = 0;

    /**
     * 获取编码器名称
     */
    const char *getCodecName();

    /**
     * 获取音视频类型
     */
    TrackType getTrackType();
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
     */
    virtual uint32_t dts() const = 0;

    /**
     * 返回显示时间戳，单位毫秒
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
     */
    virtual bool keyFrame() const = 0;

    /**
     * 是否为配置帧，譬如sps pps vps
     */
    virtual bool configFrame() const = 0;

    /**
     * 是否可以缓存
     */
    virtual bool cacheAble() const { return true; }

    /**
     * 返回可缓存的frame
     */
    static Ptr getCacheAbleFrame(const Ptr &frame);
};

class FrameImp : public Frame {
public:
    typedef std::shared_ptr<FrameImp> Ptr;

    char *data() const override{
        return (char *)_buffer.data();
    }

    uint32_t size() const override {
        return _buffer.size();
    }

    uint32_t dts() const override {
        return _dts;
    }

    uint32_t pts() const override{
        return _pts ? _pts : _dts;
    }

    uint32_t prefixSize() const override{
        return _prefix_size;
    }

    CodecId getCodecId() const override{
        return _codecid;
    }

    bool keyFrame() const override {
        return false;
    }

    bool configFrame() const override{
        return false;
    }

public:
    CodecId _codecid = CodecInvalid;
    string _buffer;
    uint32_t _dts = 0;
    uint32_t _pts = 0;
    uint32_t _prefix_size = 0;
};

/**
 * 一个Frame类中可以有多个帧，他们通过 0x 00 00 01 分隔
 * ZLMediaKit会先把这种复合帧split成单个帧然后再处理
 * 一个复合帧可以通过无内存拷贝的方式切割成多个子Frame
 * 提供该类的目的是切割复合帧时防止内存拷贝，提高性能
 */
template<typename Parent>
class FrameInternal : public Parent{
public:
    typedef std::shared_ptr<FrameInternal> Ptr;
    FrameInternal(const Frame::Ptr &parent_frame, char *ptr, uint32_t size, int prefix_size)
            : Parent(ptr, size, parent_frame->dts(), parent_frame->pts(), prefix_size) {
        _parent_frame = parent_frame;
    }
    bool cacheAble() const override {
        return _parent_frame->cacheAble();
    }
private:
    Frame::Ptr _parent_frame;
};

/**
 * 循环池辅助类
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
 * 写帧接口的抽象接口类
 */
class FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameWriterInterface> Ptr;
    FrameWriterInterface(){}
    virtual ~FrameWriterInterface(){}

    /**
     * 写入帧数据
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
     */
    FrameWriterInterfaceHelper(const onWriteFrame& cb){
        _writeCallback = cb;
    }

    virtual ~FrameWriterInterfaceHelper(){}

    /**
     * 写入帧数据
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

    /**
     * 添加代理
     */
    void addDelegate(const FrameWriterInterface::Ptr &delegate){
        //_delegates_write可能多线程同时操作
        lock_guard<mutex> lck(_mtx);
        _delegates_write.emplace(delegate.get(),delegate);
        _need_update = true;
    }

    /**
     * 删除代理
     */
    void delDelegate(FrameWriterInterface *ptr){
        //_delegates_write可能多线程同时操作
        lock_guard<mutex> lck(_mtx);
        _delegates_write.erase(ptr);
        _need_update = true;
    }

    /**
     * 写入帧并派发
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

    /**
     * 返回代理个数
     */
    int size() const {
        return _delegates_write.size();
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
        return _pts ? _pts : dts();
    }

    uint32_t prefixSize() const override{
        return _prefix_size;
    }

    bool cacheAble() const override {
        return false;
    }
protected:
    char *_ptr;
    uint32_t _size;
    uint32_t _dts;
    uint32_t _pts = 0;
    uint32_t _prefix_size;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_FRAME_H