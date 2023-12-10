/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H264.h"
#include "H264Rtmp.h"
#include "H264Rtp.h"
#include "SPSParser.h"
#include "Util/logger.h"
#include "Util/base64.h"
#include "Common/Parser.h"
#include "Common/config.h"
#include "Extension/Factory.h"

#ifdef ENABLE_MP4
#include "mpeg4-avc.h"
#endif

using namespace std;
using namespace toolkit;

namespace mediakit {

static bool getAVCInfo(const char *sps, size_t sps_len, int &iVideoWidth, int &iVideoHeight, float &iVideoFps) {
    if (sps_len < 4) {
        return false;
    }
    T_GetBitContext tGetBitBuf;
    T_SPS tH264SpsInfo;
    memset(&tGetBitBuf, 0, sizeof(tGetBitBuf));
    memset(&tH264SpsInfo, 0, sizeof(tH264SpsInfo));
    tGetBitBuf.pu8Buf = (uint8_t *)sps + 1;
    tGetBitBuf.iBufSize = (int)(sps_len - 1);
    if (0 != h264DecSeqParameterSet((void *)&tGetBitBuf, &tH264SpsInfo)) {
        return false;
    }
    h264GetWidthHeight(&tH264SpsInfo, &iVideoWidth, &iVideoHeight);
    h264GeFramerate(&tH264SpsInfo, &iVideoFps);
    // ErrorL << iVideoWidth << " " << iVideoHeight << " " << iVideoFps;
    return true;
}

bool getAVCInfo(const string &strSps, int &iVideoWidth, int &iVideoHeight, float &iVideoFps) {
    return getAVCInfo(strSps.data(), strSps.size(), iVideoWidth, iVideoHeight, iVideoFps);
}

static const char *memfind(const char *buf, ssize_t len, const char *subbuf, ssize_t sublen) {
    for (auto i = 0; i < len - sublen; ++i) {
        if (memcmp(buf + i, subbuf, sublen) == 0) {
            return buf + i;
        }
    }
    return NULL;
}

void splitH264(
    const char *ptr, size_t len, size_t prefix, const std::function<void(const char *, size_t, size_t)> &cb) {
    auto start = ptr + prefix;
    auto end = ptr + len;
    size_t next_prefix;
    while (true) {
        auto next_start = memfind(start, end - start, "\x00\x00\x01", 3);
        if (next_start) {
            //找到下一帧
            if (*(next_start - 1) == 0x00) {
                //这个是00 00 00 01开头
                next_start -= 1;
                next_prefix = 4;
            } else {
                //这个是00 00 01开头
                next_prefix = 3;
            }
            //记得加上本帧prefix长度
            cb(start - prefix, next_start - start + prefix, prefix);
            //搜索下一帧末尾的起始位置
            start = next_start + next_prefix;
            //记录下一帧的prefix长度
            prefix = next_prefix;
            continue;
        }
        //未找到下一帧,这是最后一帧
        cb(start - prefix, end - start + prefix, prefix);
        break;
    }
}

size_t prefixSize(const char *ptr, size_t len) {
    if (len < 4) {
        return 0;
    }

    if (ptr[0] != 0x00 || ptr[1] != 0x00) {
        //不是0x00 00开头
        return 0;
    }

    if (ptr[2] == 0x00 && ptr[3] == 0x01) {
        //是0x00 00 00 01
        return 4;
    }

    if (ptr[2] == 0x01) {
        //是0x00 00 01
        return 3;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

H264Track::H264Track(const string &sps, const string &pps, int sps_prefix_len, int pps_prefix_len) {
    _sps = sps.substr(sps_prefix_len);
    _pps = pps.substr(pps_prefix_len);
    update();
}

CodecId H264Track::getCodecId() const {
    return CodecH264;
}

int H264Track::getVideoHeight() const {
    return _height;
}

int H264Track::getVideoWidth() const {
    return _width;
}

float H264Track::getVideoFps() const {
    return _fps;
}

bool H264Track::ready() const {
    return !_sps.empty() && !_pps.empty();
}

bool H264Track::inputFrame(const Frame::Ptr &frame) {
    using H264FrameInternal = FrameInternal<H264FrameNoCacheAble>;
    int type = H264_TYPE(frame->data()[frame->prefixSize()]);
   
    if ((type == H264Frame::NAL_B_P || type == H264Frame::NAL_IDR) && ready()) {
        return inputFrame_l(frame);
    }

    //非I/B/P帧情况下，split一下，防止多个帧粘合在一起
    bool ret = false;
    splitH264(frame->data(), frame->size(), frame->prefixSize(), [&](const char *ptr, size_t len, size_t prefix) {
        H264FrameInternal::Ptr sub_frame = std::make_shared<H264FrameInternal>(frame, (char *)ptr, len, prefix);
        if (inputFrame_l(sub_frame)) {
            ret = true;
        }
    });
    return ret;
}

toolkit::Buffer::Ptr H264Track::getExtraData() const {
    CHECK(ready());

#ifdef ENABLE_MP4
    struct mpeg4_avc_t avc;
    memset(&avc, 0, sizeof(avc));
    string sps_pps = string("\x00\x00\x00\x01", 4) + _sps + string("\x00\x00\x00\x01", 4) + _pps;
    h264_annexbtomp4(&avc, sps_pps.data(), (int)sps_pps.size(), NULL, 0, NULL, NULL);

    std::string extra_data;
    extra_data.resize(1024);
    auto extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, (uint8_t *)extra_data.data(), extra_data.size());
    if (extra_data_size == -1) {
        WarnL << "生成H264 extra_data 失败";
        return nullptr;
    }
    extra_data.resize(extra_data_size);
    return std::make_shared<BufferString>(std::move(extra_data));
#else
    std::string extra_data;
    // AVCDecoderConfigurationRecord start
    extra_data.push_back(1); // version
    extra_data.push_back(_sps[1]); // profile
    extra_data.push_back(_sps[2]); // compat
    extra_data.push_back(_sps[3]); // level
    extra_data.push_back((char)0xff); // 6 bits reserved + 2 bits nal size length - 1 (11)
    extra_data.push_back((char)0xe1); // 3 bits reserved + 5 bits number of sps (00001)
    // sps
    uint16_t size = (uint16_t)_sps.size();
    size = htons(size);
    extra_data.append((char *)&size, 2);
    extra_data.append(_sps);
    // pps
    extra_data.push_back(1); // version
    size = (uint16_t)_pps.size();
    size = htons(size);
    extra_data.append((char *)&size, 2);
    extra_data.append(_pps);
    return std::make_shared<BufferString>(std::move(extra_data));
#endif
}

void H264Track::setExtraData(const uint8_t *data, size_t bytes) {
#ifdef ENABLE_MP4
    struct mpeg4_avc_t avc;
    memset(&avc, 0, sizeof(avc));
    if (mpeg4_avc_decoder_configuration_record_load(data, bytes, &avc) > 0) {
        std::vector<uint8_t> config(bytes * 2);
        int size = mpeg4_avc_to_nalu(&avc, config.data(), bytes * 2);
        if (size > 4) {
            splitH264((char *)config.data(), size, 4, [&](const char *ptr, size_t len, size_t prefix) {
                inputFrame_l(std::make_shared<H264FrameNoCacheAble>((char *)ptr, len, 0, 0, prefix));
            });
            update();
        }
    }
#else
    CHECK(bytes >= 8); // 6 + 2
    size_t offset = 6;

    uint16_t sps_size = data[offset] << 8 | data[offset + 1];
    auto sps_ptr = data + offset + 2;
    offset += (2 + sps_size);
    CHECK(bytes >= offset + 2); // + pps_size
    _sps.assign((char *)sps_ptr, sps_size);

    uint16_t pps_size = data[offset] << 8 | data[offset + 1];
    auto pps_ptr = data + offset + 2;
    offset += (2 + pps_size);
    CHECK(bytes >= offset);
    _pps.assign((char *)pps_ptr, pps_size);
    update();
#endif
}

bool H264Track::update() {
    return getAVCInfo(_sps, _width, _height, _fps);
}

Track::Ptr H264Track::clone() const {
    return std::make_shared<H264Track>(*this);
}

bool H264Track::inputFrame_l(const Frame::Ptr &frame) {
    int type = H264_TYPE(frame->data()[frame->prefixSize()]);
    bool ret = true;
    switch (type) {
        case H264Frame::NAL_SPS: {
            _sps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_config_frame = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        case H264Frame::NAL_PPS: {
            _pps = string(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            _latest_is_config_frame = true;
            ret = VideoTrack::inputFrame(frame);
            break;
        }
        default:
            // 避免识别不出关键帧
            if (_latest_is_config_frame && !frame->dropAble()) {
                if (!frame->keyFrame()) {
                    const_cast<Frame::Ptr &>(frame) = std::make_shared<FrameCacheAble>(frame, true);
                }
            }
            // 判断是否是I帧, 并且如果是,那判断前面是否插入过config帧, 如果插入过就不插入了
            if (frame->keyFrame() && !_latest_is_config_frame) {
                insertConfigFrame(frame);
            }
            if(!frame->dropAble()){
                _latest_is_config_frame = false;
            }
            ret = VideoTrack::inputFrame(frame);
            break;
    }

    if (_width == 0 && ready()) {
        update();
    }
    return ret;
}

void H264Track::insertConfigFrame(const Frame::Ptr &frame) {
    if (!_sps.empty()) {
        auto spsFrame = FrameImp::create<H264Frame>();
        spsFrame->_prefix_size = 4;
        spsFrame->_buffer.assign("\x00\x00\x00\x01", 4);
        spsFrame->_buffer.append(_sps);
        spsFrame->_dts = frame->dts();
        spsFrame->setIndex(frame->getIndex());
        VideoTrack::inputFrame(spsFrame);
    }

    if (!_pps.empty()) {
        auto ppsFrame = FrameImp::create<H264Frame>();
        ppsFrame->_prefix_size = 4;
        ppsFrame->_buffer.assign("\x00\x00\x00\x01", 4);
        ppsFrame->_buffer.append(_pps);
        ppsFrame->_dts = frame->dts();
        ppsFrame->setIndex(frame->getIndex());
        VideoTrack::inputFrame(ppsFrame);
    }
}

class H264Sdp : public Sdp {
public:
    H264Sdp(const string &strSPS, const string &strPPS, int payload_type, int bitrate) : Sdp(90000, payload_type) {
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecH264) << "/" << 90000 << "\r\n";

        /**
         Single NAI Unit Mode = 0. // Single NAI mode (Only nals from 1-23 are allowed)
         Non Interleaved Mode = 1，// Non-interleaved Mode: 1-23，24 (STAP-A)，28 (FU-A) are allowed
         Interleaved Mode = 2,  // 25 (STAP-B)，26 (MTAP16)，27 (MTAP24)，28 (EU-A)，and 29 (EU-B) are allowed.
         **/
        GET_CONFIG(bool, h264_stap_a, Rtp::kH264StapA);
        _printer << "a=fmtp:" << payload_type << " packetization-mode=" << h264_stap_a << "; profile-level-id=";

        uint32_t profile_level_id = 0;
        if (strSPS.length() >= 4) { // sanity check
            profile_level_id = (uint8_t(strSPS[1]) << 16) |
                               (uint8_t(strSPS[2]) << 8) |
                               (uint8_t(strSPS[3])); // profile_idc|constraint_setN_flag|level_idc
        }

        char profile[8];
        snprintf(profile, sizeof(profile), "%06X", profile_level_id);
        _printer << profile;
        _printer << "; sprop-parameter-sets=";
        _printer << encodeBase64(strSPS) << ",";
        _printer << encodeBase64(strPPS) << "\r\n";
    }

    string getSdp() const { return _printer; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr H264Track::getSdp(uint8_t payload_type) const {
    if (!ready()) {
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<H264Sdp>(_sps, _pps, payload_type, getBitRate() / 1024);
}

namespace {

CodecId getCodec() {
    return CodecH264;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<H264Track>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    //a=fmtp:96 packetization-mode=1;profile-level-id=42C01F;sprop-parameter-sets=Z0LAH9oBQBboQAAAAwBAAAAPI8YMqA==,aM48gA==
    auto map = Parser::parseArgs(track->_fmtp, ";", "=");
    auto sps_pps = map["sprop-parameter-sets"];
    string base64_SPS = findSubString(sps_pps.data(), NULL, ",");
    string base64_PPS = findSubString(sps_pps.data(), ",", NULL);
    auto sps = decodeBase64(base64_SPS);
    auto pps = decodeBase64(base64_PPS);
    if (sps.empty() || pps.empty()) {
        //如果sdp里面没有sps/pps,那么可能在后续的rtp里面恢复出sps/pps
        return std::make_shared<H264Track>();
    }
    return std::make_shared<H264Track>(sps, pps, 0, 0);
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<H264RtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<H264RtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<H264RtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<H264RtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<H264FrameNoCacheAble>((char *)data, bytes, dts, pts, prefixSize(data, bytes));
}

} // namespace

CodecPlugin h264_plugin = { getCodec,
                            getTrackByCodecId,
                            getTrackBySdp,
                            getRtpEncoderByCodecId,
                            getRtpDecoderByCodecId,
                            getRtmpEncoderByTrack,
                            getRtmpDecoderByTrack,
                            getFrameFromPtr };

} // namespace mediakit
