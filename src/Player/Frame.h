//
// Created by xzl on 2018/10/18.
//

#ifndef ZLMEDIAKIT_FRAME_H
#define ZLMEDIAKIT_FRAME_H

#include "Util/RingBuffer.h"
#include "Network/Socket.h"

using namespace ZL::Util;
using namespace ZL::Network;

typedef enum {
    CodecInvalid = -1,
    CodecH264 = 0,
    CodecAAC = 0x0100,
    CodecMax
} CodecId;

typedef enum {
    TrackInvalid = -1,
    TrackVideo = 0,
    TrackAudio,
    TrackMax
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

/**
 * 帧环形缓存接口类
 */
class FrameRingInterface {
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

    /**
     * 写入帧数据
     * @param frame 帧
     */
    virtual void inputFrame(const Frame::Ptr &frame) = 0;
};

class FrameRing : public FrameRingInterface{
public:
    typedef std::shared_ptr<FrameRing> Ptr;

    FrameRing(){
        _frameRing = std::make_shared<RingType>();
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
        _frameRing->write(frame,frame->keyFrame());
    }
protected:
    RingType::Ptr _frameRing;
};

class FrameRingInterfaceDelegate : public FrameRingInterface {
public:
    typedef std::shared_ptr<FrameRingInterfaceDelegate> Ptr;

    FrameRingInterfaceDelegate(){
        _delegate = std::make_shared<FrameRing>();
    }
    virtual ~FrameRingInterfaceDelegate(){}

    void setDelegate(const FrameRingInterface::Ptr &delegate){
        _delegate = delegate;
    }
    /**
     * 获取帧环形缓存
     * @return
     */
    FrameRingInterface::RingType::Ptr getFrameRing() const override {
        if(_delegate){
            return _delegate->getFrameRing();
        }
        return nullptr;
    }

    /**
     * 设置帧环形缓存
     * @param ring
     */
    void setFrameRing(const FrameRingInterface::RingType::Ptr &ring) override {
        if(_delegate){
            _delegate->setFrameRing(ring);
        }
    }

    /**
     * 写入帧数据
     * @param frame 帧
     */
    void inputFrame(const Frame::Ptr &frame) override{
        if(_delegate){
            _delegate->inputFrame(frame);
        }
    }
private:
    FrameRingInterface::Ptr _delegate;
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
    uint32_t iPrefixSize = 4;
} ;




#endif //ZLMEDIAKIT_FRAME_H
