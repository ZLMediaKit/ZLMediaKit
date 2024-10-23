/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "VP8Rtp.h"
#include "Extension/Frame.h"
#include "Common/config.h"

namespace mediakit{

const int16_t kNoPictureId = -1;
const int8_t  kNoTl0PicIdx = -1;
const uint8_t kNoTemporalIdx = 0xFF;
const int kNoKeyIdx = -1;

// internal bits
constexpr int kXBit = 0x80;
constexpr int kNBit = 0x20;
constexpr int kSBit = 0x10;
constexpr int kKeyIdxField = 0x1F;
constexpr int kIBit = 0x80;
constexpr int kLBit = 0x40;
constexpr int kTBit = 0x20;
constexpr int kKBit = 0x10;
constexpr int kYBit = 0x20;
constexpr int kFailedToParse = 0;
// VP8 payload descriptor
// https://datatracker.ietf.org/doc/html/rfc7741#section-4.2
//
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |X|R|N|S|R| PID | (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   |I|L|T|K| RSV   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// I:   |M| PictureID   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//      |   PictureID   |
//      +-+-+-+-+-+-+-+-+
// L:   |   TL0PICIDX   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// T/K: |TID|Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
struct RTPVideoHeaderVP8 {
    void InitRTPVideoHeaderVP8();
    
    int Size() const;
    int Write(uint8_t *data, int size) const;
    int Read(const uint8_t *data, int data_length);
    bool isFirstPacket() const { return beginningOfPartition && partitionId == 0; }
    friend bool operator!=(const RTPVideoHeaderVP8 &lhs, const RTPVideoHeaderVP8 &rhs) { return !(lhs == rhs); }
    friend bool operator==(const RTPVideoHeaderVP8 &lhs, const RTPVideoHeaderVP8 &rhs) {
        return lhs.nonReference == rhs.nonReference && lhs.pictureId == rhs.pictureId && lhs.tl0PicIdx == rhs.tl0PicIdx && lhs.temporalIdx == rhs.temporalIdx
            && lhs.layerSync == rhs.layerSync && lhs.keyIdx == rhs.keyIdx && lhs.partitionId == rhs.partitionId
            && lhs.beginningOfPartition == rhs.beginningOfPartition;
    }

    bool nonReference; // Frame is discardable.
    int16_t pictureId; // Picture ID index, 15 bits;
                       // kNoPictureId if PictureID does not exist.
    int8_t tl0PicIdx; // TL0PIC_IDX, 8 bits;
                       // kNoTl0PicIdx means no value provided.
    uint8_t temporalIdx; // Temporal layer index, or kNoTemporalIdx.
    bool layerSync; // This frame is a layer sync frame.
                    // Disabled if temporalIdx == kNoTemporalIdx.
    int8_t keyIdx; // 5 bits; kNoKeyIdx means not used.
    int8_t partitionId; // VP8 partition ID
    bool beginningOfPartition; // True if this packet is the first
                               // in a VP8 partition. Otherwise false
};

void RTPVideoHeaderVP8::InitRTPVideoHeaderVP8() {
    nonReference = false;
    pictureId = kNoPictureId;
    tl0PicIdx = kNoTl0PicIdx;
    temporalIdx = kNoTemporalIdx;
    layerSync = false;
    keyIdx = kNoKeyIdx;
    partitionId = 0;
    beginningOfPartition = false;
}

int RTPVideoHeaderVP8::Size() const {
    bool tid_present = this->temporalIdx != kNoTemporalIdx;
    bool keyid_present = this->keyIdx != kNoKeyIdx;
    bool tl0_pid_present = this->tl0PicIdx != kNoTl0PicIdx;
    bool pid_present = this->pictureId != kNoPictureId;
    int ret = 2;
    if (pid_present)
        ret += 2;
    if (tl0_pid_present)
        ret++;
    if (tid_present || keyid_present)
        ret++;
    return ret == 2 ? 1 : ret;
}

int RTPVideoHeaderVP8::Write(uint8_t *data, int size) const {
    int ret = 0;
    bool tid_present = this->temporalIdx != kNoTemporalIdx;
    bool keyid_present = this->keyIdx != kNoKeyIdx;
    bool tl0_pid_present = this->tl0PicIdx != kNoTl0PicIdx;
    bool pid_present = this->pictureId != kNoPictureId;
    uint8_t x_field = 0;
    if (pid_present)
        x_field |= kIBit;
    if (tl0_pid_present)
        x_field |= kLBit;
    if (tid_present)
        x_field |= kTBit;
    if (keyid_present)
        x_field |= kKBit;

    uint8_t flags = 0;
    if (x_field != 0)
        flags |= kXBit;
    if (this->nonReference)
        flags |= kNBit;
    // Create header as first packet in the frame. NextPacket() will clear it
    // after first use.
    flags |= kSBit;
    data[ret++] = flags;
    if (x_field == 0) {
        return ret;
    }
    data[ret++] = x_field;
    if (pid_present) {
        const uint16_t pic_id = static_cast<uint16_t>(this->pictureId);
        data[ret++] = (0x80 | ((pic_id >> 8) & 0x7F));
        data[ret++] = (pic_id & 0xFF);
    }
    if (tl0_pid_present) {
        data[ret++] = this->tl0PicIdx;
    }
    if (tid_present || keyid_present) {
        uint8_t data_field = 0;
        if (tid_present) {
            data_field |= this->temporalIdx << 6;
            if (this->layerSync)
                data_field |= kYBit;
        }
        if (keyid_present) {
            data_field |= (this->keyIdx & kKeyIdxField);
        }
        data[ret++] = data_field;
    }
    return ret;
}

int RTPVideoHeaderVP8::Read(const uint8_t *data, int data_length) {
    // RTC_DCHECK_GT(data_length, 0);
    int parsed_bytes = 0;
    // Parse mandatory first byte of payload descriptor.
    bool extension = (*data & 0x80) ? true : false; // X bit
    this->nonReference = (*data & 0x20) ? true : false; // N bit
    this->beginningOfPartition = (*data & 0x10) ? true : false; // S bit
    this->partitionId = (*data & 0x07); // PID field

    data++;
    parsed_bytes++;
    data_length--;

    if (!extension)
        return parsed_bytes;

    if (data_length == 0)
        return kFailedToParse;
    // Optional X field is present.
    bool has_picture_id = (*data & 0x80) ? true : false; // I bit
    bool has_tl0_pic_idx = (*data & 0x40) ? true : false; // L bit
    bool has_tid = (*data & 0x20) ? true : false; // T bit
    bool has_key_idx = (*data & 0x10) ? true : false; // K bit

    // Advance data and decrease remaining payload size.
    data++;
    parsed_bytes++;
    data_length--;

    if (has_picture_id) {
        if (data_length == 0)
            return kFailedToParse;

        this->pictureId = (*data & 0x7F);
        if (*data & 0x80) {
            data++;
            parsed_bytes++;
            if (--data_length == 0)
                return kFailedToParse;
            // PictureId is 15 bits
            this->pictureId = (this->pictureId << 8) + *data;
        }
        data++;
        parsed_bytes++;
        data_length--;
    }

    if (has_tl0_pic_idx) {
        if (data_length == 0)
            return kFailedToParse;

        this->tl0PicIdx = *data;
        data++;
        parsed_bytes++;
        data_length--;
    }

    if (has_tid || has_key_idx) {
        if (data_length == 0)
            return kFailedToParse;

        if (has_tid) {
            this->temporalIdx = ((*data >> 6) & 0x03);
            this->layerSync = (*data & 0x20) ? true : false; // Y bit
        }
        if (has_key_idx) {
            this->keyIdx = *data & 0x1F;
        }
        data++;
        parsed_bytes++;
        data_length--;
    }
    return parsed_bytes;
}

/////////////////////////////////////////////////
// VP8RtpDecoder
VP8RtpDecoder::VP8RtpDecoder() {
    obtainFrame();
}

void VP8RtpDecoder::obtainFrame() {
    _frame = FrameImp::create<VP8Frame>();
}

bool VP8RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    auto seq = rtp->getSeq();
    bool ret = decodeRtp(rtp);
    if (!_gop_dropped && seq != (uint16_t)(_last_seq + 1) && _last_seq) {
        _gop_dropped = true;
        WarnL << "start drop vp8 gop, last seq:" << _last_seq << ", rtp:\r\n" << rtp->dumpString();
    }
    _last_seq = seq;
    return ret;
}

bool VP8RtpDecoder::decodeRtp(const RtpPacket::Ptr &rtp) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= 0) {
        // No actual payload
        return false;
    }
    auto payload = rtp->getPayload();
    auto stamp = rtp->getStampMS();
    auto seq = rtp->getSeq();

    RTPVideoHeaderVP8 info;
    int offset = info.Read(payload, payload_size);
    if (!offset) {
        //_frame_drop = true;
        return false;
    }
    bool start = info.isFirstPacket();
    if (start) {
        _frame->_pts = stamp;
        _frame->_buffer.clear();
        _frame_drop = false;
    }

    if (_frame_drop) {
        // This frame is incomplete
        return false;
    }

    if (!start && seq != (uint16_t)(_last_seq + 1)) {
        // 中间的或末尾的rtp包，其seq必须连续，否则说明rtp丢包，那么该帧不完整，必须得丢弃
        _frame_drop = true;
        _frame->_buffer.clear();
        return false;
    }
    // Append data
    _frame->_buffer.append((char *)payload + offset, payload_size - offset);
    bool end = rtp->getHeader()->mark;
    if (end) {
        // 确保下一次fu必须收到第一个包
        _frame_drop = true;
        // 该帧最后一个rtp包,输出frame  [AUTO-TRANSLATED:a648aaa5]
        // The last rtp packet of this frame, output frame
        outputFrame(rtp);
    }

    return (info.isFirstPacket() && (payload[offset] & 0x01) == 0);
}

