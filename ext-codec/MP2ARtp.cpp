/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "MP2ARtp.h"

namespace mediakit {

// ======================== MP2ARtpEncoder ========================

void MP2ARtpEncoder::outputRtp(const char *data, size_t len, size_t frag_offset, bool mark, uint64_t stamp) {
    // RFC 2250 Section 3.5:
    // 4 bytes MPEG Audio-specific header + ES data
    auto rtp = getRtpInfo().makeRtp(TrackAudio, nullptr, len + kMP2AHeaderSize, mark, stamp);
    auto payload = rtp->getPayload();

    // MPEG Audio-specific header
    // MBZ (16 bits) = 0
    payload[0] = 0;
    payload[1] = 0;
    // Frag_offset (16 bits)
    payload[2] = (frag_offset >> 8) & 0xFF;
    payload[3] = frag_offset & 0xFF;

    // ES data
    memcpy(payload + kMP2AHeaderSize, data, len);

    RtpCodec::inputRtp(std::move(rtp), false);
}

bool MP2ARtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto data = (const uint8_t *)frame->data() + frame->prefixSize();
    auto total_size = (size_t)(frame->size() - frame->prefixSize());
    if (total_size <= 0) {
        return false;
    }

    auto max_payload = getRtpInfo().getMaxSize() - kMP2AHeaderSize;
    auto base_dts = frame->dts();

    // TS demux 可能一次回调多个完整的 MPEG Audio 帧（一个 PES 包），
    // 需要逐帧解析并独立打 RTP 包，否则 FFmpeg 等接收端会因为分片
    // 导致 RTP payload 不以 sync word 开头而报 "Header missing"。
    size_t pos = 0;
    int frame_index = 0;

    while (pos + 4 <= total_size) {
        // 检查 MPEG Audio sync word
        if (data[pos] != 0xFF || (data[pos + 1] & 0xE0) != 0xE0) {
            // 跳过无效字节，寻找下一个 sync word
            ++pos;
            continue;
        }

        // 解析帧头获取帧大小
        MpegAudioFrameInfo info;
        if (!MpegAudioFrameInfo::parse(data + pos, total_size - pos, info) || info.frame_size <= 0) {
            ++pos;
            continue;
        }

        size_t frame_size = (size_t)info.frame_size;
        if (pos + frame_size > total_size) {
            // 不完整的帧，打包剩余数据
            frame_size = total_size - pos;
        }

        // 计算当前帧的时间戳偏移（毫秒）
        // 每帧 samples_per_frame 个采样点，采样率 info.sample_rate
        uint64_t stamp = base_dts;
        if (frame_index > 0 && info.sample_rate > 0) {
            stamp += (uint64_t)frame_index * info.samples_per_frame * 1000 / info.sample_rate;
        }

        // 对单个 MPEG Audio 帧打 RTP 包
        auto ptr = (const char *)(data + pos);
        size_t remain = frame_size;
        size_t frag_offset = 0;

        while (remain > 0) {
            if (remain <= max_payload) {
                outputRtp(ptr, remain, frag_offset, true, stamp);
                break;
            }
            outputRtp(ptr, max_payload, frag_offset, false, stamp);
            ptr += max_payload;
            remain -= max_payload;
            frag_offset += max_payload;
        }

        pos += frame_size;
        ++frame_index;
    }

    return true;
}

// ======================== MP2ARtpDecoder ========================

MP2ARtpDecoder::MP2ARtpDecoder() {
    obtainFrame();
}

void MP2ARtpDecoder::obtainFrame() {
    _frame = FrameImp::create<MP2AFrame>();
}

void MP2ARtpDecoder::flushData() {
    if (_frame->_buffer.empty()) {
        return;
    }
    RtpCodec::inputFrame(_frame);
    obtainFrame();
}

bool MP2ARtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= (ssize_t)kMP2AHeaderSize) {
        // 负载太小，没有有效 ES 数据
        return false;
    }

    auto payload = rtp->getPayload();
    auto stamp = rtp->getStamp();
    auto seq = rtp->getSeq();

    // 解析 MPEG Audio-specific header (RFC 2250 Section 3.5)
    // MBZ (16 bits) + Frag_offset (16 bits)
    uint16_t frag_offset = (payload[2] << 8) | payload[3];

    auto es_data = payload + kMP2AHeaderSize;
    auto es_size = payload_size - kMP2AHeaderSize;

    if (frag_offset == 0) {
        // frag_offset == 0 表示这是一个新帧（或完整帧）的开始
        // 先输出之前缓存的帧（如果有）
        flushData();
        // 使用 90kHz 时间戳转换为毫秒
        _frame->_dts = rtp->getStampMS();
        _frame->_pts = _frame->_dts;
    } else if (_frame->_buffer.empty()) {
        // frag_offset != 0 但 buffer 为空，说明丢了第一个分片包，丢弃
        _last_seq = seq;
        _last_stamp = stamp;
        return false;
    } else if (seq != (uint16_t)(_last_seq + 1)) {
        // 分片包 seq 不连续，丢包了，丢弃当前帧
        WarnL << "mp2a rtp packet loss:" << _last_seq << " -> " << seq;
        _frame->_buffer.clear();
        _last_seq = seq;
        _last_stamp = stamp;
        return false;
    }

    _last_seq = seq;
    _last_stamp = stamp;

    // 追加 ES 数据
    _frame->_buffer.append((char *)es_data, es_size);

    // mark bit 表示帧的最后一个 RTP 包，立即输出
    if (rtp->getHeader()->mark) {
        flushData();
    }

    return false;
}

} // namespace mediakit
