/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "VpxRtmp.h"
#include "Rtmp/utils.h"
#include "Common/config.h"
#include "Extension/Factory.h"
using namespace std;
using namespace toolkit;

namespace mediakit {

void VpxRtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt) {
    if (_info.codec == CodecInvalid) {
        // First, determine if it is an enhanced rtmp
        parseVideoRtmpPacket((uint8_t *)pkt->data(), pkt->size(), &_info);
    }

    if (_info.is_enhanced) {
        // Enhanced rtmp
        parseVideoRtmpPacket((uint8_t *)pkt->data(), pkt->size(), &_info);
        if (!_info.is_enhanced || _info.codec != getTrack()->getCodecId()) {
            throw std::invalid_argument("Invalid enhanced-rtmp packet!");
        }

        auto data = (uint8_t *)pkt->data() + RtmpPacketInfo::kEnhancedRtmpHeaderSize;
        auto size = pkt->size() - RtmpPacketInfo::kEnhancedRtmpHeaderSize;
        switch (_info.video.pkt_type) {
            case RtmpPacketType::PacketTypeSequenceStart: {
                getTrack()->setExtraData(data, size);
                break;
            }

            case RtmpPacketType::PacketTypeCodedFramesX:
            case RtmpPacketType::PacketTypeCodedFrames: {
                auto pts = pkt->time_stamp;
                if (RtmpPacketType::PacketTypeCodedFrames == _info.video.pkt_type) {
                    CHECK_RET(size > 3);
                    // SI24 = [CompositionTime Offset]
                    int32_t cts = (load_be24(data) + 0xff800000) ^ 0xff800000;
                    pts += cts;
                    data += 3;
                    size -= 3;
                }
                outputFrame((char*)data, size, pkt->time_stamp, pts);
                break;
            }
            default: 
                WarnL << "Unknown pkt_type: " << (int)_info.video.pkt_type; 
                break;
        }
    } else {
        CHECK_RET(pkt->size() > 5);
        uint8_t *cts_ptr = (uint8_t *)(pkt->buffer.data() + 2);
        int32_t cts = (load_be24(cts_ptr) + 0xff800000) ^ 0xff800000;
        // 国内扩展(12) Vpx rtmp
        if (pkt->isConfigFrame()) {
            getTrack()->setExtraData((uint8_t *)pkt->data() + 5, pkt->size() - 5);
        } else {
            outputFrame(pkt->data() + 5, pkt->size() - 5, pkt->time_stamp, pkt->time_stamp + cts);
        }
    }
}

void VpxRtmpDecoder::outputFrame(const char *data, size_t size, uint32_t dts, uint32_t pts) {
    RtmpCodec::inputFrame(Factory::getFrameFromPtr(getTrack()->getCodecId(), data, size, dts, pts));
}

////////////////////////////////////////////////////////////////////////
VpxRtmpEncoder::VpxRtmpEncoder(const Track::Ptr &track) : RtmpCodec(track) {
    _enhanced = mINI::Instance()[Rtmp::kEnhanced];
}

bool VpxRtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto packet = RtmpPacket::create();
    packet->buffer.resize(8 + frame->size());
    char *buff = packet->data();
    int32_t cts = frame->pts() - frame->dts();
    if (_enhanced) {
        auto header = (RtmpVideoHeaderEnhanced *)buff;
        header->enhanced = 1;
        header->frame_type = frame->keyFrame() ? (int)RtmpFrameType::key_frame : (int)RtmpFrameType::inter_frame;
        header->fourcc = htonl(getCodecFourCC(frame->getCodecId()));
        buff += RtmpPacketInfo::kEnhancedRtmpHeaderSize;
        if (cts) {
            header->pkt_type = (uint8_t)RtmpPacketType::PacketTypeCodedFrames;
            set_be24(buff, cts);
            buff += 3;
        } else {
            header->pkt_type = (uint8_t)RtmpPacketType::PacketTypeCodedFramesX;
        }
    } else {
        // flags
        uint8_t flags = getCodecFlags(frame->getCodecId());
        flags |= (uint8_t)(frame->keyFrame() ? RtmpFrameType::key_frame : RtmpFrameType::inter_frame) << 4;

        buff[0] = flags;
        buff[1] = (uint8_t)RtmpH264PacketType::h264_nalu;
        // cts
        set_be24(&buff[2], cts);
        buff += 5;
    }

    packet->time_stamp = frame->dts();
    memcpy(buff, frame->data(), frame->size());
    buff += frame->size();
    packet->body_size = buff - packet->data();
    packet->chunk_id = CHUNK_VIDEO;
    packet->stream_index = STREAM_MEDIA;
    packet->type_id = MSG_VIDEO;
    // Output rtmp packet
    RtmpCodec::inputRtmp(packet);
    return true;
}

void VpxRtmpEncoder::makeConfigPacket() {
    auto extra_data = getTrack()->getExtraData();
    if (!extra_data || !extra_data->size())
        return;
    auto pkt = RtmpPacket::create();
    pkt->body_size = 5 + extra_data->size();
    pkt->buffer.resize(pkt->body_size);
    auto buff = pkt->buffer.data();
    if (_enhanced) {
        auto header = (RtmpVideoHeaderEnhanced *)buff;
        header->enhanced = 1;
        header->pkt_type = (int)RtmpPacketType::PacketTypeSequenceStart;
        header->frame_type = (int)RtmpFrameType::key_frame;
        header->fourcc = htonl(getCodecFourCC(getTrack()->getCodecId()));
    } else {
        uint8_t flags = getCodecFlags(getTrack()->getCodecId());
        flags |= ((uint8_t)RtmpFrameType::key_frame << 4);
        buff[0] = flags;
        buff[1] = (uint8_t)RtmpH264PacketType::h264_config_header;
        // cts
        memset(buff + 2, 0, 3);
    }
    memcpy(buff+5, extra_data->data(), extra_data->size());
    pkt->chunk_id = CHUNK_VIDEO;
    pkt->stream_index = STREAM_MEDIA;
    pkt->time_stamp = 0;
    pkt->type_id = MSG_VIDEO;
    RtmpCodec::inputRtmp(pkt);
}

} // namespace mediakit
