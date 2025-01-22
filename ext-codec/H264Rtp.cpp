/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H264Rtp.h"
#include "Common/config.h"

namespace mediakit{

#pragma pack(push, 1)

class FuFlags {
public:
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned start_bit: 1;
    unsigned end_bit: 1;
    unsigned reserved: 1;
    unsigned nal_type: 5;
#else
    unsigned nal_type: 5;
    unsigned reserved: 1;
    unsigned end_bit: 1;
    unsigned start_bit: 1;
#endif
};

#pragma pack(pop)

H264RtpDecoder::H264RtpDecoder() {
    _frame = obtainFrame();
}

H264Frame::Ptr H264RtpDecoder::obtainFrame() {
    auto frame = FrameImp::create<H264Frame>();
    frame->_prefix_size = 4;
    return frame;
}

bool H264RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    auto seq = rtp->getSeq();
    auto last_is_gop = _is_gop;
    _is_gop = decodeRtp(rtp);
    if (!_gop_dropped && seq != (uint16_t)(_last_seq + 1) && _last_seq) {
        _gop_dropped = true;
        WarnL << "start drop h264 gop, last seq:" << _last_seq << ", rtp:\r\n" << rtp->dumpString();
    }
    _last_seq = seq;
    // 确保有sps rtp的时候，gop从sps开始；否则从关键帧开始  [AUTO-TRANSLATED:115ae07c]
    // cpp
// Ensure that when there is sps rtp, the gop starts from sps; otherwise, it starts from the key frame
    return _is_gop && !last_is_gop;
}

/*
RTF3984 5.2节  Common Structure of the RTP Payload Format
Table 1.  Summary of NAL unit types and their payload structures

   Type   Packet    Type name                        Section
   ---------------------------------------------------------
   0      undefined                                    -
   1-23   NAL unit  Single NAL unit packet per H.264   5.6
   24     STAP-A    Single-time aggregation packet     5.7.1
   25     STAP-B    Single-time aggregation packet     5.7.1
   26     MTAP16    Multi-time aggregation packet      5.7.2
   27     MTAP24    Multi-time aggregation packet      5.7.2
   28     FU-A      Fragmentation unit                 5.8
   29     FU-B      Fragmentation unit                 5.8
   30-31  undefined                                    -
 /*
 RTF3984 Section 5.2  Common Structure of the RTP Payload Format
 Table 1.  Summary of NAL unit types and their payload structures
 
 Type   Packet    Type name                        Section
 ---------------------------------------------------------
 0      undefined                                    -
 1-23   NAL unit  Single NAL unit packet per H.264   5.6
 24     STAP-A    Single-time aggregation packet     5.7.1
 25     STAP-B    Single-time aggregation packet     5.7.1
 26     MTAP16    Multi-time aggregation packet      5.7.2
 27     MTAP24    Multi-time aggregation packet      5.7.2
 28     FU-A      Fragmentation unit                 5.8
 29     FU-B      Fragmentation unit                 5.8
 30-31  undefined                                    -
 
 * [AUTO-TRANSLATED:57545317]
*/

bool H264RtpDecoder::singleFrame(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp){
    _frame->_buffer.assign("\x00\x00\x00\x01", 4);
    _frame->_buffer.append((char *) ptr, size);
    _frame->_pts = stamp;
    auto key = _frame->keyFrame() || _frame->configFrame();
    outputFrame(rtp, _frame);
    return key;
}

bool H264RtpDecoder::unpackStapA(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp) {
    // STAP-A 单一时间的组合包  [AUTO-TRANSLATED:cfa62307]
    // STAP-A single-time aggregation packet
    auto have_key_frame = false;
    auto end = ptr + size;
    while (ptr + 2 < end) {
        uint16_t len = (ptr[0] << 8) | ptr[1];
        if (!len || ptr + len > end) {
            WarnL << "invalid rtp data size:" << len << ",rtp:\r\n" << rtp->dumpString();
            _gop_dropped = true;
            break;
        }
        ptr += 2;
        if (singleFrame(rtp, ptr, len, stamp)) {
            have_key_frame = true;
        }
        ptr += len;
    }
    return have_key_frame;
}

