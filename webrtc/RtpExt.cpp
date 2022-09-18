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
#include "Sdp.h"

#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

using namespace std;
using namespace toolkit;

namespace mediakit {

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
    void setId(uint8_t id);
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
    void setId(uint8_t id);
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

void RtpExtOneByte::setId(uint8_t in) {
    id = in & 0x0F;
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

void RtpExtTwoByte::setId(uint8_t in) {
    id = in;
}

uint8_t *RtpExtTwoByte::getData() {
    return data;
}

//////////////////////////////////////////////////////////////////

static constexpr uint16_t kOneByteHeader = 0xBEDE;
static constexpr uint16_t kTwoByteHeader = 0x1000;

template<typename Type>
static bool isOneByteExt(){
    return false;
}

template<>
bool isOneByteExt<RtpExtOneByte>(){
    return true;
}

template<typename Type>
void appendExt(map<uint8_t, RtpExt> &ret, uint8_t *ptr, const uint8_t *end) {
    while (ptr < end) {
        auto ext = reinterpret_cast<Type *>(ptr);
        if (ext->getId() == (uint8_t) RtpExtType::padding) {
            //padding，忽略
            ++ptr;
            continue;
        }
        //15类型的rtp ext为保留
        CHECK(ext->getId() < (uint8_t) RtpExtType::reserved);
        CHECK(reinterpret_cast<uint8_t *>(ext) + Type::kMinSize <= end);
        CHECK(ext->getData() + ext->getSize() <= end);
        ret.emplace(ext->getId(), RtpExt(ext, isOneByteExt<Type>(), reinterpret_cast<char *>(ext->getData()), ext->getSize()));
        ptr += Type::kMinSize + ext->getSize();
    }
}

RtpExt::RtpExt(void *ext, bool one_byte_ext, const char *str, size_t size) {
    _ext = ext;
    _one_byte_ext = one_byte_ext;
    _data = str;
    _size = size;
}

const char *RtpExt::data() const {
    return _data;
}

size_t RtpExt::size() const {
    return _size;
}

const uint8_t& RtpExt::operator[](size_t pos) const{
    CHECK(pos < _size);
    return ((uint8_t*)_data)[pos];
}

RtpExt::operator std::string() const{
    return string(_data, _size);
}

map<uint8_t/*id*/, RtpExt/*data*/> RtpExt::getExtValue(const RtpHeader *header) {
    map<uint8_t, RtpExt> ret;
    assert(header);
    auto ext_size = header->getExtSize();
    if (!ext_size) {
        return ret;
    }
    auto reserved = header->getExtReserved();
    auto ptr = const_cast<RtpHeader *>(header)->getExtData();
    auto end = ptr + ext_size;
    if (reserved == kOneByteHeader) {
        appendExt<RtpExtOneByte>(ret, ptr, end);
        return ret;
    }
    if ((reserved & 0xFFF0) == kTwoByteHeader) {
        appendExt<RtpExtTwoByte>(ret, ptr, end);
        return ret;
    }
    return ret;
}

#define XX(type, url) {RtpExtType::type , url},
static map<RtpExtType/*id*/, string/*ext*/> s_type_to_url = {RTP_EXT_MAP(XX)};
#undef XX


#define XX(type, url) {url, RtpExtType::type},
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

const char *RtpExt::getExtName(RtpExtType type) {
#define XX(type, url) case RtpExtType::type: return #type;
    switch (type) {
        RTP_EXT_MAP(XX)
        default: return "unknown ext type";
    }
#undef XX
}

string RtpExt::dumpString() const {
    _StrPrinter printer;
    switch (_type) {
        case RtpExtType::ssrc_audio_level : {
            bool vad;
            printer << "audio level:" << (int) getAudioLevel(&vad) << ", vad:" << vad;
            break;
        }
        case RtpExtType::abs_send_time : {
            printer << "abs send time:" << getAbsSendTime();
            break;
        }
        case RtpExtType::transport_cc : {
            printer << "twcc ext seq:" << getTransportCCSeq();
            break;
        }
        case RtpExtType::sdes_mid : {
            printer << "sdes mid:" << getSdesMid();
            break;
        }
        case RtpExtType::sdes_rtp_stream_id : {
            printer << "rtp stream id:" << getRtpStreamId();
            break;
        }
        case RtpExtType::sdes_repaired_rtp_stream_id : {
            printer << "rtp repaired stream id:" << getRepairedRtpStreamId();
            break;
        }
        case RtpExtType::video_timing : {
            uint8_t flags;
            uint16_t encode_start, encode_finish, packetization_complete, last_pkt_left_pacer, reserved_net0, reserved_net1;
            getVideoTiming(flags, encode_start, encode_finish, packetization_complete, last_pkt_left_pacer,
                           reserved_net0, reserved_net1);
            printer << "video timing, flags:" << (int) flags
                    << ",encode:" << encode_start << "-" << encode_finish
                    << ",packetization_complete:" << packetization_complete
                    << ",last_pkt_left_pacer:" << last_pkt_left_pacer
                    << ",reserved_net0:" << reserved_net0
                    << ",reserved_net1:" << reserved_net1;
            break;
        }
        case RtpExtType::video_content_type : {
            printer << "video content type:" << (int)getVideoContentType();
            break;
        }
        case RtpExtType::video_orientation : {
            bool camera_bit, flip_bit, first_rotation, second_rotation;
            getVideoOrientation(camera_bit, flip_bit, first_rotation, second_rotation);
            printer << "video orientation:" << camera_bit << "-" << flip_bit << "-" << first_rotation << "-" << second_rotation;
            break;
        }
        case RtpExtType::playout_delay : {
            uint16_t min_delay, max_delay;
            getPlayoutDelay(min_delay, max_delay);
            printer << "playout delay:" << min_delay << "-" << max_delay;
            break;
        }
        case RtpExtType::toffset : {
            printer << "toffset:" << getTransmissionOffset();
            break;
        }
        case RtpExtType::framemarking : {
            printer << "framemarking tid:" << (int)getFramemarkingTID();
            break;
        }
        default: {
            printer << getExtName(_type) << ", hex:" << hexdump(data(), size());
            break;
        }
    }
    return std::move(printer);
}

//https://tools.ietf.org/html/rfc6464
// 0                   1
//                    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//                   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//                   |  ID   | len=0 |V| level       |
//                   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//              Figure 1: Sample Audio Level Encoding Using the
//                          One-Byte Header Format
//
//
//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |      ID       |     len=1     |V|    level    |    0 (pad)    |
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//              Figure 2: Sample Audio Level Encoding Using the
//                          Two-Byte Header Format
uint8_t RtpExt::getAudioLevel(bool *vad) const{
    CHECK(_type == RtpExtType::ssrc_audio_level && size() >= 1);
    auto &byte = (*this)[0];
    if (vad) {
        *vad = byte & 0x80;
    }
    return byte & 0x7F;
}

//http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
//Wire format: 1-byte extension, 3 bytes of data. total 4 bytes extra per packet (plus shared 4 bytes for all extensions present: 2 byte magic word 0xBEDE, 2 byte # of extensions). Will in practice replace the “toffset” extension so we should see no long term increase in traffic as a result.
//
//Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
//
//Relation to NTP timestamps: abs_send_time_24 = (ntp_timestamp_64 >> 14) & 0x00ffffff ; NTP timestamp is 32 bits for whole seconds, 32 bits fraction of second.
//
//Notes: Packets are time stamped when going out, preferably close to metal. Intermediate RTP relays (entities possibly altering the stream) should remove the extension or set its own timestamp.
uint32_t RtpExt::getAbsSendTime() const {
    CHECK(_type == RtpExtType::abs_send_time && size() >= 3);
    uint32_t ret = 0;
    ret |= (*this)[0] << 16;
    ret |= (*this)[1] << 8;
    ret |= (*this)[2];
    return ret;
}

//https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
//     0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |       0xBE    |    0xDE       |           length=1            |
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |  ID   | L=1   |transport-wide sequence number | zero padding  |
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
uint16_t RtpExt::getTransportCCSeq() const {
    CHECK(_type == RtpExtType::transport_cc && size() >= 2);
    uint16_t ret;
    ret = (*this)[0] << 8;
    ret |= (*this)[1];
    return ret;
}

//https://tools.ietf.org/html/draft-ietf-avtext-sdes-hdr-ext-07
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   |  len  | SDES Item text value ...                      |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
string RtpExt::getSdesMid() const {
    CHECK(_type == RtpExtType::sdes_mid && size() >= 1);
    return *this;
}


//https://tools.ietf.org/html/draft-ietf-avtext-rid-06
//用于simulcast
//3.1.  RTCP 'RtpStreamId' SDES Extension
//
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |RtpStreamId=TBD|     length    | RtpStreamId                 ...
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//
//   The RtpStreamId payload is UTF-8 encoded and is not null-terminated.
//
//      RFC EDITOR NOTE: Please replace TBD with the assigned SDES
//      identifier value.

//3.2.  RTCP 'RepairedRtpStreamId' SDES Extension
//
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |Repaired...=TBD|     length    | RepairRtpStreamId           ...
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//
//   The RepairedRtpStreamId payload is UTF-8 encoded and is not null-
//   terminated.
//
//      RFC EDITOR NOTE: Please replace TBD with the assigned SDES
//      identifier value.

string RtpExt::getRtpStreamId() const {
    CHECK(_type == RtpExtType::sdes_rtp_stream_id && size() >= 1);
    return *this;
}

string RtpExt::getRepairedRtpStreamId() const {
    CHECK(_type == RtpExtType::sdes_repaired_rtp_stream_id && size() >= 1);
    return *this;
}


//http://www.webrtc.org/experiments/rtp-hdrext/video-timing
//Wire format: 1-byte extension, 13 bytes of data. Total 14 bytes extra per packet (plus 1-3 padding byte in some cases, plus shared 4 bytes for all extensions present: 2 byte magic word 0xBEDE, 2 byte # of extensions).
//
//First byte is a flags field. Defined flags:
//
//0x01 - extension is set due to timer.
//0x02 - extension is set because the frame is larger than usual.
//Both flags may be set at the same time. All remaining 6 bits are reserved and should be ignored.
//
//Next, 6 timestamps are stored as 16-bit values in big-endian order, representing delta from the capture time of a packet in ms. Timestamps are, in order:
//
//Encode start.
//Encode finish.
//Packetization complete.
//Last packet left the pacer.
//Reserved for network.
//Reserved for network (2).

void RtpExt::getVideoTiming(uint8_t &flags,
                            uint16_t &encode_start,
                            uint16_t &encode_finish,
                            uint16_t &packetization_complete,
                            uint16_t &last_pkt_left_pacer,
                            uint16_t &reserved_net0,
                            uint16_t &reserved_net1) const {
    CHECK(_type == RtpExtType::video_timing && size() >= 13);
    flags = (*this)[0];
    encode_start = (*this)[1] << 8 | (*this)[2];
    encode_finish = (*this)[3] << 8 | (*this)[4];
    packetization_complete = (*this)[5] << 8 | (*this)[6];
    last_pkt_left_pacer = (*this)[7] << 8 | (*this)[8];
    reserved_net0 = (*this)[9] << 8 | (*this)[10];
    reserved_net1 = (*this)[11] << 8 | (*this)[12];
}


//http://www.webrtc.org/experiments/rtp-hdrext/color-space
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |  ID   | L = 3 |   primaries   |   transfer    |    matrix     |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |range+chr.sit. |
// +-+-+-+-+-+-+-+-+


//http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
//Values:
//0x00: Unspecified. Default value. Treated the same as an absence of an extension.
//0x01: Screenshare. Video stream is of a screenshare type.
//0x02: 摄像头？
//Notes: Extension shoud be present only in the last packet of key-frames.
// If attached to other packets it should be ignored.
// If extension is absent, Unspecified value is assumed.
uint8_t RtpExt::getVideoContentType() const {
    CHECK(_type == RtpExtType::video_content_type && size() >= 1);
    return (*this)[0];
}

//http://www.3gpp.org/ftp/Specs/html-info/26114.htm
void RtpExt::getVideoOrientation(bool &camera_bit, bool &flip_bit, bool &first_rotation, bool &second_rotation) const {
    CHECK(_type == RtpExtType::video_orientation && size() >= 1);
    uint8_t byte = (*this)[0];
    camera_bit = (byte & 0x08) >> 3;
    flip_bit = (byte & 0x04) >> 2;
    first_rotation = (byte & 0x02) >> 1;
    second_rotation = byte & 0x01;
}

//http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//|  ID   | len=2 |       MIN delay       |       MAX delay       |
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
void RtpExt::getPlayoutDelay(uint16_t &min_delay, uint16_t &max_delay) const {
    CHECK(_type == RtpExtType::playout_delay && size() >= 3);
    uint32_t bytes = (*this)[0] << 16 | (*this)[1] << 8 | (*this)[2];
    min_delay = (bytes & 0x00FFF000) >> 12;
    max_delay = bytes & 0x00000FFF;
}

//urn:ietf:params:rtp-hdrext:toffset
//https://tools.ietf.org/html/rfc5450
//       0                   1                   2                   3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |  ID   | len=2 |              transmission offset              |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
uint32_t RtpExt::getTransmissionOffset() const {
    CHECK(_type == RtpExtType::toffset && size() >= 3);
    return (*this)[0] << 16 | (*this)[1] << 8 | (*this)[2];
}

//http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07
//      0                   1                   2                   3
//	    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//	   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//	   |  ID=? |  L=2  |S|E|I|D|B| TID |   LID         |    TL0PICIDX  |
//	   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
uint8_t RtpExt::getFramemarkingTID() const {
    CHECK(_type == RtpExtType::framemarking && size() >= 3);
    return (*this)[0] & 0x07;
}

void RtpExt::setExtId(uint8_t ext_id) {
    assert(ext_id > (int) RtpExtType::padding && ext_id <= (int) RtpExtType::reserved && _ext);
    if (_one_byte_ext) {
        auto ptr = reinterpret_cast<RtpExtOneByte *>(_ext);
        ptr->setId(ext_id);
    } else {
        auto ptr = reinterpret_cast<RtpExtTwoByte *>(_ext);
        ptr->setId(ext_id);
    }
}

void RtpExt::clearExt(){
    assert(_ext);
    if (_one_byte_ext) {
        auto ptr = reinterpret_cast<RtpExtOneByte *>(_ext);
        memset(ptr, (int) RtpExtType::padding, RtpExtOneByte::kMinSize + ptr->getSize());
    } else {
        auto ptr = reinterpret_cast<RtpExtTwoByte *>(_ext);
        memset(ptr, (int) RtpExtType::padding, RtpExtTwoByte::kMinSize + ptr->getSize());
    }
}

void RtpExt::setType(RtpExtType type) {
    _type = type;
}

RtpExtType RtpExt::getType() const {
    return _type;
}

RtpExt::operator bool() const {
    return _ext != nullptr;
}

RtpExtContext::RtpExtContext(const RtcMedia &m){
    for (auto &ext : m.extmap) {
        auto ext_type = RtpExt::getExtType(ext.ext);
        _rtp_ext_id_to_type.emplace(ext.id, ext_type);
        _rtp_ext_type_to_id.emplace(ext_type, ext.id);
    }
}

string RtpExtContext::getRid(uint32_t ssrc) const{
    auto it = _ssrc_to_rid.find(ssrc);
    if (it == _ssrc_to_rid.end()) {
        return "";
    }
    return it->second;
}

void RtpExtContext::setRid(uint32_t ssrc, const string &rid) {
    _ssrc_to_rid[ssrc] = rid;
}

RtpExt RtpExtContext::changeRtpExtId(const RtpHeader *header, bool is_recv, string *rid_ptr, RtpExtType type) {
    string rid, repaired_rid;
    RtpExt ret;
    auto ext_map = RtpExt::getExtValue(header);
    for (auto &pr : ext_map) {
        if (is_recv) {
            auto it = _rtp_ext_id_to_type.find(pr.first);
            if (it == _rtp_ext_id_to_type.end()) {
                //TraceL << "接收rtp时,忽略不识别的rtp ext, id=" << (int) pr.first;
                pr.second.clearExt();
                continue;
            }
            pr.second.setType(it->second);
            //重新赋值ext id为 ext type，作为后面处理ext的统一中间类型
            pr.second.setExtId((uint8_t) it->second);
            switch (it->second) {
                case RtpExtType::sdes_rtp_stream_id : rid = pr.second.getRtpStreamId(); break;
                case RtpExtType::sdes_repaired_rtp_stream_id : repaired_rid = pr.second.getRepairedRtpStreamId(); break;
                default : break;
            }
        } else {
            pr.second.setType((RtpExtType) pr.first);
            auto it = _rtp_ext_type_to_id.find((RtpExtType) pr.first);
            if (it == _rtp_ext_type_to_id.end()) {
                //TraceL << "发送rtp时, 忽略不被客户端支持rtp ext:" << pr.second.dumpString();
                pr.second.clearExt();
                continue;
            }
            //重新赋值ext id为客户端sdp声明的类型
            pr.second.setExtId(it->second);
        }
        if (pr.second.getType() == type) {
            ret = pr.second;
        }
    }

    if (!is_recv) {
        return ret;
    }
    if (rid.empty()) {
        rid = repaired_rid;
    }
    auto ssrc = ntohl(header->ssrc);
    if (rid.empty()) {
        //获取rid
        rid = _ssrc_to_rid[ssrc];
    } else {
        //设置rid
        auto it = _ssrc_to_rid.find(ssrc);
        if (it == _ssrc_to_rid.end() || it->second != rid) {
            _ssrc_to_rid[ssrc] = rid;
            onGetRtp(header->pt, ssrc, rid);
        }
    }
    if (rid_ptr) {
        *rid_ptr = rid;
    }
    return ret;
}

void RtpExtContext::setOnGetRtp(OnGetRtp cb) {
    _cb = std::move(cb);
}

void RtpExtContext::onGetRtp(uint8_t pt, uint32_t ssrc, const string &rid){
    if (_cb) {
        _cb(pt, ssrc, rid);
    }
}

}// namespace mediakit