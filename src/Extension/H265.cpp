/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H265.h"
#include "SPSParser.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

bool getHEVCInfo(const char * vps, size_t vps_len,const char * sps,size_t sps_len,int &iVideoWidth, int &iVideoHeight, float  &iVideoFps){
    T_GetBitContext tGetBitBuf;
    T_HEVCSPS tH265SpsInfo;	
    T_HEVCVPS tH265VpsInfo;
    if ( vps_len > 2 ){
        memset(&tGetBitBuf,0,sizeof(tGetBitBuf));	
        memset(&tH265VpsInfo,0,sizeof(tH265VpsInfo));
        tGetBitBuf.pu8Buf = (uint8_t*)vps+2;
        tGetBitBuf.iBufSize = (int)(vps_len-2);
        if(0 != h265DecVideoParameterSet((void *) &tGetBitBuf, &tH265VpsInfo)){
            return false;
        }
    }

    if ( sps_len > 2 ){
        memset(&tGetBitBuf,0,sizeof(tGetBitBuf));
        memset(&tH265SpsInfo,0,sizeof(tH265SpsInfo));
        tGetBitBuf.pu8Buf = (uint8_t*)sps+2;
        tGetBitBuf.iBufSize = (int)(sps_len-2);
        if(0 != h265DecSeqParameterSet((void *) &tGetBitBuf, &tH265SpsInfo)){
            return false;
        }
    }
    else 
        return false;
    h265GetWidthHeight(&tH265SpsInfo, &iVideoWidth, &iVideoHeight);
    iVideoFps = 0;
    h265GeFramerate(&tH265VpsInfo, &tH265SpsInfo, &iVideoFps);
    return true;
}

bool getHEVCInfo(const string &strVps, const string &strSps, int &iVideoWidth, int &iVideoHeight, float &iVideoFps) {
    return getHEVCInfo(strVps.data(), strVps.size(), strSps.data(), strSps.size(), iVideoWidth, iVideoHeight,iVideoFps);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

H265Track::H265Track(const string &vps,const string &sps, const string &pps,int vps_prefix_len, int sps_prefix_len, int pps_prefix_len) {
    _vps = vps.substr(vps_prefix_len);
    _sps = sps.substr(sps_prefix_len);
    _pps = pps.substr(pps_prefix_len);
    onReady();
}

const string &H265Track::getVps() const {
    return _vps;
}

const string &H265Track::getSps() const {
    return _sps;
}

const string &H265Track::getPps() const {
    return _pps;
}

CodecId H265Track::getCodecId() const {
    return CodecH265;
}

int H265Track::getVideoHeight() const {
    return _height;
}

int H265Track::getVideoWidth() const {
    return _width;
}

float H265Track::getVideoFps() const {
    return _fps;
}

bool H265Track::ready() {
    return !_vps.empty() && !_sps.empty() && !_pps.empty();
}

bool H265Track::inputFrame(const Frame::Ptr &frame) {
    int type = H265_TYPE(frame->data()[frame->prefixSize()]);
    if (!frame->configFrame() && type != H265Frame::NAL_SEI_PREFIX && ready()) {
        return inputFrame_l(frame);
    }
    bool ret = false;
    splitH264(frame->data(), frame->size(), frame->prefixSize(), [&](const char *ptr, size_t len, size_t prefix) {
        using H265FrameInternal = FrameInternal<H265FrameNoCacheAble>;
        H265FrameInternal::Ptr sub_frame = std::make_shared<H265FrameInternal>(frame, (char *) ptr, len, prefix);
        if (inputFrame_l(sub_frame)) {
            ret = true;
        }
    });
    return ret;
}

bool H265Track::inputFrame_l(const Frame::Ptr &frame) {
    if (frame->keyFrame()) {
        insertConfigFrame(frame);
        _is_idr = true;
        return VideoTrack::inputFrame(frame);
    }

    _is_idr = false;
    bool ret = true;

    //非idr帧
    switch (H265_TYPE( frame->data()[frame->prefixSize()])) {
        case H265Frame::NAL_VPS: {
            _vps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            break;
        }
        case H265Frame::NAL_SPS: {
            _sps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            break;
        }
        case H265Frame::NAL_PPS: {
            _pps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            break;
        }
        default: {
            ret = VideoTrack::inputFrame(frame);
            break;
        }
    }
    if (_width == 0 && ready()) {
        onReady();
    }
    return ret;
}

void H265Track::onReady() {
    if (!getHEVCInfo(_vps, _sps, _width, _height, _fps)) {
        _vps.clear();
        _sps.clear();
        _pps.clear();
    }
}

Track::Ptr H265Track::clone() {
    return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this);
}

void H265Track::insertConfigFrame(const Frame::Ptr &frame) {
    if (_is_idr) {
        return;
    }
    if (!_vps.empty()) {
        auto vpsFrame = FrameImp::create<H265Frame>();
        vpsFrame->_prefix_size = 4;
        vpsFrame->_buffer.assign("\x00\x00\x00\x01", 4);
        vpsFrame->_buffer.append(_vps);
        vpsFrame->_dts = frame->dts();
        VideoTrack::inputFrame(vpsFrame);
    }
    if (!_sps.empty()) {
        auto spsFrame = FrameImp::create<H265Frame>();
        spsFrame->_prefix_size = 4;
        spsFrame->_buffer.assign("\x00\x00\x00\x01", 4);
        spsFrame->_buffer.append(_sps);
        spsFrame->_dts = frame->dts();
        VideoTrack::inputFrame(spsFrame);
    }

    if (!_pps.empty()) {
        auto ppsFrame = FrameImp::create<H265Frame>();
        ppsFrame->_prefix_size = 4;
        ppsFrame->_buffer.assign("\x00\x00\x00\x01", 4);
        ppsFrame->_buffer.append(_pps);
        ppsFrame->_dts = frame->dts();
        VideoTrack::inputFrame(ppsFrame);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
            int bitrate = 4000,
            int payload_type = 96) : Sdp(90000,payload_type) {
        //视频通道
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName() << "/" << 90000 << "\r\n";
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

Sdp::Ptr H265Track::getSdp() {
    if(!ready()){
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<H265Sdp>(getVps(), getSps(), getPps(), getBitRate() / 1024);
}

}//namespace mediakit

