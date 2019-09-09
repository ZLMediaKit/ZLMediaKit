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

#ifndef ZLMEDIAKIT_H265_H
#define ZLMEDIAKIT_H265_H

#include "Frame.h"
#include "Track.h"
#include "Util/base64.h"
#include "H264.h"
using namespace toolkit;
#define H265_TYPE(v) (((uint8_t)(v) >> 1) & 0x3f)

namespace mediakit {

bool getHEVCInfo(const string &strVps, const string &strSps, int &iVideoWidth, int &iVideoHeight, float &iVideoFps);

/**
* 265帧类
*/
class H265Frame : public Frame {
public:
    typedef std::shared_ptr<H265Frame> Ptr;

    typedef enum {
        NAL_TRAIL_N = 0,
        NAL_TRAIL_R = 1,
        NAL_TSA_N = 2,
        NAL_TSA_R = 3,
        NAL_STSA_N = 4,
        NAL_STSA_R = 5,
        NAL_RADL_N = 6,
        NAL_RADL_R = 7,
        NAL_RASL_N = 8,
        NAL_RASL_R = 9,
        NAL_BLA_W_LP = 16,
        NAL_BLA_W_RADL = 17,
        NAL_BLA_N_LP = 18,
        NAL_IDR_W_RADL = 19,
        NAL_IDR_N_LP = 20,
        NAL_CRA_NUT = 21,
        NAL_VPS = 32,
        NAL_SPS = 33,
        NAL_PPS = 34,
        NAL_AUD = 35,
        NAL_EOS_NUT = 36,
        NAL_EOB_NUT = 37,
        NAL_FD_NUT = 38,
        NAL_SEI_PREFIX = 39,
        NAL_SEI_SUFFIX = 40,
    } NaleType;

    char *data() const override {
        return (char *) buffer.data();
    }

    uint32_t size() const override {
        return buffer.size();
    }

    uint32_t dts() const override {
        return timeStamp;
    }

    uint32_t pts() const override {
        return ptsStamp ? ptsStamp : timeStamp;
    }

    uint32_t prefixSize() const override {
        return iPrefixSize;
    }

    TrackType getTrackType() const override {
        return TrackVideo;
    }

    CodecId getCodecId() const override {
        return CodecH265;
    }

    bool keyFrame() const override {
        return isKeyFrame(H265_TYPE(buffer[iPrefixSize]));
    }

    bool configFrame() const override{
        switch(H265_TYPE(buffer[iPrefixSize])){
            case H265Frame::NAL_VPS:
            case H265Frame::NAL_SPS:
            case H265Frame::NAL_PPS:
                return true;
            default:
                return false;
        }
    }

    static bool isKeyFrame(int type) {
        switch (type) {
            case NAL_BLA_N_LP:
            case NAL_BLA_W_LP:
            case NAL_BLA_W_RADL:
            case NAL_CRA_NUT:
            case NAL_IDR_N_LP:
            case NAL_IDR_W_RADL:
                return true;
            default:
                return false;
        }
    }

public:
    uint32_t timeStamp;
    uint32_t ptsStamp = 0;
    string buffer;
    uint32_t iPrefixSize = 4;
};


class H265FrameNoCacheAble : public FrameNoCacheAble {
public:
    typedef std::shared_ptr<H265FrameNoCacheAble> Ptr;

    H265FrameNoCacheAble(char *ptr, uint32_t size, uint32_t dts,uint32_t pts, int prefixeSize = 4) {
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _pts = pts;
        _prefixSize = prefixeSize;
    }

    TrackType getTrackType() const override {
        return TrackVideo;
    }

    CodecId getCodecId() const override {
        return CodecH265;
    }

    bool keyFrame() const override {
        return H265Frame::isKeyFrame(H265_TYPE(((uint8_t *) _ptr)[_prefixSize]));
    }

    bool configFrame() const override{
        switch(H265_TYPE(((uint8_t *) _ptr)[_prefixSize])){
            case H265Frame::NAL_VPS:
            case H265Frame::NAL_SPS:
            case H265Frame::NAL_PPS:
                return true;
            default:
                return false;
        }
    }
};

typedef FrameInternal<H265FrameNoCacheAble> H265FrameInternal;

/**
* 265视频通道
*/
class H265Track : public VideoTrack {
public:
    typedef std::shared_ptr<H265Track> Ptr;

    /**
     * 不指定sps pps构造h265类型的媒体
     * 在随后的inputFrame中获取sps pps
     */
    H265Track() {}

    /**
     * 构造h265类型的媒体
     * @param vps vps帧数据
     * @param sps sps帧数据
     * @param pps pps帧数据
     * @param vps_prefix_len 265头长度，可以为3个或4个字节，一般为0x00 00 00 01
     * @param sps_prefix_len 265头长度，可以为3个或4个字节，一般为0x00 00 00 01
     * @param pps_prefix_len 265头长度，可以为3个或4个字节，一般为0x00 00 00 01
     */
    H265Track(const string &vps,const string &sps, const string &pps,int vps_prefix_len = 4, int sps_prefix_len = 4, int pps_prefix_len = 4) {
        _vps = vps.substr(vps_prefix_len);
        _sps = sps.substr(sps_prefix_len);
        _pps = pps.substr(pps_prefix_len);
		onReady();
    }

    /**
     * 返回不带0x00 00 00 01头的vps
     * @return
     */
    const string &getVps() const {
        return _vps;
    }

    /**
     * 返回不带0x00 00 00 01头的sps
     * @return
     */
    const string &getSps() const {
        return _sps;
    }

    /**
     * 返回不带0x00 00 00 01头的pps
     * @return
     */
    const string &getPps() const {
        return _pps;
    }

