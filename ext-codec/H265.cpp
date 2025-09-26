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
    H265Track::update();
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
    int type = H265_TYPE(frame->data()[frame->prefixSize()]);
    bool ret = true;
    switch (type) {
        case H265Frame::NAL_VPS: {
            _vps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_config_frame = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        case H265Frame::NAL_SPS: {
            _sps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_config_frame = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        case H265Frame::NAL_PPS: {
            _pps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_config_frame = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        default: {
            // 判断是否是I帧, 并且如果是,那判断前面是否插入过config帧, 如果插入过就不插入了
            if (frame->keyFrame() && !_latest_is_config_frame) {
                insertConfigFrame(frame);
            }
            if (!frame->dropAble()) {
                _latest_is_config_frame = false;
            }
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
    extra_data.resize(extra_data_size);
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

std::vector<Frame::Ptr> H265Track::getConfigFrames() const {
    if (!ready()) {
        return {};
    }
    return { createConfigFrame<H265Frame>(_vps, 0, getIndex()),
             createConfigFrame<H265Frame>(_sps, 0, getIndex()),
             createConfigFrame<H265Frame>(_pps, 0, getIndex()) };
}

Track::Ptr H265Track::clone() const {
    return std::make_shared<H265Track>(*this);
}

void H265Track::insertConfigFrame(const Frame::Ptr &frame) {
    if (!_vps.empty()) {
        VideoTrack::inputFrame(createConfigFrame<H265Frame>(_vps, frame->dts(), frame->getIndex()));
    }
    if (!_sps.empty()) {
        VideoTrack::inputFrame(createConfigFrame<H265Frame>(_sps, frame->dts(), frame->getIndex()));
    }
    if (!_pps.empty()) {
        VideoTrack::inputFrame(createConfigFrame<H265Frame>(_pps, frame->dts(), frame->getIndex()));
    }
}

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size) : _data(data), _size(size), _bitPos(0) {}

    uint32_t readBits(int n) {
        uint32_t result = 0;
        for (int i = 0; i < n; i++) {
            if (_bitPos >= _size * 8) throw std::runtime_error("Out of range");
            int bytePos = _bitPos / 8;
            int bitOffset = 7 - (_bitPos % 8);
            result = (result << 1) | ((_data[bytePos] >> bitOffset) & 0x01);
            _bitPos++;
        }
        return result;
    }

    void skipBits(int n) {
        _bitPos += n;
        if (_bitPos > _size * 8) throw std::runtime_error("Skip out of range");
    }

private:
    const uint8_t* _data;
    size_t _size;
    size_t _bitPos;
};

struct HevcProfileInfo {
    int profile_id = -1; // profile-id
    int level_id   = -1; // level-id
    int tier_flag  = -1; // tier-flag
};

// 移除 00 00 03 防竞争字节
std::vector<uint8_t> removeEmulationPrevention(const uint8_t *data, size_t size) {
    std::vector<uint8_t> out;
    out.reserve(size);
    for (size_t i = 0; i < size; i++) {
        if (i + 2 < size && data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x03) {
            out.push_back(0x00);
            out.push_back(0x00);
            i += 2; // skip 0x00 0x00 0x03
        } else {
            out.push_back(data[i]);
        }
    }
    return out;
}

// 从 VPS 或 SPS 里提取 profile/level/tier 信息
HevcProfileInfo parse_hevc_profile_tier_level(const uint8_t *nalu, size_t size) {
    // 去掉起始码 (00 00 01 或 00 00 00 01)
    size_t offset = 0;
    if (size > 4 && nalu[0] == 0x00 && nalu[1] == 0x00) {
        if (nalu[2] == 0x01)
            offset = 3;
        else if (nalu[2] == 0x00 && nalu[3] == 0x01)
            offset = 4;
    }

    auto rbsp = removeEmulationPrevention(nalu + offset, size - offset);
    BitReader br(rbsp.data(), rbsp.size());

    // ---- NALU header ----
    br.skipBits(1 + 6 + 6 + 3); // forbidden_zero_bit + nal_unit_type + nuh_layer_id + nuh_temporal_id_plus1

    // VPS 和 SPS 都包含 profile_tier_level()
    // 先解析最少需要的部分

    // vps_video_parameter_set_id 或 sps_video_parameter_set_id (略过)
    br.readBits(4);

    // sps 里还有 sps_max_sub_layers_minus1
    uint32_t max_sub_layers_minus1 = br.readBits(3);
    // temporal_id_nesting_flag
    br.readBits(1);

    // ---- profile_tier_level ----
    HevcProfileInfo info;
    uint32_t profile_space = br.readBits(2); // general_profile_space
    info.tier_flag = br.readBits(1); // general_tier_flag
    info.profile_id = br.readBits(5); // general_profile_idc

    // general_profile_compatibility_flag[32]
    for (int i = 0; i < 32; i++)
        br.readBits(1);

    // general_progressive_source_flag 等 (跳过)
    br.readBits(1); // progressive_source_flag
    br.readBits(1); // interlaced_source_flag
    br.readBits(1); // non_packed_constraint_flag
    br.readBits(1); // frame_only_constraint_flag

    // general_reserved_zero_44bits
    br.skipBits(44);

    // general_level_idc (8 bits)
    info.level_id = br.readBits(8);

    return info;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * h265类型sdp
 * h265 type sdp
 
 * [AUTO-TRANSLATED:4418a7df]
 */
class H265Sdp : public Sdp {
public:
    /**
     * 构造函数
     * @param sps 265 sps,不带0x00000001头
     * @param pps 265 pps,不带0x00000001头
     * @param payload_type  rtp payload type 默认96
     * @param bitrate 比特率
     * Constructor
     * @param sps 265 sps, without 0x00000001 header
     * @param pps 265 pps, without 0x00000001 header
     * @param payload_type  rtp payload type, default 96
     * @param bitrate Bitrate
     
     * [AUTO-TRANSLATED:93f4ec48]
     */
    H265Sdp(const string &strVPS, const string &strSPS, const string &strPPS, int payload_type, int bitrate) : Sdp(90000, payload_type) {
        // 视频通道  [AUTO-TRANSLATED:642ca881]
        // Video channel
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecH265) << "/" << 90000 << "\r\n";

        auto info = parse_hevc_profile_tier_level((uint8_t *)strSPS.data(), strSPS.size());
        _printer << "a=fmtp:" << payload_type << " level-id=" << info.level_id << "; profile-id=" << info.profile_id << "; tier-flag=" << info.tier_flag << "; ";
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
    return std::make_shared<H265Sdp>(_vps, _sps, _pps, payload_type, getBitRate() >> 10);
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
        // 如果sdp里面没有sps/pps,那么可能在后续的rtp里面恢复出sps/pps  [AUTO-TRANSLATED:9300510b]
        // If there is no sps/pps in the sdp, then it may be possible to recover sps/pps from the subsequent rtp
        return std::make_shared<H265Track>();
    }
    return std::make_shared<H265Track>(vps, sps, pps,
                                       prefixSize(vps.data(), vps.size()),
                                       prefixSize(sps.data(), sps.size()),
                                       prefixSize(pps.data(), pps.size()));
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

