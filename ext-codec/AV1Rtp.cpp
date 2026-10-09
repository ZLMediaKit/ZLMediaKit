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

struct RtpElement {
    size_t obu_index;
    size_t offset;
    size_t size;
};

static size_t packetPrefixSize(const std::vector<RtpElement> &elements) {
    size_t size = 0;
    for (auto &element : elements) {
        size += leb128Size(element.size) + element.size;
    }
    return size;
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
                              uint64_t stamp, uint8_t aggregation_header, bool key_pos) {
    auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, len + kAggregationHeaderSize, mark, stamp);
    auto payload = rtp->data() + RtpPacket::kRtpTcpHeaderSize + RtpPacket::kRtpHeaderSize;

    // 写入聚合头
    payload[0] = aggregation_header;

    // 复制数据
    if (len > 0) {
        memcpy(payload + kAggregationHeaderSize, data, len);
    }

    RtpCodec::inputRtp(std::move(rtp), key_pos);
}

bool AV1RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = frame->data() + frame->prefixSize();
    auto size = frame->size() - frame->prefixSize();
    if (!size) {
        return false;
    }
    auto obus = parseObus((const uint8_t *)ptr, size);
    if (obus.empty()) {
        return false;
    }

    bool has_seq = false;
    for (auto &obu : obus) {
        if (obuType(obu.header) == kObuTypeSequenceHeader) {
            has_seq = true;
            break;
        }
    }
    if (!_got_key_frame && !has_seq) {
        return false;
    }
    _got_key_frame = _got_key_frame || has_seq;

    auto rtp_payload_size = getRtpInfo().getMaxSize();
    if (rtp_payload_size <= kAggregationHeaderSize) {
        return false;
    }
    auto max_size = rtp_payload_size - kAggregationHeaderSize;
    auto stamp = frame->pts();

    std::vector<std::string> elements;
    std::vector<int> layer_ids;
    for (auto &obu : obus) {
        std::string buf;
        buf.push_back(obu.header & ~kObuSizePresentBit);
        if (obu.has_extension) {
            buf.push_back(obu.extension_header);
        }
        buf.append((char *)obu.payload_data, obu.payload_size);
        elements.emplace_back(std::move(buf));
        layer_ids.emplace_back(obu.has_extension ? (obu.extension_header & 0xF8) : -1);
    }

    // 同一帧的多个 OBU 共用一个 RTP 包序列，避免序列头被当成完整帧。
    size_t obu_index = 0;
    size_t obu_offset = 0;
    bool first_packet = true;
    while (obu_index < elements.size()) {
        std::vector<RtpElement> packet_elements;
        int packet_layer = -1;

        while (obu_index < elements.size() && packet_elements.size() < kMaxNumObusToOmitSize) {
            auto layer_id = layer_ids[obu_index];
            if (!packet_elements.empty() && packet_layer >= 0 && layer_id >= 0 && packet_layer != layer_id) {
                break;
            }

            // 加入新 OBU 后，前面的 OBU 需补长度字段。
            auto prefix_size = packetPrefixSize(packet_elements);
            if (prefix_size >= max_size) {
                break;
            }

            auto &obu = elements[obu_index];
            auto remaining = obu.size() - obu_offset;
            auto fragment_size = (std::min)(max_size - prefix_size, remaining);
            if (!fragment_size) {
                break;
            }

            packet_elements.push_back({obu_index, obu_offset, fragment_size});
            if (layer_id >= 0) {
                packet_layer = layer_id;
            }

            if (fragment_size < remaining) {
                obu_offset += fragment_size;
                break;
            }

            ++obu_index;
            obu_offset = 0;
            if (obu_index == elements.size()) {
                break;
            }

            auto next_layer = layer_ids[obu_index];
            if (packet_layer >= 0 && next_layer >= 0 && packet_layer != next_layer) {
                break;
            }
        }

        if (packet_elements.empty()) {
            WarnL << "Failed to packetize AV1 frame within RTP payload limit";
            return false;
        }

        std::string payload;
        payload.reserve(max_size);
        for (size_t i = 0; i < packet_elements.size(); ++i) {
            auto &element = packet_elements[i];
            if (i + 1 < packet_elements.size()) {
                uint8_t leb[8];
                auto bytes = writeLeb128(element.size, leb);
                payload.append((char *)leb, bytes);
            }
            payload.append(elements[element.obu_index].data() + element.offset, element.size);
        }

        auto &first = packet_elements.front();
        auto &last = packet_elements.back();
        bool first_is_fragment = first.offset != 0;
        bool last_is_fragment = last.offset + last.size < elements[last.obu_index].size();
        bool mark = obu_index == elements.size() && obu_offset == 0;
        outputRtp((uint8_t *)payload.data(), payload.size(), mark, stamp,
                  makeAggregationHeader(first_is_fragment, last_is_fragment,
                                        (int)packet_elements.size(), first_packet && has_seq),
                  first_packet && has_seq);
        first_packet = false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////
// AV1RtpDecoder 实现
//////////////////////////////////////////////////////////////////////////

AV1RtpDecoder::AV1RtpDecoder() {
    obtainFrame();
}

void AV1RtpDecoder::obtainFrame() {
    _frame = FrameImp::create<AV1Frame>();
    _current_frame_starts_new_sequence = false;
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
    auto rtp_stamp = rtp->getStamp();
    auto payload = rtp->getPayload();
    auto seq = rtp->getSeq();

    // 解析聚合头
    auto agg_header = parseAggregationHeader(payload[0]);

    const uint8_t* data = payload + kAggregationHeaderSize;
    size_t remaining = payload_size - kAggregationHeaderSize;

    // 序号空洞必须在更新 _last_seq 之前判断；marker 只是 SHOULD，不能作为唯一判据。
    bool seq_gap = _has_last_seq && seq != (uint16_t)(_last_seq + 1);
    // 记录进入当前时间单元之前，上一个时间单元是否已正常收尾
    bool prev_unit_completed = _unit_completed;

    // Marker 可能缺失；RTP 时间戳变化时结束或丢弃上一时间单元。
    if (_has_last_stamp && rtp_stamp != _last_rtp_stamp) {
        // 上一个时间单元没有以 marker 收尾，且序号出现空洞，说明它尾部有丢包，
        // 此时只能丢弃，否则会把半帧当成完整帧输出。
        if (_assembling_fragment || _drop_frame || (seq_gap && !prev_unit_completed)) {
            if (_current_frame_starts_new_sequence) {
                _received_keyframe = false;
            }
            _fragment_buffer.clear();
            _assembling_fragment = false;
            _drop_frame = false;
            _frame->_buffer.clear();
            obtainFrame();
        } else if (!_frame->_buffer.empty() && _received_keyframe) {
            flushFrame(_last_dts);
        } else if (!_frame->_buffer.empty()) {
            obtainFrame();
        }
        _unit_completed = false;
    }

    // 如果开始新的编码视频序列，清理之前的状态
    if (agg_header.starts_new_coded_video_sequence) {
        // DebugL << "Starting new coded video sequence";
        resetState();
        obtainFrame();
        _current_frame_starts_new_sequence = true;
    }

    // 出现序号空洞时，无法判断丢失的报文落在上一个时间单元的尾部还是当前时间单元的头部，
    // 因此保守地把当前时间单元也丢掉，避免输出残缺帧。
    // 但 N=1 的报文本就是新编码视频序列的起始，空洞只可能属于它之前的序列，
    // 丢掉它会把用于恢复的 sequence header / 关键帧一起丢掉，必须保留。
    if (seq_gap && !_drop_frame && !agg_header.starts_new_coded_video_sequence) {
        WarnL << "RTP seq gap in AV1 temporal unit, expected=" << (uint16_t)(_last_seq + 1)
              << " got=" << seq << ", dropping incomplete frame";
        _fragment_buffer.clear();
        _assembling_fragment = false;
        _frame->_buffer.clear();
        _drop_frame = true;
        if (_current_frame_starts_new_sequence) {
            _received_keyframe = false;
        }
    }
    _last_seq = seq;
    _has_last_seq = true;

    // 无首片的续片说明当前帧不完整；只丢当前帧，保留已有序列状态。
    if (!_drop_frame && agg_header.first_obu_is_fragment && !_assembling_fragment) {
        WarnL << "Orphan AV1 fragment, seq=" << seq << ", stamp=" << stamp
              << ", rtp_stamp=" << rtp_stamp << ", W=" << agg_header.num_obu_elements
              << ", Y=" << agg_header.last_obu_is_fragment << ", dropping current frame";
        _fragment_buffer.clear();
        _frame->_buffer.clear();
        _drop_frame = true;
        if (_current_frame_starts_new_sequence) {
            _received_keyframe = false;
        }
    }

    if (!_drop_frame && !processPayload(agg_header, data, remaining)) {
        WarnL << "Invalid AV1 RTP payload, seq=" << seq << ", stamp=" << stamp
              << ", rtp_stamp=" << rtp_stamp << ", Z=" << agg_header.first_obu_is_fragment
              << ", Y=" << agg_header.last_obu_is_fragment << ", W=" << agg_header.num_obu_elements
              << ", dropping current frame";
        _fragment_buffer.clear();
        _assembling_fragment = false;
        _frame->_buffer.clear();
        _drop_frame = true;
        if (_current_frame_starts_new_sequence) {
            _received_keyframe = false;
        }
    }

    bool marker = rtp->getHeader()->mark;
    if (marker) {
        // marker 到达说明当前时间单元已收尾，之后的丢包都属于下一个时间单元
        _unit_completed = true;
        if (_assembling_fragment || _drop_frame) {
            WarnL << "Dropping incomplete AV1 frame at marker";
            if (_current_frame_starts_new_sequence) {
                _received_keyframe = false;
            }
            _fragment_buffer.clear();
            _assembling_fragment = false;
            _drop_frame = false;
            _frame->_buffer.clear();
            obtainFrame();
            _last_dts = stamp;
            _last_rtp_stamp = rtp_stamp;
            _has_last_stamp = true;
            return false;
        }
        _last_dts = stamp;
        _last_rtp_stamp = rtp_stamp;
        _has_last_stamp = true;
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
    _last_rtp_stamp = rtp_stamp;
    _has_last_stamp = true;
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
                WarnL << "Failed to read OBU element size";
                return false;
            } else if (element_size > remaining) {
                WarnL << "OBU element size (" << element_size << ") exceeds remaining payload ("
                      << remaining << ")";
                return false;
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
              << ") and parsed OBU elements (" << element_index << ")";
        return false;
    }

    return true;
}

bool AV1RtpDecoder::emitObu(const uint8_t* data, size_t size) {
    if (size == 0) {
        return true;
    }

    uint8_t obu_header = data[0];
    size_t header_size = 1;

    // 检查OBU头部是否已经包含size bit
    bool already_has_size = obuHasSize(obu_header);

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
            return false;
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
    _drop_frame = false;
    _current_frame_starts_new_sequence = false;
    _has_last_seq = false;
    _has_last_stamp = false;
    _unit_completed = false;
    _received_keyframe = false;
}

} // namespace mediakit