bool H264RtpDecoder::mergeFu(const RtpPacket::Ptr &rtp, const uint8_t *ptr, ssize_t size, uint64_t stamp, uint16_t seq){
    auto nal_suffix = *ptr & (~0x1F);
    FuFlags *fu = (FuFlags *) (ptr + 1);
    if (fu->start_bit) {
        // 该帧的第一个rtp包  [AUTO-TRANSLATED:a9581a23]
        // The first rtp packet of this frame
        _frame->_buffer.assign("\x00\x00\x00\x01", 4);
        _frame->_buffer.push_back(nal_suffix | fu->nal_type);
        _frame->_pts = stamp;
        _fu_dropped = false;
    }

    if (_fu_dropped) {
        // 该帧不完整  [AUTO-TRANSLATED:6bd7eca7]
        // This frame is incomplete
        return false;
    }

    if (!fu->start_bit && seq != (uint16_t) (_last_seq + 1)) {
        // 中间的或末尾的rtp包，其seq必须连续，否则说明rtp丢包，那么该帧不完整，必须得丢弃  [AUTO-TRANSLATED:6953b332]
        // The middle or end rtp packet, its seq must be continuous, otherwise it indicates that the rtp packet is lost, then the frame is incomplete and must be discarded
        _fu_dropped = true;
        _frame->_buffer.clear();
        return false;
    }

    // 后面追加数据  [AUTO-TRANSLATED:248516e9]
    // Append data
    _frame->_buffer.append((char *) ptr + 2, size - 2);

    if (!fu->end_bit) {
        // 非末尾包  [AUTO-TRANSLATED:2e43ac3c]
        // Not the end packet
        return fu->start_bit ? (_frame->keyFrame() || _frame->configFrame()) : false;
    }

    // 确保下一次fu必须收到第一个包  [AUTO-TRANSLATED:491d81ec]
    // Ensure that the next fu must receive the first packet
    _fu_dropped = true;
    // 该帧最后一个rtp包,输出frame  [AUTO-TRANSLATED:a648aaa5]
    // The last rtp packet of this frame, output frame
    outputFrame(rtp, _frame);
    return false;
}

bool H264RtpDecoder::decodeRtp(const RtpPacket::Ptr &rtp) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= 0) {
        // 无实际负载  [AUTO-TRANSLATED:305af48f]
        // No actual payload
        return false;
    }
    auto frame = rtp->getPayload();
    auto stamp = rtp->getStampMS();
    auto seq = rtp->getSeq();
    int nal = H264_TYPE(frame[0]);

    switch (nal) {
        case 24:
            // 24 STAP-A Single-time aggregation packet 5.7.1
            return unpackStapA(rtp, frame + 1, payload_size - 1, stamp);

        case 28:
            // 28 FU-A Fragmentation unit
            return mergeFu(rtp, frame, payload_size, stamp, seq);

        default: {
            if (nal < 24) {
                //Single NAL Unit Packets
                return singleFrame(rtp, frame, payload_size, stamp);
            }
            _gop_dropped = true;
            WarnL << "不支持该类型的264 RTP包, nal type:" << nal << ", rtp:\r\n" << rtp->dumpString();
            return false;
        }
    }
}

void H264RtpDecoder::outputFrame(const RtpPacket::Ptr &rtp, const H264Frame::Ptr &frame) {
    if (frame->dropAble()) {
        // 不参与dts生成  [AUTO-TRANSLATED:dff3b747]
        // Not involved in dts generation
        frame->_dts = frame->_pts;
    } else {
        // rtsp没有dts，那么根据pts排序算法生成dts  [AUTO-TRANSLATED:f37c17f3]
        // Rtsp does not have dts, so dts is generated according to the pts sorting algorithm
        _dts_generator.getDts(frame->_pts, frame->_dts);
    }

    if (frame->keyFrame() && _gop_dropped) {
        _gop_dropped = false;
        InfoL << "new gop received, rtp:\r\n" << rtp->dumpString();
    }
    if (!_gop_dropped || frame->configFrame()) {
        RtpCodec::inputFrame(frame);
    }
    _frame = obtainFrame();
}

