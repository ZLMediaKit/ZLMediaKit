/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
class H265Frame : public FrameImp {
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
        NAL_RSV_IRAP_VCL22 = 22,
        NAL_RSV_IRAP_VCL23 = 23,

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

    H265Frame(){
        _codecid = CodecH265;
    }

    bool keyFrame() const override {
        return isKeyFrame(H265_TYPE(_buffer[_prefix_size]));
    }

    bool configFrame() const override{
        switch(H265_TYPE(_buffer[_prefix_size])){
            case H265Frame::NAL_VPS:
            case H265Frame::NAL_SPS:
            case H265Frame::NAL_PPS : return true;
            default : return false;
        }
    }

    static bool isKeyFrame(int type) {
        return type >= NAL_BLA_W_LP && type <= NAL_RSV_IRAP_VCL23;
    }
};

class H265FrameNoCacheAble : public FrameFromPtr {
public:
    typedef std::shared_ptr<H265FrameNoCacheAble> Ptr;

    H265FrameNoCacheAble(char *ptr, uint32_t size, uint32_t dts,uint32_t pts, int prefix_size = 4) {
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _pts = pts;
        _prefix_size = prefix_size;
    }

    CodecId getCodecId() const override {
        return CodecH265;
    }

    bool keyFrame() const override {
        return H265Frame::isKeyFrame(H265_TYPE(((uint8_t *) _ptr)[_prefix_size]));
    }

    bool configFrame() const override{
        switch(H265_TYPE(((uint8_t *) _ptr)[_prefix_size])){
            case H265Frame::NAL_VPS:
            case H265Frame::NAL_SPS:
            case H265Frame::NAL_PPS:return true;
            default:return false;
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
     */
    const string &getVps() const {
        return _vps;
    }

    /**
     * 返回不带0x00 00 00 01头的sps
     */
    const string &getSps() const {
        return _sps;
    }

    /**
     * 返回不带0x00 00 00 01头的pps
     */
    const string &getPps() const {
        return _pps;
    }

    CodecId getCodecId() const override {
        return CodecH265;
    }

    /**
     * 返回视频高度
     */
    int getVideoHeight() const override{
        return _height ;
    }

    /**
     * 返回视频宽度
     */
    int getVideoWidth() const override{
        return _width;
    }

    /**
     * 返回视频fps
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
        if(frame->configFrame() || type == H265Frame::NAL_SEI_PREFIX){
            splitH264(frame->data(), frame->size(), frame->prefixSize(), [&](const char *ptr, int len, int prefix){
                H265FrameInternal::Ptr sub_frame = std::make_shared<H265FrameInternal>(frame, (char*)ptr, len, prefix);
                inputFrame_l(sub_frame);
            });
        } else {
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
            vpsFrame->_prefix_size = 4;
            vpsFrame->_buffer.assign("\x0\x0\x0\x1", 4);
            vpsFrame->_buffer.append(_vps);
            vpsFrame->_dts = frame->dts();
            VideoTrack::inputFrame(vpsFrame);
        }
        if (!_sps.empty()) {
            auto spsFrame = std::make_shared<H265Frame>();
            spsFrame->_prefix_size = 4;
            spsFrame->_buffer.assign("\x0\x0\x0\x1", 4);
            spsFrame->_buffer.append(_sps);
            spsFrame->_dts = frame->dts();
            VideoTrack::inputFrame(spsFrame);
        }

        if (!_pps.empty()) {
            auto ppsFrame = std::make_shared<H265Frame>();
            ppsFrame->_prefix_size = 4;
            ppsFrame->_buffer.assign("\x0\x0\x0\x1", 4);
            ppsFrame->_buffer.append(_pps);
            ppsFrame->_dts = frame->dts();
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
     * 构造函数
     * @param sps 265 sps,不带0x00000001头
     * @param pps 265 pps,不带0x00000001头
     * @param payload_type  rtp payload type 默认96
     * @param bitrate 比特率
     */
    H265Sdp(const string &strVPS,
            const string &strSPS,
            const string &strPPS,
            int payload_type = 96,
            int bitrate = 4000) : Sdp(90000,payload_type) {
        //视频通道
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        _printer << "b=AS:" << bitrate << "\r\n";
        _printer << "a=rtpmap:" << payload_type << " H265/" << 90000 << "\r\n";
        _printer << "a=fmtp:" << payload_type << " ";
        _printer << "sprop-vps=";
        _printer << encodeBase64(strVPS) << "; ";
        _printer << "sprop-sps=";
        _printer << encodeBase64(strSPS) << "; ";
        _printer << "sprop-pps=";
        _printer << encodeBase64(strPPS) << "\r\n";
        _printer << "a=control:trackID=" << (int)TrackVideo << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    CodecId getCodecId() const override {
        return CodecH265;
    }
private:
    _StrPrinter _printer;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_H265_H