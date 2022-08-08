/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Decoder.h"
#include "PSDecoder.h"
#include "TSDecoder.h"
#include "Extension/H264.h"
#include "Extension/H265.h"
#include "Extension/AAC.h"
#include "Extension/G711.h"
#include "Extension/Opus.h"

#if defined(ENABLE_RTPPROXY) || defined(ENABLE_HLS)
#include "mpeg-ts-proto.h"
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
#define SWITCH_CASE(codec_id) case codec_id : return #codec_id
static const char *getCodecName(int codec_id) {
    switch (codec_id) {
        SWITCH_CASE(PSI_STREAM_MPEG1);
        SWITCH_CASE(PSI_STREAM_MPEG2);
        SWITCH_CASE(PSI_STREAM_AUDIO_MPEG1);
        SWITCH_CASE(PSI_STREAM_MP3);
        SWITCH_CASE(PSI_STREAM_AAC);
        SWITCH_CASE(PSI_STREAM_MPEG4);
        SWITCH_CASE(PSI_STREAM_MPEG4_AAC_LATM);
        SWITCH_CASE(PSI_STREAM_H264);
        SWITCH_CASE(PSI_STREAM_MPEG4_AAC);
        SWITCH_CASE(PSI_STREAM_H265);
        SWITCH_CASE(PSI_STREAM_AUDIO_AC3);
        SWITCH_CASE(PSI_STREAM_AUDIO_EAC3);
        SWITCH_CASE(PSI_STREAM_AUDIO_DTS);
        SWITCH_CASE(PSI_STREAM_VIDEO_DIRAC);
        SWITCH_CASE(PSI_STREAM_VIDEO_VC1);
        SWITCH_CASE(PSI_STREAM_VIDEO_SVAC);
        SWITCH_CASE(PSI_STREAM_AUDIO_SVAC);
        SWITCH_CASE(PSI_STREAM_AUDIO_G711A);
        SWITCH_CASE(PSI_STREAM_AUDIO_G711U);
        SWITCH_CASE(PSI_STREAM_AUDIO_G722);
        SWITCH_CASE(PSI_STREAM_AUDIO_G723);
        SWITCH_CASE(PSI_STREAM_AUDIO_G729);
        SWITCH_CASE(PSI_STREAM_AUDIO_OPUS);
        default : return "unknown codec";
    }
}

void DecoderImp::onStream(int stream, int codecid, const void *extra, size_t bytes, int finish){
    switch (codecid) {
        case PSI_STREAM_H264: {
            onTrack(std::make_shared<H264Track>());
            break;
        }

        case PSI_STREAM_H265: {
            onTrack(std::make_shared<H265Track>());
            break;
        }

        case PSI_STREAM_MPEG4_AAC :
        case PSI_STREAM_AAC: {
            onTrack(std::make_shared<AACTrack>());
            break;
        }

        case PSI_STREAM_AUDIO_G711A:
        case PSI_STREAM_AUDIO_G711U: {
            auto codec = codecid == PSI_STREAM_AUDIO_G711A ? CodecG711A : CodecG711U;
            //G711传统只支持 8000/1/16的规格，FFmpeg貌似做了扩展，但是这里不管它了
            onTrack(std::make_shared<G711Track>(codec, 8000, 1, 16));
            break;
        }

        case PSI_STREAM_AUDIO_OPUS: {
            onTrack(std::make_shared<OpusTrack>());
            break;
        }

        default:
            if(codecid != 0){
                WarnL<< "unsupported codec type:" << getCodecName(codecid) << " " << (int)codecid;
            }
            break;
    }

    //防止未获取视频track提前complete导致忽略后续视频的问题，用于兼容一些不太规范的ps流
    if (finish && _tracks[TrackVideo] ) {
        _sink->addTrackCompleted();
        InfoL << "add track finished";
    }
}

