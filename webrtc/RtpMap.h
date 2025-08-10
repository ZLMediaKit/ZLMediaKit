/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPMAP_H
#define ZLMEDIAKIT_RTPMAP_H

#include <set>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include "assert.h"
#include "Extension/Frame.h"

namespace mediakit {

class RtpMap {
public:
    using Ptr = std::shared_ptr<RtpMap>;
    RtpMap(const std::string code_name, uint32_t payload, uint32_t clock_rate) :
       _code_name (code_name), _payload(payload), _clock_rate(clock_rate) {
    }
    virtual ~RtpMap() = default;

    virtual TrackType getType() = 0;

    virtual std::map <std::string /*key*/, std::string /*value*/> getFmtp() {
        return _fmtp;
    }

    std::string getCodeName() {return _code_name;}
    uint32_t getPayload() {return _payload;}
    uint32_t getClockRate() {return _clock_rate;}

protected:
    std::map<std::string /*key*/, std::string /*value*/> _fmtp;
    std::string _code_name;
    uint32_t _payload;
    uint32_t _clock_rate;
};

class VideoRtpMap :public RtpMap {
public:
    VideoRtpMap(const std::string codeName, uint32_t payload, uint32_t clock_rate) 
        : RtpMap(codeName, payload, clock_rate) {};

    TrackType getType() override { return TrackVideo;};
};

class AudioRtpMap : public RtpMap {
public:
    AudioRtpMap(const std::string codeName, uint32_t payload, uint32_t clock_rate) 
       : RtpMap(codeName, payload, clock_rate) {};

    TrackType getType() override { return TrackAudio;};
};

#define H264_PROFILE_IDC_MAP(XX) \
    XX(PROFILE_H264_BASELINE,  66,  "baseline")      \
    XX(PROFILE_H264_MAIN,      77,  "main")          \
    XX(PROFILE_H264_HIGH,      100, "high")          \
    XX(PROFILE_H264_HIGH10,    110, "high10")        \
    XX(PROFILE_H264_HIGH422,   122, "high422")       \
    XX(PROFILE_H264_HIGH444,   244, "high444")       \

typedef enum {
    H264ProfileIdcInvalid = -1,
#define XX(name, value, str) name = value,
    H264_PROFILE_IDC_MAP(XX)
#undef XX
    H264ProfileIdcMax
} H264ProfileIdc;

#define H264_PROFILE_LEVEL_MAP(XX) \
    XX(10) \
    XX(20) \
    XX(30) \
    XX(31) \
    XX(40) \
    XX(41) \
    XX(50) \
    XX(51)

typedef enum {
    H264ProfileLevelInvalid = -1,
#define XX(value) H264_PROFILE_LEVEL_##value = value,
    H264_PROFILE_LEVEL_MAP(XX)
#undef XX
    H264ProfileLevelMax
} H264ProfileLevel;

class H264RtpMap :public VideoRtpMap {
public:
    H264RtpMap(uint32_t payload, uint32_t clock_rate, H264ProfileIdc profile_idc) 
       : VideoRtpMap("H264", payload, clock_rate), _profile_idc(profile_idc) {
        _fmtp.emplace("level-asymmetry-allowed", "1");
        _fmtp.emplace("packetization-mode", "1");

        toolkit::_StrPrinter printer;
        printer << std::setw(2) << std::setfill('0') << std::hex << _profile_idc;
        printer << std::setw(2) << std::setfill('0') << std::hex << _profile_iop;
        printer << std::setw(2) << std::setfill('0') << std::hex << _profile_level;
        _fmtp.emplace("profile-level-id", printer);
    };

private:
    H264ProfileIdc _profile_idc;
    int _profile_iop = 0;
    H264ProfileLevel _profile_level = H264_PROFILE_LEVEL_31;

};

class H265RtpMap :public VideoRtpMap {
public:
    H265RtpMap(uint32_t payload, uint32_t clock_rate, H264ProfileIdc profile_idc) 
       : VideoRtpMap("H264", payload, clock_rate), _profile_idc(profile_idc) {
        _fmtp.emplace("level-asymmetry-allowed", "1");
        _fmtp.emplace("packetization-mode", "1");

        toolkit::_StrPrinter printer;
        printer << std::setw(2) << std::setfill('0') << std::hex << _profile_idc;
        printer << std::setw(2) << std::setfill('0') << std::hex << _profile_iop;
        printer << std::setw(2) << std::setfill('0') << std::hex << _profile_level;
        _fmtp.emplace("profile-level-id", printer);
    }

private:
    H264ProfileIdc _profile_idc;
    int _profile_iop = 0;
    H264ProfileLevel _profile_level = H264_PROFILE_LEVEL_31;

};

class VP9RtpMap :public VideoRtpMap {
public:
    VP9RtpMap(uint32_t payload, uint32_t clock_rate, int profile_id) 
       : VideoRtpMap("VP9", payload, clock_rate), _profile_id(profile_id) {
        _fmtp.emplace("profile-id=", std::to_string(_profile_id));
    };

private:
    int _profile_id = 1;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_RTPMAP_H
