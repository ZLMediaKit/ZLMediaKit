/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "MP2VRtp.h"
#include "Common/config.h"

namespace mediakit {

// ======================== MP2VRtpDecoder ========================

MP2VRtpDecoder::MP2VRtpDecoder() {
    obtainFrame();
}

void MP2VRtpDecoder::obtainFrame() {
    _frame = FrameImp::create<MP2VFrame>();
}

bool MP2VRtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    auto seq = rtp->getSeq();
    auto last_gop_dropped = _gop_dropped;
    bool is_gop_start = decodeRtp(rtp);
    if (!_gop_dropped && seq != (uint16_t)(_last_seq + 1) && _last_seq) {
        _gop_dropped = true;
        WarnL << "start drop mp2v gop, last seq:" << _last_seq << ", rtp:\r\n" << rtp->dumpString();
    }
    _last_seq = seq;
    return is_gop_start && !last_gop_dropped;
}

/**
 * RFC 2250 MPEG Video-specific header (4 bytes):
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |    MBZ  |T|         TR        |AN|N|S|B|E|  P  | | BFC | | FFC |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                                    FBV     FFV
 *
 * T: MPEG-2 specific header extension present (1 bit)
 * TR: Temporal Reference (10 bits)
 * AN: Active N bit (1 bit)
 * N: New picture header (1 bit)
 * S: Sequence-header-present (1 bit)
 * B: Beginning-of-slice (1 bit)
 * E: End-of-slice (1 bit)
 * P: Picture-Type (3 bits): I(1), P(2), B(3), D(4)
 * FBV: full_pel_backward_vector (1 bit)
 * BFC: backward_f_code (3 bits)
 * FFV: full_pel_forward_vector (1 bit)
 * FFC: forward_f_code (3 bits)
 */
bool MP2VRtpDecoder::decodeRtp(const RtpPacket::Ptr &rtp) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= (ssize_t)kMP2VHeaderSize) {
        // 负载太小，不包含有效数据
        return false;
    }
    auto payload = rtp->getPayload();
    auto stamp = rtp->getStampMS();
    auto seq = rtp->getSeq();

    // 解析 RFC 2250 MPEG Video-specific header
    bool t_bit = (payload[0] >> 2) & 0x01;
    // uint16_t temporal_ref = ((payload[0] & 0x03) << 8) | payload[1];
    // bool seq_header_present = (payload[2] >> 5) & 0x01;
    // bool begin_of_slice = (payload[2] >> 4) & 0x01;
    // bool end_of_slice = (payload[2] >> 3) & 0x01;
    uint8_t picture_type = (payload[2] & 0x07);

    // 如果 T bit 置位，还有 4 字节的 MPEG-2 扩展头需要跳过
    size_t header_size = kMP2VHeaderSize + (t_bit ? 4 : 0);
    if (payload_size <= (ssize_t)header_size) {
        return false;
    }

    auto es_data = payload + header_size;
    auto es_size = payload_size - header_size;

    // 检查是否为新帧（时间戳变化）
    if (!_frame->_buffer.empty() && stamp != _frame->_pts) {
        // 时间戳变化，输出上一帧
        outputFrame(rtp);
    }

    if (_frame->_buffer.empty()) {
        // 新帧开始
        _frame->_pts = stamp;
        _drop_flag = false;
        _picture_type = picture_type;
    }

    if (_drop_flag) {
        return false;
    }

    // 检测 seq 不连续，丢弃当前帧
    if (!_frame->_buffer.empty() && seq != (uint16_t)(_last_seq + 1) && _last_seq) {
        _drop_flag = true;
        _frame->_buffer.clear();
        return false;
    }

    // 追加 ES 数据
    _frame->_buffer.append((char *)es_data, es_size);

    // RTP mark bit 标识帧结束
    if (rtp->getHeader()->mark) {
        outputFrame(rtp);
        return _picture_type == 1; // I-Picture
    }

    return false;
}

void MP2VRtpDecoder::outputFrame(const RtpPacket::Ptr &rtp) {
    if (_frame->_buffer.empty()) {
        return;
    }

    // 生成 DTS（MPEG-2 有 B 帧，PTS 和 DTS 不一定相同）
    _dts_generator.getDts(_frame->_pts, _frame->_dts);

    bool is_key = _frame->keyFrame();
    if (is_key && _gop_dropped) {
        _gop_dropped = false;
        InfoL << "new mp2v gop received, rtp:\r\n" << rtp->dumpString();
    }
    if (!_gop_dropped) {
        RtpCodec::inputFrame(_frame);
    }
    obtainFrame();
}

// ======================== MP2VRtpEncoder ========================

