/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H264.h"
#include "SPSParser.h"
#include "Util/logger.h"
using namespace toolkit;
using namespace std;

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
    onReady();
}

H264Track::H264Track(const Frame::Ptr &sps, const Frame::Ptr &pps) {
    if (sps->getCodecId() != CodecH264 || pps->getCodecId() != CodecH264) {
        throw std::invalid_argument("必须输入H264类型的帧");
    }
    _sps = string(sps->data() + sps->prefixSize(), sps->size() - sps->prefixSize());
    _pps = string(pps->data() + pps->prefixSize(), pps->size() - pps->prefixSize());
    onReady();
}

const string &H264Track::getSps() const {
    return _sps;
}

const string &H264Track::getPps() const {
    return _pps;
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

bool H264Track::ready() {
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

void H264Track::onReady() {
    if (!getAVCInfo(_sps, _width, _height, _fps)) {
        _sps.clear();
        _pps.clear();
    }
}

Track::Ptr H264Track::clone() {
    return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this);
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
            _latest_is_config_frame = false;
            ret = VideoTrack::inputFrame(frame);
            break;
    }

    if (_width == 0 && ready()) {
        onReady();
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
        VideoTrack::inputFrame(spsFrame);
    }

    if (!_pps.empty()) {
        auto ppsFrame = FrameImp::create<H264Frame>();
        ppsFrame->_prefix_size = 4;
        ppsFrame->_buffer.assign("\x00\x00\x00\x01", 4);
        ppsFrame->_buffer.append(_pps);
        ppsFrame->_dts = frame->dts();
        VideoTrack::inputFrame(ppsFrame);
    }
}

class H264Sdp : public Sdp {
public:
    H264Sdp(const string &strSPS, const string &strPPS, int bitrate, int payload_type = 96)
        : Sdp(90000, payload_type) {
        //视频通道
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName() << "/" << 90000 << "\r\n";
        _printer << "a=fmtp:" << payload_type << " packetization-mode=1; profile-level-id=";

        char strTemp[1024];
        uint32_t profile_level_id = 0;
        if (strSPS.length() >= 4) { // sanity check
            profile_level_id = (uint8_t(strSPS[1]) << 16) |
                               (uint8_t(strSPS[2]) << 8) |
                               (uint8_t(strSPS[3])); // profile_idc|constraint_setN_flag|level_idc
        }
        memset(strTemp, 0, sizeof(strTemp));
        snprintf(strTemp, sizeof(strTemp), "%06X", profile_level_id);
        _printer << strTemp;
        _printer << "; sprop-parameter-sets=";
        memset(strTemp, 0, sizeof(strTemp));
        av_base64_encode(strTemp, sizeof(strTemp), (uint8_t *)strSPS.data(), (int)strSPS.size());
        _printer << strTemp << ",";
        memset(strTemp, 0, sizeof(strTemp));
        av_base64_encode(strTemp, sizeof(strTemp), (uint8_t *)strPPS.data(), (int)strPPS.size());
        _printer << strTemp << "\r\n";
        _printer << "a=control:trackID=" << (int)TrackVideo << "\r\n";
    }

    string getSdp() const { return _printer; }

    CodecId getCodecId() const { return CodecH264; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr H264Track::getSdp() {
    if (!ready()) {
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<H264Sdp>(getSps(), getPps(), getBitRate() / 1024);
}

} // namespace mediakit
