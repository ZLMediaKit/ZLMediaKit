/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Rtmp/utils.h"
#include "H264Rtmp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

void H264RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt) {
    if (pkt->isConfigFrame()) {
        CHECK_RET(pkt->size() > 5);
        getTrack()->setExtraData((uint8_t *)pkt->data() + 5, pkt->size() - 5);
        return;
    }

    CHECK_RET(pkt->size() > 9);
    uint8_t *cts_ptr = (uint8_t *)(pkt->buffer.data() + 2);
    int32_t cts = (((cts_ptr[0] << 16) | (cts_ptr[1] << 8) | (cts_ptr[2])) + 0xff800000) ^ 0xff800000;
    auto pts = pkt->time_stamp + cts;
    splitFrame((uint8_t *)pkt->data() + 5, pkt->size() - 5, pkt->time_stamp, pts);
}

void H264RtmpDecoder::splitFrame(const uint8_t *data, size_t size, uint32_t dts, uint32_t pts) {
    auto end = data + size;
    while (data + 4 < end) {
        uint32_t frame_len = load_be32(data);
        data += 4;
        if (data + frame_len > end) {
            break;
        }
        outputFrame((const char *)data, frame_len, dts, pts);
        data += frame_len;
    }
}

void H264RtmpDecoder::outputFrame(const char *data, size_t len, uint32_t dts, uint32_t pts) {
    auto frame = FrameImp::create<H264Frame>();
    frame->_prefix_size = 4;
    frame->_dts = dts;
    frame->_pts = pts;
    frame->_buffer.assign("\x00\x00\x00\x01", 4); // 添加264头
    frame->_buffer.append(data, len);
    RtmpCodec::inputFrame(frame);
}

////////////////////////////////////////////////////////////////////////

void H264RtmpEncoder::flush() {
    inputFrame(nullptr);
}

bool H264RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    if (!_rtmp_packet) {
        _rtmp_packet = RtmpPacket::create();
        //flags/not config/cts预占位
        _rtmp_packet->buffer.resize(5);
    }

    return _merger.inputFrame(frame, [this](uint64_t dts, uint64_t pts, const Buffer::Ptr &, bool have_key_frame) {
            // flags
            _rtmp_packet->buffer[0] = (uint8_t)RtmpVideoCodec::h264 | ((uint8_t)(have_key_frame ? RtmpFrameType::key_frame : RtmpFrameType::inter_frame) << 4);
            _rtmp_packet->buffer[1] = (uint8_t)RtmpH264PacketType::h264_nalu;
            int32_t cts = pts - dts;
            // cts
            set_be24(&_rtmp_packet->buffer[2], cts);
            _rtmp_packet->time_stamp = dts;
            _rtmp_packet->body_size = _rtmp_packet->buffer.size();
            _rtmp_packet->chunk_id = CHUNK_VIDEO;
            _rtmp_packet->stream_index = STREAM_MEDIA;
            _rtmp_packet->type_id = MSG_VIDEO;
            // 输出rtmp packet
            RtmpCodec::inputRtmp(_rtmp_packet);
            _rtmp_packet = nullptr;
        }, &_rtmp_packet->buffer);
}

void H264RtmpEncoder::makeConfigPacket() {
    auto flags = (uint8_t)RtmpVideoCodec::h264;
    flags |= ((uint8_t)RtmpFrameType::key_frame << 4);
    auto pkt = RtmpPacket::create();
    // header
    pkt->buffer.push_back(flags);
    pkt->buffer.push_back((uint8_t)RtmpH264PacketType::h264_config_header);
    // cts
    pkt->buffer.append("\x0\x0\x0", 3);
    // AVCDecoderConfigurationRecord start
    auto extra_data = getTrack()->getExtraData();
    CHECK(extra_data);
    pkt->buffer.append(extra_data->data(), extra_data->size());

    pkt->body_size = pkt->buffer.size();
    pkt->chunk_id = CHUNK_VIDEO;
    pkt->stream_index = STREAM_MEDIA;
    pkt->time_stamp = 0;
    pkt->type_id = MSG_VIDEO;
    RtmpCodec::inputRtmp(pkt);
}

}//namespace mediakit
