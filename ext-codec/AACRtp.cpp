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
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    _frame = FrameImp::create();
    _frame->_codec_id = CodecAAC;
}

bool AACRtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= 0) {
        //无实际负载
        return false;
    }

    auto stamp = rtp->getStampMS();
    //rtp数据开始部分
    auto ptr = rtp->getPayload();
    //rtp数据末尾
    auto end = ptr + payload_size;
    //首2字节表示Au-Header的个数，单位bit，所以除以16得到Au-Header个数
    auto au_header_count = ((ptr[0] << 8) | ptr[1]) >> 4;
    if (!au_header_count) {
        //问题issue: https://github.com/ZLMediaKit/ZLMediaKit/issues/1869
        WarnL << "invalid aac rtp au_header_count";
        return false;
    }
    //记录au_header起始指针
    auto au_header_ptr = ptr + 2;
    ptr = au_header_ptr +  au_header_count * 2;

    if (end < ptr) {
        //数据不够
        return false;
    }

    if (!_last_dts) {
        //记录第一个时间戳
        _last_dts = stamp;
    }

    //每个audio unit时间戳增量
    auto dts_inc = (stamp - _last_dts) / au_header_count;
    if (dts_inc < 0 && dts_inc > 100) {
        //时间戳增量异常，忽略
        dts_inc = 0;
    }

    for (int i = 0; i < au_header_count; ++i) {
        // 之后的2字节是AU_HEADER,其中高13位表示一帧AAC负载的字节长度，低3位无用
        uint16_t size = ((au_header_ptr[0] << 8) | au_header_ptr[1]) >> 3;
        if (ptr + size > end) {
            //数据不够
            break;
        }

        if (size) {
            //设置aac数据
            _frame->_buffer.assign((char *) ptr, size);
            //设置当前audio unit时间戳
            _frame->_dts = _last_dts + i * dts_inc;
            ptr += size;
            au_header_ptr += 2;
            flushData();
        }
    }
    //记录上次时间戳
    _last_dts = stamp;
    return false;
}

void AACRtpDecoder::flushData() {
    auto ptr = reinterpret_cast<const uint8_t *>(_frame->data());
    if ((ptr[0] == 0xFF && (ptr[1] & 0xF0) == 0xF0) && _frame->size() > ADTS_HEADER_LEN) {
        // adts头打入了rtp包，不符合规范，兼容EasyPusher的bug
        _frame->_prefix_size = ADTS_HEADER_LEN;
    }
    RtpCodec::inputFrame(_frame);
    obtainFrame();
}

}//namespace mediakit