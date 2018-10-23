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
    virtual uint32_t stamp() = 0;

    /**
     * 前缀长度，譬如264前缀为0x00 00 00 01,那么前缀长度就是4
     * aac前缀则为7个字节
     */
    virtual uint32_t prefixSize() = 0;
};

/**
 * 帧环形缓存接口类
 */
class FrameRingInterface {
public:
    typedef RingBuffer<Frame::Ptr> RingType;

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
     * @param key_pos 是否为关键帧
     */
    virtual void inputFrame(const Frame::Ptr &frame,bool key_pos) = 0;
};


class FrameRing : public FrameRingInterface{
public:
    typedef std::shared_ptr<FrameRing> Ptr;

    FrameRing(){
        //禁用缓存
        _frameRing = std::make_shared<RingType>(1);
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
     * @param key_pos
     */
    void inputFrame(const Frame::Ptr &frame,bool key_pos) override{
        _frameRing->write(frame,key_pos);
    }
protected:
    RingType::Ptr _frameRing;
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
    uint32_t stamp() override {
        return timeStamp;
    }
    uint32_t prefixSize() override{
        return iPrefixSize;
    }

    TrackType getTrackType() const override{
        return TrackVideo;
    }

    CodecId getCodecId() const override{
        return CodecH264;
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
    uint32_t stamp() override {
        return timeStamp;
    }
    uint32_t prefixSize() override{
        return iPrefixSize;
    }

    TrackType getTrackType() const override{
        return TrackAudio;
    }

    CodecId getCodecId() const override{
        return CodecAAC;
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
