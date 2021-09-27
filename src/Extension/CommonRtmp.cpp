/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "CommonRtmp.h"

namespace mediakit{

CommonRtmpDecoder::CommonRtmpDecoder(CodecId codec) {
    _codec = codec;
    obtainFrame();
}

CodecId CommonRtmpDecoder::getCodecId() const {
    return _codec;
}

void CommonRtmpDecoder::obtainFrame() {
    _frame = FrameImp::create();
    _frame->_codec_id = _codec;
}

void CommonRtmpDecoder::inputRtmp(const RtmpPacket::Ptr &rtmp) {
    //拷贝负载
    _frame->_buffer.assign(rtmp->buffer.data() + 1, rtmp->buffer.size() - 1);
    _frame->_dts = rtmp->time_stamp;
    //写入环形缓存
    RtmpCodec::inputFrame(_frame);
    //创建下一帧
    obtainFrame();
}

/////////////////////////////////////////////////////////////////////////////////////

CommonRtmpEncoder::CommonRtmpEncoder(const Track::Ptr &track) : CommonRtmpDecoder(track->getCodecId()) {
    _audio_flv_flags = getAudioRtmpFlags(track);
}

bool CommonRtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    if (!_audio_flv_flags) {
        return false;
    }
    auto rtmp = RtmpPacket::create();
    //header
    rtmp->buffer.push_back(_audio_flv_flags);
    //data
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