/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtpExt.h"

#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

//https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
//https://tools.ietf.org/html/rfc5285

//       0                   1                   2                   3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |       0xBE    |    0xDE       |           length=3            |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |  ID   | L=0   |     data      |  ID   |  L=1  |   data...
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//            ...data   |    0 (pad)    |    0 (pad)    |  ID   | L=3   |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                          data                                 |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class RtpExtOneByte {
public:
    static constexpr uint16_t kMinSize = 1;
    size_t getSize() const;
    uint8_t getId() const;
    uint8_t* getData();

private:
#if __BYTE_ORDER == __BIG_ENDIAN
    uint8_t id: 4;
    uint8_t len: 4;
#else
    uint8_t len: 4;
    uint8_t id: 4;
#endif
    uint8_t data[1];
} PACKED;

//0                   1                   2                   3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |         0x100         |appbits|           length=3            |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |      ID       |     L=0       |     ID        |     L=1       |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |       data    |    0 (pad)    |       ID      |      L=4      |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                          data                                 |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class RtpExtTwoByte {
public:
    static constexpr uint16_t kMinSize = 2;

    size_t getSize() const;
    uint8_t getId() const;
    uint8_t* getData();

private:
    uint8_t id;
    uint8_t len;
    uint8_t data[1];
} PACKED;

#if defined(_WIN32)
#pragma pack(pop)
#endif // defined(_WIN32)

//////////////////////////////////////////////////////////////////

size_t RtpExtOneByte::getSize() const {
    return len + 1;
}

uint8_t RtpExtOneByte::getId() const {
    return id;
}

uint8_t *RtpExtOneByte::getData() {
    return data;
}

//////////////////////////////////////////////////////////////////

size_t RtpExtTwoByte::getSize() const {
    return len;
}

uint8_t RtpExtTwoByte::getId() const {
    return id;
}

uint8_t *RtpExtTwoByte::getData() {
    return data;
}

//////////////////////////////////////////////////////////////////

static constexpr uint16_t kOneByteHeader = 0xBEDE;
static constexpr uint16_t kTwoByteHeader = 0x1000;

map<uint8_t/*id*/, RtpExt/*data*/> RtpExt::getExtValue(const RtpHeader *header, const RtcMedia &media) {
    map<uint8_t, RtpExt> ret;
    assert(header);
    auto ext_size = header->getExtSize();
    if (!ext_size) {
        return ret;
    }
    auto reserved = header->getExtReserved();
    auto ptr = const_cast<RtpHeader *>(header)->getExtData();
    auto end = ptr + ext_size;
    RtpExtType type;
    if (reserved == kOneByteHeader) {
        while (ptr < end) {
            RtpExtOneByte *ext = reinterpret_cast<RtpExtOneByte *>(ptr);
            if (ext->getId() == (uint8_t) RtpExtType::padding) {
                //padding，忽略
                ++ptr;
                continue;
            }
            //15类型的rtp ext为保留
            CHECK(ext->getId() < (uint8_t) RtpExtType::reserved);
            CHECK(reinterpret_cast<uint8_t *>(ext) + RtpExtOneByte::kMinSize <= end);
            CHECK(ext->getData() + ext->getSize() <= end);
            ret.emplace(ext->getId(), RtpExt(type, reinterpret_cast<char *>(ext->getData()), ext->getSize()));
            ptr += RtpExtOneByte::kMinSize + ext->getSize();
        }
        return ret;
    }

    if ((reserved & 0xFFF0) >> 4 == kTwoByteHeader) {
        while (ptr < end) {
            RtpExtTwoByte *ext = reinterpret_cast<RtpExtTwoByte *>(ptr);
            if (ext->getId() == (uint8_t) RtpExtType::padding) {
                //padding，忽略
                ++ptr;
                continue;
            }
            //15类型的rtp ext为保留
            CHECK(ext->getId() < (uint8_t) RtpExtType::reserved);
            CHECK(reinterpret_cast<uint8_t *>(ext) + RtpExtTwoByte::kMinSize <= end);
            CHECK(ext->getData() + ext->getSize() <= end);
            ret.emplace(ext->getId(), RtpExt(type, reinterpret_cast<char *>(ext->getData()), ext->getSize()));
            ptr += RtpExtTwoByte::kMinSize + ext->getSize();
        }
        return ret;
    }

    return ret;
}

#define RTP_EXT_MAP(XX) \
    XX(RtpExtType::ssrc_audio_level,            "urn:ietf:params:rtp-hdrext:ssrc-audio-level") \
    XX(RtpExtType::abs_send_time,               "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time") \
    XX(RtpExtType::transport_cc,                "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01") \
    XX(RtpExtType::sdes_mid,                    "urn:ietf:params:rtp-hdrext:sdes:mid") \
    XX(RtpExtType::sdes_rtp_stream_id,          "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id") \
    XX(RtpExtType::sdes_repaired_rtp_stream_id, "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id") \
    XX(RtpExtType::video_timing,                "http://www.webrtc.org/experiments/rtp-hdrext/video-timing") \
    XX(RtpExtType::color_space,                 "http://www.webrtc.org/experiments/rtp-hdrext/color-space") \
    XX(RtpExtType::video_content_type,          "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type") \
    XX(RtpExtType::playout_delay,               "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay") \
    XX(RtpExtType::video_orientation,           "urn:3gpp:video-orientation") \
    XX(RtpExtType::toffset,                     "urn:ietf:params:rtp-hdrext:toffset")

#define XX(type, url) {type , url},
static unordered_map<RtpExtType/*id*/, string/*ext*/> s_type_to_url = {RTP_EXT_MAP(XX)};
#undef XX


#define XX(type, url) {url, type},
static unordered_map<string/*ext*/, RtpExtType/*id*/> s_url_to_type = {RTP_EXT_MAP(XX)};
#undef XX

RtpExtType RtpExt::getExtType(const string &url) {
    auto it = s_url_to_type.find(url);
    if (it == s_url_to_type.end()) {
        throw std::invalid_argument(string("未识别的rtp ext url类型:") + url);
    }
    return it->second;
}

const string &RtpExt::getExtUrl(RtpExtType type) {
    auto it = s_type_to_url.find(type);
    if (it == s_type_to_url.end()) {
        throw std::invalid_argument(string("未识别的rtp ext类型:") + to_string((int) type));
    }
    return it->second;
}

