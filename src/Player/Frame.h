//
// Created by xzl on 2018/10/18.
//

#ifndef ZLMEDIAKIT_FRAME_H
#define ZLMEDIAKIT_FRAME_H

#include "Network/Socket.h"

using namespace ZL::Network;

class Frame : public Buffer {
public:
    typedef std::shared_ptr<Frame> Ptr;
    virtual ~Frame(){}
    virtual uint32_t stamp() = 0;
};

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
public:
    uint16_t sequence;
    uint32_t timeStamp;
    unsigned char type;
    string buffer;
};



//ADTS 头中相对有用的信息 采样率、声道数、帧长度
class AdtsFrame : public Frame {
public:
    typedef std::shared_ptr<AdtsFrame> Ptr;

    char *data() const override{
        return (char *)buffer;
    }
    uint32_t size() const override {
        return aac_frame_length;
    }
    uint32_t stamp() override {
        return timeStamp;
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
} ;




#endif //ZLMEDIAKIT_FRAME_H
