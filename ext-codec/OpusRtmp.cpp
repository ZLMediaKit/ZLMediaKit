/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "OpusRtmp.h"
#include "Rtmp/utils.h"
#include "Common/config.h"
#include "Extension/Factory.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

void OpusRtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt) {
    auto data = pkt->data();
    int size = pkt->size();
    auto flags = (uint8_t)data[0];
    auto codec = (RtmpAudioCodec)(flags >> 4);
    auto type = flags & 0x0F;
    data++; size--;
    if (codec == RtmpAudioCodec::ex_header) {
        // @todo parse enhance audio header and check fourcc
        data += 4;
        size -= 4;
        if (type == (uint8_t)RtmpPacketType::PacketTypeSequenceStart) {
            getTrack()->setExtraData((uint8_t *)data, size);
        } else {
            outputFrame(data, size, pkt->time_stamp, pkt->time_stamp);
        }
    } else {
        if (codec == RtmpAudioCodec::aac) {
            uint8_t pkt_type = *data;
            data++; size--;
            if (pkt_type == (uint8_t)RtmpAACPacketType::aac_config_header) {
                getTrack()->setExtraData((uint8_t *)data, size);
                return;
            }
        }
        outputFrame(data, size, pkt->time_stamp, pkt->time_stamp);
    } 
}

void OpusRtmpDecoder::outputFrame(const char *data, size_t size, uint32_t dts, uint32_t pts) {
    RtmpCodec::inputFrame(Factory::getFrameFromPtr(getTrack()->getCodecId(), data, size, dts, pts));
}

////////////////////////////////////////////////////////////////////////
OpusRtmpEncoder::OpusRtmpEncoder(const Track::Ptr &track) : RtmpCodec(track) {
    _enhanced = mINI::Instance()[Rtmp::kEnhanced];
}

bool OpusRtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto packet = RtmpPacket::create();
    if (_enhanced) {
        uint8_t flags = ((uint8_t)RtmpAudioCodec::ex_header << 4) | (uint8_t)RtmpPacketType::PacketTypeCodedFrames;
        packet->buffer.push_back(flags);
        uint32_t fourcc = htonl(getCodecFourCC(getTrack()->getCodecId()));
        packet->buffer.append(reinterpret_cast<char *>(&fourcc), 4);
    } else {
        uint8_t flags = getAudioRtmpFlags(getTrack());
        packet->buffer.push_back(flags);
        if (getTrack()->getCodecId() == CodecAAC) {
            packet->buffer.push_back((uint8_t)RtmpAACPacketType::aac_raw);
        }
    }
    packet->buffer.append(frame->data(), frame->size());
    packet->body_size = packet->buffer.size();
    packet->time_stamp = frame->dts();
    packet->chunk_id = CHUNK_AUDIO;
    packet->stream_index = STREAM_MEDIA;
    packet->type_id = MSG_AUDIO;
    // Output rtmp packet
    RtmpCodec::inputRtmp(packet);
    return true;
}

void OpusRtmpEncoder::makeConfigPacket() {
    auto extra_data = getTrack()->getExtraData();
    if (!extra_data || !extra_data->size())
        return;
    auto packet = RtmpPacket::create();
    if (_enhanced) {
        uint8_t flags = ((uint8_t)RtmpAudioCodec::ex_header << 4) | (uint8_t)RtmpPacketType::PacketTypeSequenceStart;
        packet->buffer.push_back(flags);
        uint32_t fourcc = htonl(getCodecFourCC(getTrack()->getCodecId()));
        packet->buffer.append(reinterpret_cast<char *>(&fourcc), 4);
    } else {
        uint8_t flags = getAudioRtmpFlags(getTrack());
        packet->buffer.push_back(flags);
        if (getTrack()->getCodecId() == CodecAAC) {
            packet->buffer.push_back((uint8_t)RtmpAACPacketType::aac_config_header);
        }
        else{
            return ;
        }
    }
    packet->buffer.append(extra_data->data(), extra_data->size());
    packet->body_size = packet->buffer.size();
    packet->chunk_id = CHUNK_AUDIO;
    packet->stream_index = STREAM_MEDIA;
    packet->time_stamp = 0;
    packet->type_id = MSG_AUDIO;
    RtmpCodec::inputRtmp(packet);
}

} // namespace mediakit
