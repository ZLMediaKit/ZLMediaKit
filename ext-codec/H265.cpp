/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H265.h"
#include "H265Rtp.h"
#include "H265Rtmp.h"
#include "SPSParser.h"
#include "Util/base64.h"
#include "Common/Parser.h"
#include "Extension/Factory.h"

#ifdef ENABLE_MP4
#include "mpeg4-hevc.h"
#endif

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
    update();
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

bool H265Track::ready() const {
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
        update();
    }
    return ret;
}

toolkit::Buffer::Ptr H265Track::getExtraData() const {
    CHECK(ready());
#ifdef ENABLE_MP4
    struct mpeg4_hevc_t hevc;
    memset(&hevc, 0, sizeof(hevc));
    string vps_sps_pps = string("\x00\x00\x00\x01", 4) + _vps + string("\x00\x00\x00\x01", 4) + _sps + string("\x00\x00\x00\x01", 4) + _pps;
    h265_annexbtomp4(&hevc, vps_sps_pps.data(), (int) vps_sps_pps.size(), NULL, 0, NULL, NULL);

    std::string extra_data;
    extra_data.resize(1024);
    auto extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, (uint8_t *)extra_data.data(), extra_data.size());
    if (extra_data_size == -1) {
        WarnL << "生成H265 extra_data 失败";
        return nullptr;
    }
    return std::make_shared<BufferString>(std::move(extra_data));
#else
    WarnL << "请开启MP4相关功能并使能\"ENABLE_MP4\",否则对H265的支持不完善";
    return nullptr;
#endif
}

void H265Track::setExtraData(const uint8_t *data, size_t bytes) {
#ifdef ENABLE_MP4
    struct mpeg4_hevc_t hevc;
    memset(&hevc, 0, sizeof(hevc));
    if (mpeg4_hevc_decoder_configuration_record_load(data, bytes, &hevc) > 0) {
        std::vector<uint8_t> config(bytes * 2);
        int size = mpeg4_hevc_to_nalu(&hevc, config.data(), bytes * 2);
        if (size > 4) {
            splitH264((char *)config.data(), size, 4, [&](const char *ptr, size_t len, size_t prefix) {
                inputFrame_l(std::make_shared<H265FrameNoCacheAble>((char *)ptr, len, 0, 0, prefix));
            });
            update();
        }
    }
#else
    WarnL << "请开启MP4相关功能并使能\"ENABLE_MP4\",否则对H265的支持不完善";
#endif
}

bool H265Track::update() {
    return getHEVCInfo(_vps, _sps, _width, _height, _fps);
}

Track::Ptr H265Track::clone() const {
    return std::make_shared<H265Track>(*this);
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
        vpsFrame->setIndex(frame->getIndex());
        VideoTrack::inputFrame(vpsFrame);
    }
    if (!_sps.empty()) {
        auto spsFrame = FrameImp::create<H265Frame>();
        spsFrame->_prefix_size = 4;
        spsFrame->_buffer.assign("\x00\x00\x00\x01", 4);
        spsFrame->_buffer.append(_sps);
        spsFrame->_dts = frame->dts();
        spsFrame->setIndex(frame->getIndex());
        VideoTrack::inputFrame(spsFrame);
    }

    if (!_pps.empty()) {
        auto ppsFrame = FrameImp::create<H265Frame>();
        ppsFrame->_prefix_size = 4;
        ppsFrame->_buffer.assign("\x00\x00\x00\x01", 4);
        ppsFrame->_buffer.append(_pps);
        ppsFrame->_dts = frame->dts();
        ppsFrame->setIndex(frame->getIndex());
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
    H265Sdp(const string &strVPS, const string &strSPS, const string &strPPS, int payload_type, int bitrate) : Sdp(90000, payload_type) {
        //视频通道
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecH265) << "/" << 90000 << "\r\n";
        _printer << "a=fmtp:" << payload_type << " ";
        _printer << "sprop-vps=";
        _printer << encodeBase64(strVPS) << "; ";
        _printer << "sprop-sps=";
        _printer << encodeBase64(strSPS) << "; ";
        _printer << "sprop-pps=";
        _printer << encodeBase64(strPPS) << "\r\n";
    }

    string getSdp() const override { return _printer; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr H265Track::getSdp(uint8_t payload_type) const {
    if (!ready()) {
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<H265Sdp>(_vps, _sps, _pps, payload_type, getBitRate() / 1024);
}

namespace {

CodecId getCodec() {
    return CodecH265;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<H265Track>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    // a=fmtp:96 sprop-sps=QgEBAWAAAAMAsAAAAwAAAwBdoAKAgC0WNrkky/AIAAADAAgAAAMBlQg=; sprop-pps=RAHA8vA8kAA=
    auto map = Parser::parseArgs(track->_fmtp, ";", "=");
    auto vps = decodeBase64(map["sprop-vps"]);
    auto sps = decodeBase64(map["sprop-sps"]);
    auto pps = decodeBase64(map["sprop-pps"]);
    if (sps.empty() || pps.empty()) {
        // 如果sdp里面没有sps/pps,那么可能在后续的rtp里面恢复出sps/pps
        return std::make_shared<H265Track>();
    }
    return std::make_shared<H265Track>(vps, sps, pps, 0, 0, 0);
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<H265RtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<H265RtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<H265RtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<H265RtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<H265FrameNoCacheAble>((char *)data, bytes, dts, pts, prefixSize(data, bytes));
}

} // namespace

CodecPlugin h265_plugin = { getCodec,
                            getTrackByCodecId,
                            getTrackBySdp,
                            getRtpEncoderByCodecId,
                            getRtpDecoderByCodecId,
                            getRtmpEncoderByTrack,
                            getRtmpDecoderByTrack,
                            getFrameFromPtr };

}//namespace mediakit

