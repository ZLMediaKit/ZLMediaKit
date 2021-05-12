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
    //for firefox
    csrc_audio_level = 9,
    //svc ?
    framemarking = 10,
    video_content_type = 11,
    playout_delay = 12,
    video_orientation = 13,
    toffset = 14,
    reserved = 15,
    // e2e ?
    encrypt = reserved
};

class RtcMedia;

//使用次对象的方法前需保证RtpHeader内存未释放
class RtpExt {
public:
    template<typename Type>
    friend void appendExt(map<uint8_t, RtpExt> &ret, uint8_t *ptr, const uint8_t *end);

    ~RtpExt() = default;

    static map<uint8_t/*id*/, RtpExt/*data*/> getExtValue(const RtpHeader *header);
    static RtpExtType getExtType(const string &url);
    static const string& getExtUrl(RtpExtType type);
    static const char *getExtName(RtpExtType type);

    void setType(RtpExtType type);
    string dumpString() const;

    uint8_t getAudioLevel(bool *vad) const;
    uint32_t getAbsSendTime() const;
    uint16_t getTransportCCSeq() const;
    string getSdesMid() const;
    string getRtpStreamId() const;
    string getRepairedRtpStreamId() const;

    void getVideoTiming(uint8_t &flags,
                        uint16_t &encode_start,
                        uint16_t &encode_finish,
                        uint16_t &packetization_complete,
                        uint16_t &last_pkt_left_pacer,
                        uint16_t &reserved_net0,
                        uint16_t &reserved_net1) const;

    uint8_t getVideoContentType() const;

    void getVideoOrientation(bool &camera_bit,
                             bool &flip_bit,
                             bool &first_rotation,
                             bool &second_rotation) const;

    void getPlayoutDelay(uint16_t &min_delay, uint16_t &max_delay) const;

    uint32_t getTransmissionOffset() const;

    uint8_t getFramemarkingTID() const;

    void setExtId(uint8_t ext_id);
    void clearExt();

private:
    RtpExt(void *ptr, bool one_byte_ext, const char *str, size_t size);
    const char *data() const;
    size_t size() const;
    const char& operator[](size_t pos) const;
    operator std::string() const;

private:
    void *_ext = nullptr;
    const char *_data;
    size_t _size;
    bool _one_byte_ext = true;
    RtpExtType _type = RtpExtType::padding;
};


#endif //ZLMEDIAKIT_RTPEXT_H
