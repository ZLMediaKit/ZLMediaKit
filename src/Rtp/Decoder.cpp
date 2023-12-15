/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Decoder.h"
#include "PSDecoder.h"
#include "TSDecoder.h"
#include "Extension/Factory.h"

#if defined(ENABLE_RTPPROXY) || defined(ENABLE_HLS)
#include "mpeg-ts.h"
#endif

using namespace toolkit;

namespace mediakit {

void Decoder::setOnDecode(Decoder::onDecode cb) {
    _on_decode = std::move(cb);
}

void Decoder::setOnStream(Decoder::onStream cb) {
    _on_stream = std::move(cb);
}
    
static Decoder::Ptr createDecoder_l(DecoderImp::Type type) {
    switch (type){
        case DecoderImp::decoder_ps:
#ifdef ENABLE_RTPPROXY
            return std::make_shared<PSDecoder>();
#else
            WarnL << "创建ps解复用器失败，请打开ENABLE_RTPPROXY然后重新编译";
            return nullptr;
#endif//ENABLE_RTPPROXY

        case DecoderImp::decoder_ts:
#ifdef ENABLE_HLS
            return std::make_shared<TSDecoder>();
#else
            WarnL << "创建mpegts解复用器失败，请打开ENABLE_HLS然后重新编译";
            return nullptr;
#endif//ENABLE_HLS

        default: return nullptr;
    }
}

/////////////////////////////////////////////////////////////

DecoderImp::Ptr DecoderImp::createDecoder(Type type, MediaSinkInterface *sink){
    auto decoder =  createDecoder_l(type);
    if(!decoder){
        return nullptr;
    }
    return DecoderImp::Ptr(new DecoderImp(decoder, sink));
}

void DecoderImp::flush() {
    for (auto &pr : _tracks) {
        pr.second.second.flush();
    }
}

ssize_t DecoderImp::input(const uint8_t *data, size_t bytes){
    return _decoder->input(data, bytes);
}

DecoderImp::DecoderImp(const Decoder::Ptr &decoder, MediaSinkInterface *sink){
    _decoder = decoder;
    _sink = sink;
    _decoder->setOnDecode([this](int stream, int codecid, int flags, int64_t pts, int64_t dts, const void *data, size_t bytes) {
        onDecode(stream, codecid, flags, pts, dts, data, bytes);
    });
    _decoder->setOnStream([this](int stream, int codecid, const void *extra, size_t bytes, int finish) {
        onStream(stream, codecid, extra, bytes, finish);
    });
}

#if defined(ENABLE_RTPPROXY) || defined(ENABLE_HLS)

void DecoderImp::onStream(int stream, int codecid, const void *extra, size_t bytes, int finish) {
    // G711传统只支持 8000/1/16的规格，FFmpeg貌似做了扩展，但是这里不管它了
    auto track = Factory::getTrackByCodecId(getCodecByMpegId(codecid), 8000, 1, 16);
    if (!track) {
        return;
    }
    onTrack(stream, std::move(track));
    // 防止未获取视频track提前complete导致忽略后续视频的问题，用于兼容一些不太规范的ps流
    if (finish && _have_video) {
        _sink->addTrackCompleted();
        InfoL << "Add track finished";
    }
}

void DecoderImp::onDecode(int stream, int codecid, int flags, int64_t pts, int64_t dts, const void *data, size_t bytes) {
    pts /= 90;
    dts /= 90;

    auto codec = getCodecByMpegId(codecid);
    if (codec == CodecInvalid) {
        return;
    }
    auto &ref = _tracks[stream];
    if (!ref.first) {
        onTrack(stream, Factory::getTrackByCodecId(codec, 8000, 1, 16));
    }
    if (!ref.first) {
        WarnL << "Unsupported codec :" << getCodecName(codec);
        return;
    }
    auto frame = Factory::getFrameFromPtr(codec, (char *)data, bytes, dts, pts);
    if (getTrackType(codec) != TrackVideo) {
        onFrame(stream, frame);
        return;
    }
    ref.second.inputFrame(frame, [this, stream, codec](uint64_t dts, uint64_t pts, const Buffer::Ptr &buffer, bool) {
        onFrame(stream, Factory::getFrameFromBuffer(codec, buffer, dts, pts));
    });
}
#else
void DecoderImp::onDecode(int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,size_t bytes) {}
void DecoderImp::onStream(int stream,int codecid,const void *extra,size_t bytes,int finish) {}
#endif

void DecoderImp::onTrack(int index, const Track::Ptr &track) {
    if (!track) {
        return;
    }
    track->setIndex(index);
    auto &ref = _tracks[index];
    if (ref.first) {
        WarnL << "Already existed a same track: " << index << ", codec: " << track->getCodecName();
        return;
    }
    ref.first = track;
    _sink->addTrack(track);
    InfoL << "Got track: " << track->getCodecName();
    _have_video = track->getTrackType() == TrackVideo ? true : _have_video;
}

void DecoderImp::onFrame(int index, const Frame::Ptr &frame) {
    if (frame) {
        frame->setIndex(index);
        _sink->inputFrame(frame);
    }
}

}//namespace mediakit

