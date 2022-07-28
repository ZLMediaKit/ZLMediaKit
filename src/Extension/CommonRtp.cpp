/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "CommonRtp.h"

using namespace mediakit;

CommonRtpDecoder::CommonRtpDecoder(CodecId codec, size_t max_frame_size ){
    _codec = codec;
    _max_frame_size = max_frame_size;
    obtainFrame();
}

CodecId CommonRtpDecoder::getCodecId() const {
    return _codec;
}

void CommonRtpDecoder::obtainFrame() {
    _frame = FrameImp::create();
    _frame->_codec_id = _codec;
}

bool CommonRtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool){
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= 0) {
        //无实际负载
        return false;
    }
    auto payload = rtp->getPayload();
    auto stamp = rtp->getStampMS();
    auto seq = rtp->getSeq();

    if (_frame->_dts != stamp || _frame->_buffer.size() > _max_frame_size) {
        //时间戳发生变化或者缓存超过MAX_FRAME_SIZE，则清空上帧数据
        if (!_frame->_buffer.empty()) {
            //有有效帧，则输出
            RtpCodec::inputFrame(_frame);
        }

        //新的一帧数据
        obtainFrame();
        _frame->_dts = stamp;
        _drop_flag = false;
    } else if (_last_seq != 0 && (uint16_t)(_last_seq + 1) != seq) {
        //时间戳未发生变化，但是seq却不连续，说明中间rtp丢包了，那么整帧应该废弃
        WarnL << "rtp丢包:" << _last_seq << " -> " << seq;
        _drop_flag = true;
        _frame->_buffer.clear();
    }

    if (!_drop_flag) {
        _frame->_buffer.append((char *)payload, payload_size);
    }

    _last_seq = seq;
    return false;
}

////////////////////////////////////////////////////////////////

CommonRtpEncoder::CommonRtpEncoder(CodecId codec, uint32_t ssrc, uint32_t mtu_size,
                                   uint32_t sample_rate,  uint8_t payload_type, uint8_t interleaved)
        : CommonRtpDecoder(codec), RtpInfo(ssrc, mtu_size, sample_rate, payload_type, interleaved) {
}

bool CommonRtpEncoder::inputFrame(const Frame::Ptr &frame){
    auto stamp = frame->pts();
    auto ptr = frame->data() + frame->prefixSize();
    auto len = frame->size() - frame->prefixSize();
    auto remain_size = len;
    auto max_size = getMaxSize();
    bool is_key = frame->keyFrame();
    bool mark = false;
    while (remain_size > 0) {
        size_t rtp_size;
        if (remain_size > max_size) {
            rtp_size = max_size;
        } else {
            rtp_size = remain_size;
            mark = true;
        }
        RtpCodec::inputRtp(makeRtp(getTrackType(), ptr, rtp_size, mark, stamp), is_key);
        ptr += rtp_size;
        remain_size -= rtp_size;
        is_key = false;
    }
    return len > 0;
}