/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AACRtp.h"

namespace mediakit{

AACRtpEncoder::AACRtpEncoder(uint32_t ui32Ssrc,
                             uint32_t ui32MtuSize,
                             uint32_t ui32SampleRate,
                             uint8_t ui8PayloadType,
                             uint8_t ui8Interleaved) :
        RtpInfo(ui32Ssrc,
                ui32MtuSize,
                ui32SampleRate,
                ui8PayloadType,
                ui8Interleaved){
}

bool AACRtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto stamp = frame->dts();
    auto data = frame->data() + frame->prefixSize();
    auto len = frame->size() - frame->prefixSize();
    auto ptr = (char *) data;
    auto remain_size = len;
    auto max_size = getMaxSize() - 4;
    while (remain_size > 0) {
        if (remain_size <= max_size) {
            _section_buf[0] = 0;
            _section_buf[1] = 16;
            _section_buf[2] = (len >> 5) & 0xFF;
            _section_buf[3] = ((len & 0x1F) << 3) & 0xFF;
            memcpy(_section_buf + 4, ptr, remain_size);
            makeAACRtp(_section_buf, remain_size + 4, true, stamp);
            break;
        }
        _section_buf[0] = 0;
        _section_buf[1] = 16;
        _section_buf[2] = ((len) >> 5) & 0xFF;
        _section_buf[3] = ((len & 0x1F) << 3) & 0xFF;
        memcpy(_section_buf + 4, ptr, max_size);
        makeAACRtp(_section_buf, max_size + 4, false, stamp);
        ptr += max_size;
        remain_size -= max_size;
    }
    return len > 0;
}

void AACRtpEncoder::makeAACRtp(const void *data, size_t len, bool mark, uint64_t stamp) {
    RtpCodec::inputRtp(makeRtp(getTrackType(), data, len, mark, stamp), false);
}

/////////////////////////////////////////////////////////////////////////////////////

AACRtpDecoder::AACRtpDecoder(const Track::Ptr &track) {
    auto aacTrack = std::dynamic_pointer_cast<AACTrack>(track);
    if (!aacTrack || !aacTrack->ready()) {
        WarnL << "该aac track无效!";
    } else {
        _aac_cfg = aacTrack->getAacCfg();
    }
    obtainFrame();
}

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
        //adts头打入了rtp包，不符合规范，兼容EasyPusher的bug
        _frame->_prefix_size = ADTS_HEADER_LEN;
    } else {
        //没有adts头则插入adts头
        char adts_header[128] = {0};
        auto size = dumpAacConfig(_aac_cfg, _frame->_buffer.size(), (uint8_t *) adts_header, sizeof(adts_header));
        if (size > 0) {
            //插入adts头
            _frame->_buffer.insert(0, adts_header, size);
            _frame->_prefix_size = size;
        }
    }
    RtpCodec::inputFrame(_frame);
    obtainFrame();
}

}//namespace mediakit