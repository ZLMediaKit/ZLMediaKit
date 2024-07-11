/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AACRtmp.h"
#include "Rtmp/Rtmp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

void AACRtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt) {
    CHECK_RET(pkt->size() > 2);
    if (pkt->isConfigFrame()) {
        getTrack()->setExtraData((uint8_t *)pkt->data() + 2, pkt->size() - 2);
        return;
    }
    RtmpCodec::inputFrame(std::make_shared<FrameFromPtr>(CodecAAC, pkt->buffer.data() + 2, pkt->buffer.size() - 2, pkt->time_stamp));
}

/////////////////////////////////////////////////////////////////////////////////////

bool AACRtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto pkt = RtmpPacket::create();
    // header
    pkt->buffer.push_back(_audio_flv_flags);
    pkt->buffer.push_back((uint8_t)RtmpAACPacketType::aac_raw);
    // aac data
    pkt->buffer.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
    pkt->body_size = pkt->buffer.size();
    pkt->chunk_id = CHUNK_AUDIO;
    pkt->stream_index = STREAM_MEDIA;
    pkt->time_stamp = frame->dts();
    pkt->type_id = MSG_AUDIO;
    RtmpCodec::inputRtmp(pkt);
    return true;
}

void AACRtmpEncoder::makeConfigPacket() {
    _audio_flv_flags = getAudioRtmpFlags(getTrack());
    auto pkt = RtmpPacket::create();
    // header
    pkt->buffer.push_back(_audio_flv_flags);
    pkt->buffer.push_back((uint8_t)RtmpAACPacketType::aac_config_header);

    // aac config
    auto extra_data = getTrack()->getExtraData();
    CHECK(extra_data);
    pkt->buffer.append(extra_data->data(), extra_data->size());

    pkt->body_size = pkt->buffer.size();
    pkt->chunk_id = CHUNK_AUDIO;
    pkt->stream_index = STREAM_MEDIA;
    pkt->time_stamp = 0;
    pkt->type_id = MSG_AUDIO;
    RtmpCodec::inputRtmp(pkt);
}

}//namespace mediakit