void DecoderImp::onDecode(int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,size_t bytes) {
    pts /= 90;
    dts /= 90;

    switch (codecid) {
        case PSI_STREAM_H264: {
            if (!_tracks[TrackVideo]) {
                onTrack(std::make_shared<H264Track>());
            }
            auto frame = std::make_shared<H264FrameNoCacheAble>((char *) data, bytes, (uint64_t)dts, (uint64_t)pts, prefixSize((char *) data, bytes));
            _merger.inputFrame(frame,[this](uint64_t dts, uint64_t pts, const Buffer::Ptr &buffer, bool) {
                onFrame(std::make_shared<FrameWrapper<H264FrameNoCacheAble> >(buffer, dts, pts, prefixSize(buffer->data(), buffer->size()), 0));
            });
            break;
        }

        case PSI_STREAM_H265: {
            if (!_tracks[TrackVideo]) {
                onTrack(std::make_shared<H265Track>());
            }
            auto frame = std::make_shared<H265FrameNoCacheAble>((char *) data, bytes, (uint64_t)dts, (uint64_t)pts, prefixSize((char *) data, bytes));
            _merger.inputFrame(frame,[this](uint64_t dts, uint64_t pts, const Buffer::Ptr &buffer, bool) {
                onFrame(std::make_shared<FrameWrapper<H265FrameNoCacheAble> >(buffer, dts, pts, prefixSize(buffer->data(), buffer->size()), 0));
            });
            break;
        }

        case PSI_STREAM_MPEG4_AAC :
        case PSI_STREAM_AAC: {
            uint8_t *ptr = (uint8_t *)data;
            if(!(bytes > 7 && ptr[0] == 0xFF && (ptr[1] & 0xF0) == 0xF0)){
                //这不是aac
                break;
            }
            if (!_tracks[TrackAudio]) {
                onTrack(std::make_shared<AACTrack>());
            }
            onFrame(std::make_shared<FrameFromPtr>(CodecAAC, (char *) data, bytes, (uint64_t)dts, 0, ADTS_HEADER_LEN));
            break;
        }

        case PSI_STREAM_AUDIO_G711A:
        case PSI_STREAM_AUDIO_G711U: {
            auto codec = codecid  == PSI_STREAM_AUDIO_G711A ? CodecG711A : CodecG711U;
            if (!_tracks[TrackAudio]) {
                //G711传统只支持 8000/1/16的规格，FFmpeg貌似做了扩展，但是这里不管它了
                onTrack(std::make_shared<G711Track>(codec, 8000, 1, 16));
            }
            onFrame(std::make_shared<FrameFromPtr>(codec, (char *) data, bytes, (uint64_t)dts));
            break;
        }

        case PSI_STREAM_AUDIO_OPUS: {
            if (!_tracks[TrackAudio]) {
                onTrack(std::make_shared<OpusTrack>());
            }
            onFrame(std::make_shared<FrameFromPtr>(CodecOpus, (char *) data, bytes, (uint64_t)dts));
            break;
        }

        default:
            // 海康的 PS 流中会有 codecid 为 0xBD 的包
            if (codecid != 0 && codecid != 0xBD) {
                if (_last_unsported_print.elapsedTime() / 1000 > 5) {
                    _last_unsported_print.resetTime();
                    WarnL << "unsupported codec type:" << getCodecName(codecid) << " " << (int) codecid;
                }
            }
            break;
    }
}
#else
void DecoderImp::onDecode(int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,size_t bytes) {}
void DecoderImp::onStream(int stream,int codecid,const void *extra,size_t bytes,int finish) {}
#endif

void DecoderImp::onTrack(const Track::Ptr &track) {
    if (!_tracks[track->getTrackType()]) {
        _tracks[track->getTrackType()] = track;
        _sink->addTrack(track);
        InfoL << "got track: " << track->getCodecName();
    }
}

void DecoderImp::onFrame(const Frame::Ptr &frame) {
    _sink->inputFrame(frame);
}

}//namespace mediakit

