/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#include "AV1.h"
#include "AV1Rtp.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace toolkit;

namespace mediakit {

// AV1 OBU类型定义
static constexpr int kObuTypeSequenceHeader = 1;
static constexpr int kObuTypeTemporalDelimiter = 2;
static constexpr int kObuTypeTileList = 8;
static constexpr int kObuTypePadding = 15;

// RTP聚合头中的位定义
static constexpr uint8_t kObuSizePresentBit = 0b00000010;
static constexpr int kAggregationHeaderSize = 1;
static constexpr int kMaxNumObusToOmitSize = 3;

// LEB128编码/解码辅助函数
static size_t writeLeb128(uint64_t value, uint8_t* buffer) {
    size_t size = 0;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;
        }
        buffer[size++] = byte;
    } while (value != 0);
    return size;
}

static size_t leb128Size(uint64_t value) {
    size_t size = 0;
    do {
        value >>= 7;
        ++size;
    } while (value != 0);
    return size;
}

static bool readLeb128(const uint8_t*& data, size_t& remaining, uint64_t& value) {
    value = 0;
    size_t shift = 0;

    while (remaining > 0 && shift < 56) {
        uint8_t byte = *data++;
        remaining--;

        value |= (uint64_t(byte & 0x7F) << shift);
        shift += 7;

        if ((byte & 0x80) == 0) {
            return true;
        }
    }

    // 兼容性处理：如果到达数据末尾但最后一个字节的MSB仍为1，
    // 假设这是leb128编码的结尾
    if (remaining == 0 && shift > 0) {
        WarnL << "Tolerating non-standard LEB128 encoding (missing termination bit)";
        return true;
    }

    return false;
}

// OBU辅助函数
static bool obuHasExtension(uint8_t obu_header) {
    return obu_header & 0b00000100;
}

static bool obuHasSize(uint8_t obu_header) {
    return obu_header & kObuSizePresentBit;
}

static int obuType(uint8_t obu_header) {
    return (obu_header & 0b01111000) >> 3;
}

