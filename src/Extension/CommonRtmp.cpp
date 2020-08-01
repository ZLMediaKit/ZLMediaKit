/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    _frame = ResourcePoolHelper<FrameImp>::obtainObj();
    _frame->_buffer.clear();
    _frame->_codec_id = _codec;
    _frame->_prefix_size = 0;
}

bool CommonRtmpDecoder::inputRtmp(const RtmpPacket::Ptr &rtmp, bool) {
    //拷贝负载
    _frame->_buffer.assign(rtmp->strBuf.data() + 1, rtmp->strBuf.size() - 1);
    _frame->_dts = rtmp->timeStamp;
    //写入环形缓存
    RtmpCodec::inputFrame(_frame);
    //创建下一帧
    obtainFrame();
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////

CommonRtmpEncoder::CommonRtmpEncoder(const Track::Ptr &track) : CommonRtmpDecoder(track->getCodecId()) {
    _audio_flv_flags = getAudioRtmpFlags(track);
}

void CommonRtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    if (!_audio_flv_flags) {
        return;
    }
    RtmpPacket::Ptr rtmp = ResourcePoolHelper<RtmpPacket>::obtainObj();
    rtmp->strBuf.clear();
    //header
    rtmp->strBuf.push_back(_audio_flv_flags);
    //data
    rtmp->strBuf.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
    rtmp->bodySize = rtmp->strBuf.size();
    rtmp->chunkId = CHUNK_AUDIO;
    rtmp->streamId = STREAM_MEDIA;
    rtmp->timeStamp = frame->dts();
    rtmp->typeId = MSG_AUDIO;
    RtmpCodec::inputRtmp(rtmp, false);
}

}//namespace mediakit