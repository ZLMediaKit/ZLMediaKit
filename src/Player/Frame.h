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

class Frame : public Buffer, public CodecInfo{
public:
    typedef std::shared_ptr<Frame> Ptr;
    virtual ~Frame(){}
    /**
     * 时间戳
     */
    virtual uint32_t stamp() const = 0;

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

class FrameWriterInterfaceHelper : public FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameWriterInterfaceHelper> Ptr;
    typedef std::function<void(const Frame::Ptr &frame)> onWriteFrame;

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


/**
 * 264帧类
 */
class H264Frame : public Frame {
public:
    typedef std::shared_ptr<H264Frame> Ptr;

    char *data() const override{
        return (char *)buffer.data();
    }
    uint32_t size() const override {
        return buffer.size();
    }
    uint32_t stamp() const override {
        return timeStamp;
    }
    uint32_t prefixSize() const override{
        return iPrefixSize;
    }

    TrackType getTrackType() const override{
        return TrackVideo;
    }

    CodecId getCodecId() const override{
        return CodecH264;
    }

    bool keyFrame() const override {
        return type == 5;
    }
public:
    uint16_t sequence;
    uint32_t timeStamp;
    unsigned char type;
    string buffer;
    uint32_t iPrefixSize = 4;
};


/**
 * aac帧，包含adts头
 */
class AACFrame : public Frame {
public:
    typedef std::shared_ptr<AACFrame> Ptr;

    char *data() const override{
        return (char *)buffer;
    }
    uint32_t size() const override {
        return aac_frame_length;
    }
    uint32_t stamp() const override {
        return timeStamp;
    }
    uint32_t prefixSize() const override{
        return iPrefixSize;
    }

    TrackType getTrackType() const override{
        return TrackAudio;
    }

    CodecId getCodecId() const override{
        return CodecAAC;
    }

    bool keyFrame() const override {
        return false;
    }
public:
    unsigned int syncword; //12 bslbf 同步字The bit string ‘1111 1111 1111’，说明一个ADTS帧的开始
    unsigned int id;        //1 bslbf   MPEG 标示符, 设置为1
    unsigned int layer;    //2 uimsbf Indicates which layer is used. Set to ‘00’
    unsigned int protection_absent;  //1 bslbf  表示是否误码校验
    unsigned int profile; //2 uimsbf  表示使用哪个级别的AAC，如01 Low Complexity(LC)--- AACLC
    unsigned int sf_index;           //4 uimsbf  表示使用的采样率下标
    unsigned int private_bit;        //1 bslbf
    unsigned int channel_configuration;  //3 uimsbf  表示声道数
    unsigned int original;               //1 bslbf
    unsigned int home;                   //1 bslbf
    //下面的为改变的参数即每一帧都不同
    unsigned int copyright_identification_bit;   //1 bslbf
    unsigned int copyright_identification_start; //1 bslbf
    unsigned int aac_frame_length; // 13 bslbf  一个ADTS帧的长度包括ADTS头和raw data block
    unsigned int adts_buffer_fullness;           //11 bslbf     0x7FF 说明是码率可变的码流
//no_raw_data_blocks_in_frame 表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧.
//所以说number_of_raw_data_blocks_in_frame == 0
//表示说ADTS帧中有一个AAC数据块并不是说没有。(一个AAC原始帧包含一段时间内1024个采样及相关数据)
    unsigned int no_raw_data_blocks_in_frame;    //2 uimsfb
    unsigned char buffer[2 * 1024 + 7];
    uint16_t sequence;
    uint32_t timeStamp;
    uint32_t iPrefixSize = 7;
} ;



class FrameNoCopyAble : public Frame{
public:
    typedef std::shared_ptr<FrameNoCopyAble> Ptr;
    char *data() const override{
        return buffer_ptr;
    }
    uint32_t size() const override {
        return buffer_size;
    }
    uint32_t stamp() const override {
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


class H264FrameNoCopyAble : public FrameNoCopyAble {
public:
    typedef std::shared_ptr<H264FrameNoCopyAble> Ptr;

    H264FrameNoCopyAble(char *ptr,uint32_t size,uint32_t stamp,int prefixeSize = 4){
        buffer_ptr = ptr;
        buffer_size = size;
        timeStamp = stamp;
        iPrefixSize = prefixeSize;
    }

    TrackType getTrackType() const override{
        return TrackVideo;
    }

    CodecId getCodecId() const override{
        return CodecH264;
    }

    bool keyFrame() const override {
        return (buffer_ptr[iPrefixSize] & 0x1F) == 5;
    }
};

class AACFrameNoCopyAble : public FrameNoCopyAble {
public:
    typedef std::shared_ptr<AACFrameNoCopyAble> Ptr;

    AACFrameNoCopyAble(char *ptr,uint32_t size,uint32_t stamp,int prefixeSize = 7){
        buffer_ptr = ptr;
        buffer_size = size;
        timeStamp = stamp;
        iPrefixSize = prefixeSize;
    }

    TrackType getTrackType() const override{
        return TrackAudio;
    }

    CodecId getCodecId() const override{
        return CodecAAC;
    }

    bool keyFrame() const override {
        return false;
    }
} ;


}//namespace mediakit

#endif //ZLMEDIAKIT_FRAME_H