////////////////////////////////////////////////////////////////////////

void H264RtpEncoder::insertConfigFrame(uint64_t pts){
    if (!_sps || !_pps) {
        return;
    }
    // gop缓存从sps开始，sps、pps后面还有时间戳相同的关键帧，所以mark bit为false  [AUTO-TRANSLATED:e8dcff77]
    // The gop cache starts from sps, sps, pps and then there are key frames with the same timestamp, so the mark bit is false
    packRtp(_sps->data() + _sps->prefixSize(), _sps->size() - _sps->prefixSize(), pts, false, true);
    packRtp(_pps->data() + _pps->prefixSize(), _pps->size() - _pps->prefixSize(), pts, false, false);
}

void H264RtpEncoder::packRtp(const char *ptr, size_t len, uint64_t pts, bool is_mark, bool gop_pos){
    if (len + 3 <= getRtpInfo().getMaxSize()) {
        // 采用STAP-A/Single NAL unit packet per H.264 模式  [AUTO-TRANSLATED:1a719984]
        // Use STAP-A/Single NAL unit packet per H.264 mode
        packRtpSmallFrame(ptr, len, pts, is_mark, gop_pos);
    } else {
        // STAP-A模式打包会大于MTU,所以采用FU-A模式  [AUTO-TRANSLATED:f3923abc]
        // STAP-A mode packaging will be larger than MTU, so FU-A mode is used
        packRtpFu(ptr, len, pts, is_mark, gop_pos);
    }
}

void H264RtpEncoder::packRtpFu(const char *ptr, size_t len, uint64_t pts, bool is_mark, bool gop_pos){
    auto packet_size = getRtpInfo().getMaxSize() - 2;
    if (len <= packet_size + 1) {
        // 小于FU-A打包最小字节长度要求，采用STAP-A/Single NAL unit packet per H.264 模式  [AUTO-TRANSLATED:b83bb4d1]
        // Less than the minimum byte length requirement for FU-A packaging, use STAP-A/Single NAL unit packet per H.264 mode
        packRtpSmallFrame(ptr, len, pts, is_mark, gop_pos);
        return;
    }

    // 末尾5bit为nalu type，固定为28(FU-A)  [AUTO-TRANSLATED:6293f1a9]
    // The last 5 bits are the nalu type, fixed to 28 (FU-A)
    auto fu_char_0 = (ptr[0] & (~0x1F)) | 28;
    auto fu_char_1 = H264_TYPE(ptr[0]);
    FuFlags *fu_flags = (FuFlags *) (&fu_char_1);
    fu_flags->start_bit = 1;

    size_t offset = 1;
    while (!fu_flags->end_bit) {
        if (!fu_flags->start_bit && len <= offset + packet_size) {
            //FU-A end
            packet_size = len - offset;
            fu_flags->end_bit = 1;
        }

        // 传入nullptr先不做payload的内存拷贝  [AUTO-TRANSLATED:1858cf77]
        // Pass in nullptr first, do not copy the payload memory
        auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, packet_size + 2, fu_flags->end_bit && is_mark, pts);
        // rtp payload 负载部分  [AUTO-TRANSLATED:aecf73cc]
        // rtp payload load part
        uint8_t *payload = rtp->getPayload();
        // FU-A 第1个字节  [AUTO-TRANSLATED:b5558495]
        // FU-A first byte
        payload[0] = fu_char_0;
        // FU-A 第2个字节  [AUTO-TRANSLATED:6b4540bb]
        // FU-A second byte
        payload[1] = fu_char_1;
        // H264 数据  [AUTO-TRANSLATED:79204239]
        // H264 data
        memcpy(payload + 2, (uint8_t *) ptr + offset, packet_size);
        // 输入到rtp环形缓存  [AUTO-TRANSLATED:5208ef90]
        // Input to the rtp ring buffer
        RtpCodec::inputRtp(rtp, gop_pos);

        offset += packet_size;
        fu_flags->start_bit = 0;
    }
}