bool MP2VRtpEncoder::hasSequenceHeader(const uint8_t *data, size_t size) {
    // 查找 sequence header start code: 00 00 01 B3
    for (size_t i = 0; i + 3 < size; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01 && data[i + 3] == 0xB3) {
            return true;
        }
    }
    return false;
}

void MP2VRtpEncoder::parsePictureInfo(const uint8_t *data, size_t size) {
    _temporal_ref = 0;
    _picture_type = 0;
    _fbv = 0;
    _bfc = 0;
    _ffv = 0;
    _ffc = 0;
    _has_seq_header = hasSequenceHeader(data, size);

    // 查找 picture start code: 00 00 01 00
    for (size_t i = 0; i + 5 < size; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01 && data[i + 3] == 0x00) {
            // temporal_reference: 10 bits, picture_coding_type: 3 bits
            _temporal_ref = (data[i + 4] << 2) | ((data[i + 5] >> 6) & 0x03);
            _picture_type = (data[i + 5] >> 3) & 0x07;

            // 解析 motion vector codes (vbv_delay 之后)
            // picture header: temporal_reference(10) + picture_coding_type(3) + vbv_delay(16)
            if (i + 8 < size) {
                uint8_t extra_byte = data[i + 8];
                if (_picture_type == 2 /* P */ || _picture_type == 3 /* B */) {
                    // full_pel_forward_vector(1) + forward_f_code(3)
                    _ffv = (extra_byte >> 2) & 0x01;
                    _ffc = ((extra_byte & 0x03) << 1);
                    if (i + 9 < size) {
                        _ffc |= (data[i + 9] >> 7) & 0x01;
                    }
                }
                if (_picture_type == 3 /* B */) {
                    // full_pel_backward_vector(1) + backward_f_code(3) 紧跟在 forward 之后
                    if (i + 9 < size) {
                        _fbv = (data[i + 9] >> 6) & 0x01;
                        _bfc = (data[i + 9] >> 3) & 0x07;
                    }
                }
            }
            return;
        }
    }
}

void MP2VRtpEncoder::buildMpvHeader(uint8_t *buf, const uint8_t *data, size_t size,
                                     bool is_begin_of_slice, bool is_end_of_slice) {
    // RFC 2250 Section 3.4
    // Byte 0: MBZ(5) + T(1) + TR high 2 bits
    // T = 0 (不发送 MPEG-2 扩展头)
    buf[0] = (_temporal_ref >> 8) & 0x03;

    // Byte 1: TR low 8 bits
    buf[1] = _temporal_ref & 0xFF;

    // Byte 2: AN(1) + N(1) + S(1) + B(1) + E(1) + P(3)
    uint8_t byte2 = 0;
    // AN = 0, N = 0
    if (_has_seq_header) {
        byte2 |= 0x20; // S bit
    }
    if (is_begin_of_slice) {
        byte2 |= 0x10; // B bit
    }
    if (is_end_of_slice) {
        byte2 |= 0x08; // E bit
    }
    byte2 |= (_picture_type & 0x07);
    buf[2] = byte2;

    // Byte 3: FBV(1) + BFC(3) + FFV(1) + FFC(3)
    buf[3] = ((_fbv & 0x01) << 7) | ((_bfc & 0x07) << 4) | ((_ffv & 0x01) << 3) | (_ffc & 0x07);
}

bool MP2VRtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = (const uint8_t *)frame->data() + frame->prefixSize();
    auto size = frame->size() - frame->prefixSize();
    if (size == 0) {
        return false;
    }

    // 解析帧信息（picture type, temporal reference 等）
    parsePictureInfo(ptr, size);

    bool is_key = frame->keyFrame();
    auto max_payload = getRtpInfo().getMaxSize() - kMP2VHeaderSize;
    size_t offset = 0;

    while (offset < size) {
        bool is_first = (offset == 0);
        size_t payload_size;
        bool is_last;

        if (size - offset <= max_payload) {
            payload_size = size - offset;
            is_last = true;
        } else {
            payload_size = max_payload;
            is_last = false;
        }

        // 构建 MPEG Video-specific header
        uint8_t mpv_header[kMP2VHeaderSize];
        buildMpvHeader(mpv_header, ptr + offset, payload_size, is_first, is_last);

        // 创建 RTP 包：MPEG header + ES data
        auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, kMP2VHeaderSize + payload_size, is_last, frame->pts());
        auto rtp_payload = rtp->getPayload();

        // 写入 MPEG Video-specific header
        memcpy(rtp_payload, mpv_header, kMP2VHeaderSize);
        // 写入 ES 数据
        memcpy(rtp_payload + kMP2VHeaderSize, ptr + offset, payload_size);

        // 输入到 RTP 环形缓存
        RtpCodec::inputRtp(rtp, is_key && is_first);

        offset += payload_size;
    }

    return true;
}

} // namespace mediakit