    CodecId getCodecId() const override {
        return CodecH265;
    }

    /**
     * 返回视频高度
     * @return
     */
    int getVideoHeight() const override{
        return _height ;
    }

    /**
     * 返回视频宽度
     * @return
     */
    int getVideoWidth() const override{
        return _width;
    }

    /**
     * 返回视频fps
     * @return
     */
    float getVideoFps() const override{
        return _fps;
    }

    bool ready() override {
        return !_vps.empty() && !_sps.empty() && !_pps.empty();
    }


    /**
    * 输入数据帧,并获取sps pps
    * @param frame 数据帧
    */
    void inputFrame(const Frame::Ptr &frame) override{
		int type = H265_TYPE(*((uint8_t *)frame->data() + frame->prefixSize()));
        if(type == H265Frame::NAL_VPS){
	        bool  first_frame = true;
	        splitH264(frame->data() + frame->prefixSize(),
                  frame->size() - frame->prefixSize(),
                  [&](const char *ptr, int len){
                      if(first_frame){
                          H265FrameInternal::Ptr sub_frame = std::make_shared<H265FrameInternal>(frame,
                                                                                                 frame->data(),
                                                                                                 len + frame->prefixSize(),
                                                                                                 frame->prefixSize());
                          inputFrame_l(sub_frame);
                          first_frame = false;
                      }else{
                          H265FrameInternal::Ptr sub_frame = std::make_shared<H265FrameInternal>(frame,
                                                                                                 (char *)ptr,
                                                                                                 len ,
                                                                                                 3);
                          inputFrame_l(sub_frame);
                      }
                  });
        	}else{
				inputFrame_l(frame);
			}
    }

private:
    /**
     * 输入数据帧,并获取sps pps
     * @param frame 数据帧
     */
    void inputFrame_l(const Frame::Ptr &frame) {
        int type = H265_TYPE(((uint8_t *) frame->data() + frame->prefixSize())[0]);
        if (H265Frame::isKeyFrame(type)) {
            insertConfigFrame(frame);
            VideoTrack::inputFrame(frame);
            _last_frame_is_idr = true;
            return;
        }

        _last_frame_is_idr = false;

        //非idr帧
        switch (type) {
            case H265Frame::NAL_VPS: {
                //vps
                _vps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            }
                break;

            case H265Frame::NAL_SPS: {
                //sps
                _sps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            }
                break;
            case H265Frame::NAL_PPS: {
                //pps
                _pps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            }
                break;

            default: {
                //other frames
                VideoTrack::inputFrame(frame);
            }
                break;
        }
    }

	/**
     * 解析sps获取宽高fps
     */
    void onReady(){
        getHEVCInfo(_vps, _sps, _width, _height, _fps);
    }
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this);
    }

    //生成sdp
    Sdp::Ptr getSdp() override ;

    //在idr帧前插入vps sps pps帧
    void insertConfigFrame(const Frame::Ptr &frame){
        if(_last_frame_is_idr){
            return;
        }
        if(!_vps.empty()){
            auto vpsFrame = std::make_shared<H265Frame>();
            vpsFrame->iPrefixSize = 4;
            vpsFrame->buffer.assign("\x0\x0\x0\x1", 4);
            vpsFrame->buffer.append(_vps);
            vpsFrame->timeStamp = frame->stamp();
            VideoTrack::inputFrame(vpsFrame);
        }
        if (!_sps.empty()) {
            auto spsFrame = std::make_shared<H265Frame>();
            spsFrame->iPrefixSize = 4;
            spsFrame->buffer.assign("\x0\x0\x0\x1", 4);
            spsFrame->buffer.append(_sps);
            spsFrame->timeStamp = frame->stamp();
            VideoTrack::inputFrame(spsFrame);
        }

        if (!_pps.empty()) {
            auto ppsFrame = std::make_shared<H265Frame>();
            ppsFrame->iPrefixSize = 4;
            ppsFrame->buffer.assign("\x0\x0\x0\x1", 4);
            ppsFrame->buffer.append(_pps);
            ppsFrame->timeStamp = frame->stamp();
            VideoTrack::inputFrame(ppsFrame);
        }
    }
private:
    string _vps;
    string _sps;
    string _pps;
    int _width = 0;
    int _height = 0;	
    float _fps = 0;
    bool _last_frame_is_idr = false;
};


/**
* h265类型sdp
*/
class H265Sdp : public Sdp {
public:

    /**
     *
     * @param sps 265 sps,不带0x00000001头
     * @param pps 265 pps,不带0x00000001头
     * @param playload_type  rtp playload type 默认96
     * @param bitrate 比特率
     */
    H265Sdp(const string &strVPS,
            const string &strSPS,
            const string &strPPS,
            int playload_type = 96,
            int bitrate = 4000) : Sdp(90000,playload_type) {
        //视频通道
        _printer << "m=video 0 RTP/AVP " << playload_type << "\r\n";
        _printer << "b=AS:" << bitrate << "\r\n";
        _printer << "a=rtpmap:" << playload_type << " H265/" << 90000 << "\r\n";
        _printer << "a=fmtp:" << playload_type << " ";
        _printer << "sprop-vps=";
        _printer << encodeBase64(strVPS) << "; ";
        _printer << "sprop-sps=";
        _printer << encodeBase64(strSPS) << "; ";
        _printer << "sprop-pps=";
        _printer << encodeBase64(strPPS) << "\r\n";
        _printer << "a=control:trackID=" << getTrackType() << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    TrackType getTrackType() const override {
        return TrackVideo;
    }

    CodecId getCodecId() const override {
        return CodecH265;
    }
private:
    _StrPrinter _printer;
};


    
}//namespace mediakit


#endif //ZLMEDIAKIT_H265_H
