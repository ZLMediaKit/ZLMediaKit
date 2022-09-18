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

namespace mediakit {

#define RTP_EXT_MAP(XX) \
    XX(ssrc_audio_level,            "urn:ietf:params:rtp-hdrext:ssrc-audio-level") \
    XX(abs_send_time,               "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time") \
    XX(transport_cc,                "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01") \
    XX(sdes_mid,                    "urn:ietf:params:rtp-hdrext:sdes:mid") \
    XX(sdes_rtp_stream_id,          "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id") \
    XX(sdes_repaired_rtp_stream_id, "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id") \
    XX(video_timing,                "http://www.webrtc.org/experiments/rtp-hdrext/video-timing") \
    XX(color_space,                 "http://www.webrtc.org/experiments/rtp-hdrext/color-space") \
    XX(csrc_audio_level,            "urn:ietf:params:rtp-hdrext:csrc-audio-level") \
    XX(framemarking,                "http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07") \
    XX(video_content_type,          "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type") \
    XX(playout_delay,               "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay") \
    XX(video_orientation,           "urn:3gpp:video-orientation") \
    XX(toffset,                     "urn:ietf:params:rtp-hdrext:toffset") \
    XX(encrypt,                     "urn:ietf:params:rtp-hdrext:encrypt")

enum class RtpExtType : uint8_t {
    padding = 0,
#define XX(type, uri) type,
    RTP_EXT_MAP(XX)
#undef XX
    reserved = encrypt,
};

class RtcMedia;

//使用次对象的方法前需保证RtpHeader内存未释放
class RtpExt {
public:
    template<typename Type>
    friend void appendExt(std::map<uint8_t, RtpExt> &ret, uint8_t *ptr, const uint8_t *end);
    friend class RtpExtContext;
    ~RtpExt() = default;

    static std::map<uint8_t/*id*/, RtpExt/*data*/> getExtValue(const RtpHeader *header);
    static RtpExtType getExtType(const std::string &url);
    static const std::string& getExtUrl(RtpExtType type);
    static const char *getExtName(RtpExtType type);

    void setType(RtpExtType type);
    RtpExtType getType() const;
    std::string dumpString() const;

    uint8_t getAudioLevel(bool *vad) const;
    uint32_t getAbsSendTime() const;
    uint16_t getTransportCCSeq() const;
    std::string getSdesMid() const;
    std::string getRtpStreamId() const;
    std::string getRepairedRtpStreamId() const;

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
    operator bool () const;

private:
    RtpExt() = default;
    RtpExt(void *ptr, bool one_byte_ext, const char *str, size_t size);
    const char *data() const;
    size_t size() const;
    const uint8_t& operator[](size_t pos) const;
    operator std::string() const;

private:
    void *_ext = nullptr;
    const char *_data;
    size_t _size;
    bool _one_byte_ext = true;
    RtpExtType _type = RtpExtType::padding;
};

class RtcMedia;
class RtpExtContext {
public:
    using Ptr = std::shared_ptr<RtpExtContext>;
    using OnGetRtp = std::function<void(uint8_t pt, uint32_t ssrc, const std::string &rid)>;

    RtpExtContext(const RtcMedia &media);
    ~RtpExtContext() = default;

    void setOnGetRtp(OnGetRtp cb);
    std::string getRid(uint32_t ssrc) const;
    void setRid(uint32_t ssrc, const std::string &rid);
    RtpExt changeRtpExtId(const RtpHeader *header, bool is_recv, std::string *rid_ptr = nullptr, RtpExtType type = RtpExtType::padding);

private:
    void onGetRtp(uint8_t pt, uint32_t ssrc, const std::string &rid);

private:
    OnGetRtp _cb;
    //发送rtp时需要修改rtp ext id
    std::map<RtpExtType, uint8_t> _rtp_ext_type_to_id;
    //接收rtp时需要修改rtp ext id
    std::unordered_map<uint8_t, RtpExtType> _rtp_ext_id_to_type;
    //ssrc --> rid
    std::unordered_map<uint32_t/*simulcast ssrc*/, std::string/*rid*/> _ssrc_to_rid;
};

} //namespace mediakit
#endif //ZLMEDIAKIT_RTPEXT_H
