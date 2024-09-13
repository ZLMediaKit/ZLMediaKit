/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AACRtp.h"

namespace mediakit{

bool AACRtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = (char *)frame->data() + frame->prefixSize();
    auto size = frame->size() - frame->prefixSize();
    auto remain_size = size;
    auto max_size = getRtpInfo().getMaxSize() - 4;
    while (remain_size > 0) {
        if (remain_size <= max_size) {
            outputRtp(ptr, remain_size, size, true, frame->dts());
            break;
        }
        outputRtp(ptr, max_size, size, false, frame->dts());
        ptr += max_size;
        remain_size -= max_size;
    }
    return true;
}

void AACRtpEncoder::outputRtp(const char *data, size_t len, size_t total_len, bool mark, uint64_t stamp) {
    auto rtp = getRtpInfo().makeRtp(TrackAudio, nullptr, len + 4, mark, stamp);
    auto payload = rtp->data() + RtpPacket::kRtpTcpHeaderSize + RtpPacket::kRtpHeaderSize;
    payload[0] = 0;
    payload[1] = 16;
    payload[2] = ((total_len) >> 5) & 0xFF;
    payload[3] = ((total_len & 0x1F) << 3) & 0xFF;
    memcpy(payload + 4, data, len);
    RtpCodec::inputRtp(std::move(rtp), false);
}

/////////////////////////////////////////////////////////////////////////////////////

AACRtpDecoder::AACRtpDecoder() {
    obtainFrame();
}

void AACRtpDecoder::obtainFrame() {
    // 从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象  [AUTO-TRANSLATED:f85fe201]
    // Re-apply the object from the cache pool to prevent overwriting the object that has been written to the ring buffer
    _frame = FrameImp::create();
    _frame->_codec_id = CodecAAC;
}

bool AACRtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= 0) {
        // 无实际负载  [AUTO-TRANSLATED:2267e6ac]
        // No actual load
        return false;
    }

    auto stamp = rtp->getStampMS();
    // rtp数据开始部分  [AUTO-TRANSLATED:f22ebdb9]
    // Start of rtp data
    auto ptr = rtp->getPayload();
    // rtp数据末尾  [AUTO-TRANSLATED:ee108f2b]
    // End of rtp data
    auto end = ptr + payload_size;
    // 首2字节表示Au-Header的个数，单位bit，所以除以16得到Au-Header个数  [AUTO-TRANSLATED:c7175051]
    // The first 2 bytes represent the number of Au-Headers, in bits, so divide by 16 to get the number of Au-Headers
    auto au_header_count = ((ptr[0] << 8) | ptr[1]) >> 4;
    if (!au_header_count) {
        // 问题issue: https://github.com/ZLMediaKit/ZLMediaKit/issues/1869  [AUTO-TRANSLATED:14be1ff8]
        // Issue: https://github.com/ZLMediaKit/ZLMediaKit/issues/1869
        WarnL << "invalid aac rtp au_header_count";
        return false;
    }
    // 记录au_header起始指针  [AUTO-TRANSLATED:b9083b72]
    // Record the starting pointer of au_header
    auto au_header_ptr = ptr + 2;
    ptr = au_header_ptr + au_header_count * 2;

    if (end < ptr) {
        // 数据不够  [AUTO-TRANSLATED:830a2785]
        // Not enough data
        return false;
    }

    if (!_last_dts) {
        // 记录第一个时间戳  [AUTO-TRANSLATED:2e85b398]
        // Record the first timestamp
        _last_dts = stamp;
    }

    // 每个audio unit时间戳增量  [AUTO-TRANSLATED:0345240c]
    // Timestamp increment for each audio unit
    auto dts_inc = static_cast<int64_t>(stamp - _last_dts) / au_header_count;
    if (dts_inc < 0 || dts_inc > 100) {
        // 时间戳增量异常，忽略  [AUTO-TRANSLATED:d2750ef8]
        // Timestamp increment anomaly, ignore
        dts_inc = 0;
    }

    for (auto i = 0u; i < (size_t)au_header_count; ++i) {
        // 之后的2字节是AU_HEADER,其中高13位表示一帧AAC负载的字节长度，低3位无用  [AUTO-TRANSLATED:404eb444]
        // The following 2 bytes are AU_HEADER, where the high 13 bits represent the byte length of one frame of AAC payload, and the low 3 bits are useless
        auto size = ((au_header_ptr[0] << 8) | au_header_ptr[1]) >> 3;
        auto len = std::min<int>(size, end - ptr);
        if (len <= 0) {
            break;
        }
        _frame->_buffer.append((char *)ptr, len);
        ptr += len;
        au_header_ptr += 2;

        if (_frame->size() >= (size_t)size) {
            // 设置当前audio unit时间戳  [AUTO-TRANSLATED:eee18d6e]
            // Set the current audio unit timestamp
            _frame->_dts = _last_dts + i * dts_inc;
            flushData();
        }
    }
    // 记录上次时间戳  [AUTO-TRANSLATED:a830d69f]
    // Record the last timestamp
    _last_dts = stamp;
    return false;
}

void AACRtpDecoder::flushData() {
    auto ptr = reinterpret_cast<const uint8_t *>(_frame->data());
    if ((ptr[0] == 0xFF && (ptr[1] & 0xF0) == 0xF0) && _frame->size() > ADTS_HEADER_LEN) {
        // adts头打入了rtp包，不符合规范，兼容EasyPusher的bug  [AUTO-TRANSLATED:203a5ee9]
        // The adts header is inserted into the rtp packet, which is not compliant with the specification, compatible with the bug of EasyPusher
        _frame->_prefix_size = ADTS_HEADER_LEN;
    }
    RtpCodec::inputFrame(_frame);
    obtainFrame();
}

}//namespace mediakit