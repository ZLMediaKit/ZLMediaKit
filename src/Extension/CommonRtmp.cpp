/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "CommonRtmp.h"

namespace mediakit {

void CommonRtmpDecoder::inputRtmp(const RtmpPacket::Ptr &rtmp) {
    auto frame = FrameImp::create();
    frame->_codec_id = getTrack()->getCodecId();
    frame->_buffer.assign(rtmp->buffer.data() + 1, rtmp->buffer.size() - 1);
    frame->_dts = rtmp->time_stamp;
    RtmpCodec::inputFrame(frame);
}

/////////////////////////////////////////////////////////////////////////////////////

bool CommonRtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    if (!_audio_flv_flags) {
        _audio_flv_flags = getAudioRtmpFlags(getTrack());
    }
    auto rtmp = RtmpPacket::create();
    // header
    rtmp->buffer.push_back(_audio_flv_flags);
    // data
    rtmp->buffer.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
    rtmp->body_size = rtmp->buffer.size();
    rtmp->chunk_id = CHUNK_AUDIO;
    rtmp->stream_index = STREAM_MEDIA;
    rtmp->time_stamp = frame->dts();
    rtmp->type_id = MSG_AUDIO;
    RtmpCodec::inputRtmp(rtmp);
    return true;
}

}//namespace mediakit