void H264RtpEncoder::packRtpSmallFrame(const char *data, size_t len, uint64_t pts, bool is_mark, bool gop_pos) {
    GET_CONFIG(bool, h264_stap_a, Rtp::kH264StapA);
    if (h264_stap_a) {
        packRtpStapA(data, len, pts, is_mark, gop_pos);
    } else {
        packRtpSingleNalu(data, len, pts, is_mark, gop_pos);
    }
}

void H264RtpEncoder::packRtpStapA(const char *ptr, size_t len, uint64_t pts, bool is_mark, bool gop_pos){
    // 如果帧长度不超过mtu,为了兼容性 webrtc，采用STAP-A模式打包  [AUTO-TRANSLATED:a091199c]
    // If the frame length does not exceed mtu, for compatibility with webrtc, use STAP-A mode packaging
    auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, len + 3, is_mark, pts);
    uint8_t *payload = rtp->getPayload();
    //STAP-A
    payload[0] = (ptr[0] & (~0x1F)) | 24;
    payload[1] = (len >> 8) & 0xFF;
    payload[2] = len & 0xff;
    memcpy(payload + 3, (uint8_t *) ptr, len);

    RtpCodec::inputRtp(rtp, gop_pos);
}

void H264RtpEncoder::packRtpSingleNalu(const char *data, size_t len, uint64_t pts, bool is_mark, bool gop_pos) {
    // Single NAL unit packet per H.264 模式  [AUTO-TRANSLATED:9332a8e4]
    // Single NAL unit packet per H.264 mode
    RtpCodec::inputRtp(getRtpInfo().makeRtp(TrackVideo, data, len, is_mark, pts), gop_pos);
}

bool H264RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = frame->data() + frame->prefixSize();
    switch (H264_TYPE(ptr[0])) {
        case H264Frame::NAL_SPS: {
            _sps = Frame::getCacheAbleFrame(frame);
            return true;
        }
        case H264Frame::NAL_PPS: {
            _pps = Frame::getCacheAbleFrame(frame);
            return true;
        }
        default: break;
    }

    GET_CONFIG(int,lowLatency,Rtp::kLowLatency);
    if (lowLatency) { // 低延迟模式
        if (_last_frame) {
            flush();
        }
        inputFrame_l(frame, true);
    } else {
        if (_last_frame) {
            // 如果时间戳发生了变化，那么markbit才置true  [AUTO-TRANSLATED:19b68429]
            // If the timestamp changes, then the markbit is set to true
            inputFrame_l(_last_frame, _last_frame->pts() != frame->pts());
        }
        _last_frame = Frame::getCacheAbleFrame(frame);
    }
    return true;
}

void H264RtpEncoder::flush() {
    if (_last_frame) {
        // 如果时间戳发生了变化，那么markbit才置true  [AUTO-TRANSLATED:6b1d0fe0]
        // If the timestamp changes, then the markbit is set to true
        inputFrame_l(_last_frame, true);
        _last_frame = nullptr;
    }
}

bool H264RtpEncoder::inputFrame_l(const Frame::Ptr &frame, bool is_mark){
    if (frame->keyFrame()) {
        // 保证每一个关键帧前都有SPS与PPS  [AUTO-TRANSLATED:9d1a9d5e]
        // Ensure that there are SPS and PPS before each key frame
        insertConfigFrame(frame->pts());
    }
    packRtp(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize(), frame->pts(), is_mark, false);
    return true;
}

}//namespace mediakit