void VP8RtpDecoder::outputFrame(const RtpPacket::Ptr &rtp) {
    if (_frame->dropAble()) {
        // 不参与dts生成  [AUTO-TRANSLATED:dff3b747]
        // Not involved in dts generation
        _frame->_dts = _frame->_pts;
    } else {
        // rtsp没有dts，那么根据pts排序算法生成dts  [AUTO-TRANSLATED:f37c17f3]
        // Rtsp does not have dts, so dts is generated according to the pts sorting algorithm
        _dts_generator.getDts(_frame->_pts, _frame->_dts);
    }

    if (_frame->keyFrame() && _gop_dropped) {
        _gop_dropped = false;
        InfoL << "new gop received, rtp:\r\n" << rtp->dumpString();
    }
    if (!_gop_dropped || _frame->configFrame()) {
        RtpCodec::inputFrame(_frame);
    }
    obtainFrame();
}

////////////////////////////////////////////////////////////////////////

bool VP8RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    RTPVideoHeaderVP8 info;
    info.InitRTPVideoHeaderVP8();
    info.beginningOfPartition = true;
    info.nonReference = !frame->dropAble();
    uint8_t header[20];
    int header_size = info.Write(header, sizeof(header));

    int pdu_size = getRtpInfo().getMaxSize() - header_size;
    const char *ptr = frame->data() + frame->prefixSize();
    size_t len = frame->size() - frame->prefixSize();
    bool key = frame->keyFrame();
    bool mark = false;
    for (size_t pos = 0; pos < len; pos += pdu_size) {
        if (len - pos <= pdu_size) {
            pdu_size = len - pos;
            mark = true;
        }

        auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, pdu_size + header_size, mark, frame->pts());
        if (rtp) {
            uint8_t *payload = rtp->getPayload();
            memcpy(payload, header, header_size);
            memcpy(payload + header_size, ptr + pos, pdu_size);
            RtpCodec::inputRtp(rtp, key);
        }

        key = false;
        header[0] &= (~kSBit); //  Clear 'Start of partition' bit.
    }
    return true;
}

} // namespace mediakit