static int maxFragmentSize(int remaining_bytes) {
    if (remaining_bytes <= 1) {
        return 0;
    }
    for (int i = 1; ; ++i) {
        if (remaining_bytes < (1 << (7 * i)) + i) {
            return remaining_bytes - i;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// AV1RtpEncoder 实现
//////////////////////////////////////////////////////////////////////////

AV1RtpEncoder::AV1RtpEncoder() {
}

std::vector<AV1RtpEncoder::ObuInfo> AV1RtpEncoder::parseObus(const uint8_t* data, size_t size) {
    std::vector<ObuInfo> result;
    const uint8_t* ptr = data;
    size_t remaining = size;

    while (remaining > 0) {
        if (remaining < 1) {
            WarnL << "Malformed AV1 input: expected OBU header";
            return {};
        }

        ObuInfo obu{};
        obu.header = *ptr++;
        remaining--;
        obu.has_extension = obuHasExtension(obu.header);
        obu.has_size_field = obuHasSize(obu.header);

        if (obu.has_extension) {
            if (remaining < 1) {
                WarnL << "Malformed AV1 input: expected extension header";
                return {};
            }
            obu.extension_header = *ptr++;
            remaining--;
        }

        uint64_t payload_size = 0;
        if (obu.has_size_field) {
            if (!readLeb128(ptr, remaining, payload_size)) {
                WarnL << "Malformed AV1 input: failed to read OBU size";
                return {};
            }
            if (payload_size > remaining) {
                WarnL << "Malformed AV1 input: OBU size exceeds remaining data";
                return {};
            }
        } else {
            payload_size = remaining;
        }

        obu.payload_data = ptr;
        obu.payload_size = payload_size;
        ptr += payload_size;
        remaining -= payload_size;

        int type = obuType(obu.header);
        if (type != kObuTypeTemporalDelimiter &&
            type != kObuTypeTileList &&
            type != kObuTypePadding) {
            result.push_back(obu);
        }
    }

    return result;
}

uint8_t AV1RtpEncoder::makeAggregationHeader(bool first_obu_is_fragment,
                                             bool last_obu_is_fragment,
                                             int num_obu_elements,
                                             bool starts_new_coded_video_sequence) {
    uint8_t header = 0;

    // Z bit: first OBU element is continuation of previous OBU
    if (first_obu_is_fragment) {
        header |= 0x80;
    }

    // Y bit: last OBU element will be continued in next packet
    if (last_obu_is_fragment) {
        header |= 0x40;
    }

    // W field: number of OBU elements (when <= 3)
    if (num_obu_elements <= kMaxNumObusToOmitSize) {
        header |= (num_obu_elements << 4);
    }

    // N bit: beginning of new coded video sequence
    if (starts_new_coded_video_sequence) {
        header |= 0x08;
    }

    return header;
}

void AV1RtpEncoder::outputRtp(const uint8_t* data, size_t len, bool mark,
                              uint64_t stamp, uint8_t aggregation_header) {
    auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, len + kAggregationHeaderSize, mark, stamp);
    auto payload = rtp->data() + RtpPacket::kRtpTcpHeaderSize + RtpPacket::kRtpHeaderSize;

    // 写入聚合头
    payload[0] = aggregation_header;

    // 复制数据
    if (len > 0) {
        memcpy(payload + kAggregationHeaderSize, data, len);
    }

    RtpCodec::inputRtp(std::move(rtp), false);
}

bool AV1RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = frame->data() + frame->prefixSize();
    auto size = frame->size() - frame->prefixSize();

    if (size == 0) {
        return false;
    }

    // 解析OBU
    auto obus = parseObus((const uint8_t*)ptr, size);
    if (obus.empty()) {
        return false;
    }

    // 检查是否包含序列头(关键帧标志)
    bool has_sequence_header = false;
    for (const auto& obu : obus) {
        int type = obuType(obu.header);
        if (type == kObuTypeSequenceHeader) {
            has_sequence_header = true;
            _got_key_frame = true;
            break;
        }
    }

    // 如果还没有收到过关键帧，且当前帧不是关键帧，则丢弃
    if (!_got_key_frame && !has_sequence_header) {
        DebugL << "Dropping AV1 frame before first keyframe";
        return false;
    }

    size_t max_payload_size = getRtpInfo().getMaxSize() - kAggregationHeaderSize;
    if (max_payload_size == 0) {
        WarnL << "Invalid RTP max payload size for AV1";
        return false;
    }

    for (size_t i = 0; i < obus.size(); ++i) {
        const auto& obu = obus[i];
        bool is_first_obu = (i == 0);
        bool is_last_obu = (i == obus.size() - 1);
        if (!sendObu(obu, is_first_obu, is_last_obu,
                     has_sequence_header && is_first_obu, frame->pts(), max_payload_size)) {
            return false;
        }
    }

    return true;
}

bool AV1RtpEncoder::sendObu(const ObuInfo& obu,
                            bool is_first_obu,
                            bool is_last_obu,
                            bool starts_new_sequence,
                            uint64_t stamp,
                            size_t max_payload_size) {
    std::vector<uint8_t> obu_bytes;
    obu_bytes.reserve(1 + (obu.has_extension ? 1 : 0) + obu.payload_size);
    obu_bytes.push_back(obu.header & ~kObuSizePresentBit);
    if (obu.has_extension) {
        obu_bytes.push_back(obu.extension_header);
    }
    if (obu.payload_size > 0) {
        obu_bytes.insert(obu_bytes.end(), obu.payload_data, obu.payload_data + obu.payload_size);
    }

    size_t offset = 0;
    bool first_fragment = true;
    while (offset < obu_bytes.size()) {
        size_t fragment_size = std::min<size_t>(max_payload_size, obu_bytes.size() - offset);
        bool last_fragment = (offset + fragment_size) == obu_bytes.size();
        uint8_t agg_header = makeAggregationHeader(
            !first_fragment,
            !last_fragment,
            1,
            first_fragment && starts_new_sequence
        );

        bool mark = last_fragment && is_last_obu;
        outputRtp(obu_bytes.data() + offset, fragment_size, mark, stamp, agg_header);

        offset += fragment_size;
        first_fragment = false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
// AV1RtpDecoder 实现
//////////////////////////////////////////////////////////////////////////

AV1RtpDecoder::AV1RtpDecoder() {
    obtainFrame();
}

void AV1RtpDecoder::obtainFrame() {
    _frame = FrameImp::create<AV1Frame>();
}

AV1RtpDecoder::AggregationHeader AV1RtpDecoder::parseAggregationHeader(uint8_t header) {
    AggregationHeader agg;
    agg.first_obu_is_fragment = (header & 0x80) != 0;
    agg.last_obu_is_fragment = (header & 0x40) != 0;
    agg.num_obu_elements = (header & 0x30) >> 4;
    agg.starts_new_coded_video_sequence = (header & 0x08) != 0;
    return agg;
}

bool AV1RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size < kAggregationHeaderSize) {
        return false;
    }

    uint32_t ssrc = rtp->getSSRC();
    if (!_has_last_ssrc || _last_ssrc != ssrc) {
        resetState();
        _last_ssrc = ssrc;
        _has_last_ssrc = true;
    }

    auto stamp = rtp->getStampMS();
    auto payload = rtp->getPayload();
    auto seq = rtp->getSeq();

    // 解析聚合头
    auto agg_header = parseAggregationHeader(payload[0]);

    const uint8_t* data = payload + kAggregationHeaderSize;
    size_t remaining = payload_size - kAggregationHeaderSize;

    // InfoL << "RTP seq=" << seq << ", Z=" << agg_header.first_obu_is_fragment
    //       << ", Y=" << agg_header.last_obu_is_fragment
    //       << ", W=" << agg_header.num_obu_elements
    //       << ", N=" << agg_header.starts_new_coded_video_sequence
    //       << ", payload_size=" << remaining;

    // if (remaining > 0) {
    //     std::ostringstream hex_stream;
    //     for (size_t i = 0; i < std::min(remaining, size_t(16)); ++i) {
    //         hex_stream << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    //     }
    //     InfoL << "RTP payload hex: " << hex_stream.str();
    // }

    // 如果开始新的编码视频序列，清理之前的状态
    if (agg_header.starts_new_coded_video_sequence) {
        InfoL << "Starting new coded video sequence";
        resetState();
        obtainFrame();
    }

    if (_has_last_seq) {
        uint16_t expected = _last_seq + 1;
        if (seq != expected && _assembling_fragment) {
            WarnL << "RTP seq gap while assembling fragment, expected=" << expected
                  << " got=" << seq << ", dropping incomplete OBU";
            _fragment_buffer.clear();
            _assembling_fragment = false;
        }
    }
    _last_seq = seq;
    _has_last_seq = true;

    if (!processPayload(agg_header, data, remaining)) {
        resetState();
        obtainFrame();
        return false;
    }

    bool marker = rtp->getHeader()->mark;
    if (marker) {
        if (_assembling_fragment) {
            WarnL << "Marker bit set while awaiting fragment continuation";
            _fragment_buffer.clear();
            _assembling_fragment = false;
        }
        _last_dts = stamp;
        if (!_received_keyframe) {
            WarnL << "AV1 RTP packet before keyframe, dropping";
            _frame->_buffer.clear();
            obtainFrame();
            return false;
        }
        flushFrame(stamp);
        return true;
    }

    _last_dts = stamp;
    return false;
}

bool AV1RtpDecoder::processPayload(const AggregationHeader& agg_header,
                                   const uint8_t* data,
                                   size_t remaining) {
    size_t element_index = 0;
    int expected_elements = agg_header.num_obu_elements;

    while (remaining > 0) {
        uint64_t element_size = 0;
        bool has_size = (expected_elements == 0) || (static_cast<int>(element_index) < expected_elements - 1);
        if (has_size) {
            if (!readLeb128(data, remaining, element_size)) {
                WarnL << "Failed to read OBU element size, trying fallback parsing";
                // 兼容性回退：如果leb128解析失败，尝试直接使用剩余字节数
                element_size = remaining;
            } else if (element_size > remaining) {
                WarnL << "OBU element size (" << element_size << ") exceeds remaining payload ("
                      << remaining << "), using remaining size";
                element_size = remaining;
            }
        } else {
            element_size = remaining;
        }

        std::vector<uint8_t> element_bytes;
        element_bytes.reserve(element_size);
        if (element_size > 0) {
            element_bytes.insert(element_bytes.end(), data, data + element_size);
            data += element_size;
            remaining -= element_size;
        }

        bool is_first = element_index == 0;
        bool is_last = (remaining == 0);

        if (is_first && agg_header.first_obu_is_fragment) {
            if (_fragment_buffer.empty()) {
                WarnL << "Unexpected fragment continuation in AV1 RTP packet";
                return false;
            }
            _fragment_buffer.insert(_fragment_buffer.end(), element_bytes.begin(), element_bytes.end());
        } else {
            if (_assembling_fragment && !_fragment_buffer.empty()) {
                WarnL << "Previous fragment never completed, discarding";
                return false;
            }
            _fragment_buffer = std::move(element_bytes);
        }

        bool will_continue = is_last && agg_header.last_obu_is_fragment;
        if (will_continue) {
            _assembling_fragment = true;
        } else {
            if (!emitObu(_fragment_buffer.data(), _fragment_buffer.size())) {
                return false;
            }
            _fragment_buffer.clear();
            _assembling_fragment = false;
        }

        ++element_index;
    }

    if (expected_elements > 0 && static_cast<int>(element_index) != expected_elements) {
        WarnL << "Mismatch between W field (" << expected_elements
              << ") and parsed OBU elements (" << element_index
              << "), tolerating for compatibility";
        // 不返回false，继续处理以提高兼容性
    }

    return true;
}

bool AV1RtpDecoder::emitObu(const uint8_t* data, size_t size) {
    if (size == 0) {
        return true;
    }

    if (size < 1) {
        WarnL << "Empty OBU fragment";
        return false;
    }

    uint8_t obu_header = data[0];
    size_t header_size = 1;

    // 检查OBU头部是否已经包含size bit
    bool already_has_size = obuHasSize(obu_header);

    // 如果RTP包中的OBU已经包含size字段，需要特殊处理
    if (already_has_size) {
        //WarnL << "RTP OBU contains size field";

        // 跳过extension header处理
        if (obuHasExtension(obu_header)) {
            if (size < 2) {
                WarnL << "OBU with extension flag but insufficient data";
                return false;
            }
            header_size = 2;
        }

        // 读取原始的size字段
        const uint8_t* ptr = data + header_size;
        size_t remaining = size - header_size;
        uint64_t original_size = 0;

        if (!readLeb128(ptr, remaining, original_size)) {
            WarnL << "Failed to read original OBU size field";
            return false;
        }

        if (original_size != remaining) {
            WarnL << "OBU size mismatch in RTP packet, original_size=" << original_size
                  << " remaining=" << remaining;
        }

        // 直接拷贝完整的OBU（包括已有的size字段）
        _frame->_buffer.append((char*)data, size);
    } else {
        // 标准情况：RTP包中的OBU没有size字段，需要我们添加

        // 写入带size bit的OBU头部
        _frame->_buffer.push_back(obu_header | kObuSizePresentBit);

        if (obuHasExtension(obu_header)) {
            if (size < 2) {
                WarnL << "OBU with extension flag but insufficient data";
                return false;
            }
            _frame->_buffer.push_back(data[1]);
            header_size = 2;
        }

        if (size < header_size) {
            WarnL << "Invalid OBU size";
            return false;
        }

        // 计算payload大小并写入leb128编码的size字段
        uint64_t payload_size = size - header_size;
        uint8_t size_bytes[8];
        size_t size_len = writeLeb128(payload_size, size_bytes);
        _frame->_buffer.append((char*)size_bytes, size_len);

        // 拷贝payload数据
        if (payload_size > 0) {
            _frame->_buffer.append((char*)data + header_size, payload_size);
        }
    }

    if (obuType(obu_header) == kObuTypeSequenceHeader) {
        _received_keyframe = true;
    }

    return true;
}

void AV1RtpDecoder::flushFrame(uint64_t stamp) {
    if (_frame->_buffer.empty()) {
        return;
    }
    _frame->_dts = stamp;
    _frame->_pts = stamp;
    RtpCodec::inputFrame(_frame);
    obtainFrame();
}

void AV1RtpDecoder::resetState() {
    _fragment_buffer.clear();
    _assembling_fragment = false;
    _has_last_seq = false;
    _received_keyframe = false;
}

} // namespace mediakit
