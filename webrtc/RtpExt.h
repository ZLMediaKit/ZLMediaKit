/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPEXT_H
#define ZLMEDIAKIT_RTPEXT_H

#include <stdint.h>
#include <map>
#include <string>
#include "Common/macros.h"
#include "Rtsp/Rtsp.h"

using namespace std;
using namespace mediakit;

enum class RtpExtType : uint8_t {
    padding = 0,
    ssrc_audio_level = 1,
    abs_send_time = 2,
    transport_cc = 3,
    sdes_mid = 4,
    sdes_rtp_stream_id = 5,
    sdes_repaired_rtp_stream_id = 6,
    video_timing = 7,
    color_space = 8,
    video_content_type = 11,
    playout_delay = 12,
    video_orientation = 13,
    toffset = 14,
    reserved = 15,
};

class RtcMedia;

class RtpExt : public std::string {
public:
    ~RtpExt() = default;
    static map<uint8_t/*id*/, RtpExt/*data*/> getExtValue(const RtpHeader *header, const RtcMedia &media);
    static RtpExtType getExtType(const string &url);
    static const string& getExtUrl(RtpExtType type);

private:
    RtpExt(RtpExtType type, const char *str, size_t size) : std::string(str, size), _type(type) {}

private:
    RtpExtType _type;
};


#endif //ZLMEDIAKIT_RTPEXT_H
