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

#ifndef ZLMEDIAKIT_H264_H
#define ZLMEDIAKIT_H264_H

#include "Frame.h"
#include "Track.h"

namespace mediakit{

bool getAVCInfo(const string &strSps,int &iVideoWidth, int &iVideoHeight, float  &iVideoFps);
bool getAVCInfo(const char * sps,int sps_len,int &iVideoWidth, int &iVideoHeight, float  &iVideoFps);

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


/**
 * 264视频通道
 */
class H264Track : public VideoTrack{
public:
    typedef std::shared_ptr<H264Track> Ptr;

    /**
     * 不指定sps pps构造h264类型的媒体
     * 在随后的inputFrame中获取sps pps
     */
    H264Track(){}
    /**
     * 构造h264类型的媒体
     * @param sps sps帧数据
     * @param pps pps帧数据
     * @param sps_prefix_len 264头长度，可以为3个或4个字节，一般为0x00 00 00 01
     * @param pps_prefix_len 264头长度，可以为3个或4个字节，一般为0x00 00 00 01
     */
    H264Track(const string &sps,const string &pps,int sps_prefix_len = 4,int pps_prefix_len = 4){
        _sps = sps.substr(sps_prefix_len);
        _pps = pps.substr(pps_prefix_len);
        parseSps(_sps);
    }

    /**
     * 构造h264类型的媒体
     * @param sps sps帧
     * @param pps pps帧
     */
    H264Track(const Frame::Ptr &sps,const Frame::Ptr &pps){
        if(sps->getCodecId() != CodecH264 || pps->getCodecId() != CodecH264 ){
            throw std::invalid_argument("必须输入H264类型的帧");
        }
        _sps = string(sps->data() + sps->prefixSize(),sps->size() - sps->prefixSize());
        _pps = string(pps->data() + pps->prefixSize(),pps->size() - pps->prefixSize());
        parseSps(_sps);
    }

    /**
     * 返回不带0x00 00 00 01头的sps
     * @return
     */
    const string &getSps() const{
        return _sps;
    }

    /**
     * 返回不带0x00 00 00 01头的pps
     * @return
     */
    const string &getPps() const{
        return _pps;
    }

    CodecId getCodecId() const override {
        return CodecH264;
    }

    /**
     * 返回视频高度
     * @return
     */
    int getVideoHeight() const override{
        return _width;
    }

    /**
     * 返回视频宽度
     * @return
     */
    int getVideoWidth() const override{
        return _height;
    }

    /**
     * 返回视频fps
     * @return
     */
    float getVideoFps() const override{
        return _fps;
    }

    bool ready() override {
        return !_sps.empty() && !_pps.empty();
    }


    /**
     * 输入数据帧,并获取sps pps
     * @param frame 数据帧
     */
    void inputFrame(const Frame::Ptr &frame) override{
        int type = (*((uint8_t *)frame->data() + frame->prefixSize())) & 0x1F;
        switch (type){
            case 7:{
                //sps
                bool flag = _sps.empty();
                _sps = string(frame->data() + frame->prefixSize(),frame->size() - frame->prefixSize());
                if(flag && _width == 0){
                    parseSps(_sps);
                }
            }
                break;
            case 8:{
                //pps
                _pps = string(frame->data() + frame->prefixSize(),frame->size() - frame->prefixSize());
            }
                break;

            case 5:{
                //I
                if(!_sps.empty()){
                    if(!_spsFrame)
                    {
                        H264Frame::Ptr insertFrame = std::make_shared<H264Frame>();
                        insertFrame->type = 7;
                        insertFrame->timeStamp = frame->stamp();
                        insertFrame->buffer.assign("\x0\x0\x0\x1",4);
                        insertFrame->buffer.append(_sps);
                        insertFrame->iPrefixSize = 4;
                        _spsFrame = insertFrame;
                    }
                    _spsFrame->timeStamp = frame->stamp();
                    VideoTrack::inputFrame(_spsFrame);
                }

                if(!_pps.empty()){
                    if(!_ppsFrame)
                    {
                        H264Frame::Ptr insertFrame = std::make_shared<H264Frame>();
                        insertFrame->type = 8;
                        insertFrame->timeStamp = frame->stamp();
                        insertFrame->buffer.assign("\x0\x0\x0\x1",4);
                        insertFrame->buffer.append(_pps);
                        insertFrame->iPrefixSize = 4;
                        _ppsFrame = insertFrame;
                    }
                    _ppsFrame->timeStamp = frame->stamp();
                    VideoTrack::inputFrame(_ppsFrame);
                }
                VideoTrack::inputFrame(frame);
            }
                break;

            case 1:{
                //B or P
                VideoTrack::inputFrame(frame);
            }
                break;
        }
    }
private:
    /**
     * 解析sps获取宽高fps
     * @param sps sps不含头数据
     */
    void parseSps(const string &sps){
        getAVCInfo(sps,_width,_height,_fps);
    }
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }
private:
    string _sps;
    string _pps;
    int _width = 0;
    int _height = 0;
    float _fps = 0;
    H264Frame::Ptr _spsFrame;
    H264Frame::Ptr _ppsFrame;
};


}//namespace mediakit


#endif //ZLMEDIAKIT_H264_H
