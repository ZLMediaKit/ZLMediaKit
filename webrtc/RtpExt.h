/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
    XX(av1,                         "https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension") \
    XX(encrypt,                     "urn:ietf:params:rtp-hdrext:encrypt")

enum class RtpExtType : uint8_t {
    padding = 0,
#define XX(type, uri) type,
    RTP_EXT_MAP(XX)
#undef XX
};

class RtcMedia;

// 使用次对象的方法前需保证RtpHeader内存未释放  [AUTO-TRANSLATED:0378877f]
// Ensure that the RtpHeader memory has not been released before using the methods of this object
class RtpExt {
public:
    template<typename Type>
    friend void appendExt(std::map<uint8_t, RtpExt> &ret, uint8_t *ptr, const uint8_t *end);
    friend class RtpExtContext;

    // len为rtp数据报实际长度, 用于校验包内声明的扩展长度, 防止堆越界读写
    static std::map<uint8_t/*id*/, RtpExt/*data*/> getExtValue(const RtpHeader *header, size_t len);
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

    const char *data() const;
    size_t size() const;

private:
    RtpExt() = default;
    RtpExt(void *ptr, bool one_byte_ext, const char *str, size_t size);
    const uint8_t& operator[](size_t pos) const;
    operator std::string() const;

private:
    void *_ext = nullptr;
    const char *_data;
    size_t _size;
    bool _one_byte_ext = true;
    RtpExtType _type = RtpExtType::padding;
};

class RtpExtContext {
public:
    using Ptr = std::shared_ptr<RtpExtContext>;

    RtpExtContext() = default;
    void addExtMap(const RtcMedia &media);

    void setRid(uint32_t ssrc, const std::string &rid);
    const std::string &getRid() const;

    void parseRtpExt(const RtpHeader *header, size_t len);
    void changeRtpExt(const RtpExtContext &to);
    const RtpExt& getRtpExt(RtpExtType type) const;
    bool needTwoBytesExt() const;
    const std::map<uint8_t/*id*/, RtpExt/*data*/> &getExtMap() const;
    int getExtId(RtpExtType type) const;

private:
    // 发送rtp时需要修改rtp ext id  [AUTO-TRANSLATED:b92a494b]
    // Modify the rtp ext id when sending rtp
    std::map<RtpExtType, uint8_t> _rtp_ext_type_to_id;
    // 接收rtp时需要修改rtp ext id  [AUTO-TRANSLATED:685e7a01]
    // Modify the rtp ext id when receiving rtp
    std::map<uint8_t, RtpExtType> _rtp_ext_id_to_type;
    bool _need_two_bytes_ext = false;
    //ssrc --> rid
    std::map<uint32_t/*simulcast ssrc*/, std::string/*rid*/> _ssrc_to_rid;

    // 当前解析的rtp的rid
    std::string _rid;
    // 当前解析的rtp的扩展列表
    RtpExt _empty_ext;
    std::map<uint8_t/*id*/, RtpExt/*data*/> _rtp_ext_map;
};

// RTP扩展头构建器，用于组装one-byte或two-byte格式的RTP header extension
// RTP extension header builder for assembling one-byte or two-byte format extensions
class RtpExtBuilder {
public:
    explicit RtpExtBuilder(bool two_byte_ext);

    // 追加一个扩展项，id为0或data为空时跳过
    // Append an extension item, skipped when id is 0 or data is null
    void appendExt(uint8_t id, const char *data, uint8_t size);

    // 完成构建：填充padding并对齐到4字节边界，回填ext length字段
    // Finalize: pad to 4-byte boundary and fill in the ext length field
    void build();

    // 获取构建后的ext buffer（需在build()之后调用）
    // Get the built ext buffer (call after build())
    const std::string &buffer() const;

private:
    bool _two_byte_ext;
    std::string _buffer;
};

} //namespace mediakit
#endif //ZLMEDIAKIT_RTPEXT_H
