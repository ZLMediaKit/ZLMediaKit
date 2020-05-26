/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Decoder.h"
#include "PSDecoder.h"
#include "TSDecoder.h"
#include "mpeg-ts-proto.h"
#include "Extension/H264.h"
#include "Extension/H265.h"
#include "Extension/AAC.h"
#include "Extension/G711.h"

namespace mediakit {
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

int DecoderImp::input(const uint8_t *data, int bytes){
    return _decoder->input(data, bytes);
}

DecoderImp::DecoderImp(const Decoder::Ptr &decoder, MediaSinkInterface *sink){
    _decoder = decoder;
    _sink = sink;
    _decoder->setOnDecode([this](int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,int bytes){
        onDecode(stream,codecid,flags,pts,dts,data,bytes);
    });
}

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
        default : return "unknown codec";
    }
}

void FrameMerger::inputFrame(const Frame::Ptr &frame,const function<void(uint32_t dts,uint32_t pts,const Buffer::Ptr &buffer)> &cb){
    if (!_frameCached.empty() && _frameCached.back()->dts() != frame->dts()) {
        Frame::Ptr back = _frameCached.back();
        Buffer::Ptr merged_frame = back;
        if(_frameCached.size() != 1){
            string merged;
            _frameCached.for_each([&](const Frame::Ptr &frame){
                merged.append(frame->data(),frame->size());
            });
            merged_frame = std::make_shared<BufferString>(std::move(merged));
        }
        cb(back->dts(),back->pts(),merged_frame);
        _frameCached.clear();
    }
    _frameCached.emplace_back(Frame::getCacheAbleFrame(frame));
}

void DecoderImp::onDecode(int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,int bytes) {
    pts /= 90;
    dts /= 90;

    switch (codecid) {
        case PSI_STREAM_H264: {
            if (!_codecid_video) {
                //获取到视频
                _codecid_video = codecid;
                InfoL<< "got video track: H264";
                auto track = std::make_shared<H264Track>();
                onTrack(track);
            }

            if (codecid != _codecid_video) {
                WarnL<< "video track change to H264 from codecid:" << getCodecName(_codecid_video);
                return;
            }

            auto frame = std::make_shared<H264FrameNoCacheAble>((char *) data, bytes, dts, pts,0);
            _merger.inputFrame(frame,[this](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer) {
                onFrame(std::make_shared<H264FrameNoCacheAble>(buffer->data(), buffer->size(), dts, pts, prefixSize(buffer->data(), buffer->size())));
            });
            break;
        }

        case PSI_STREAM_H265: {
            if (!_codecid_video) {
                //获取到视频
                _codecid_video = codecid;
                InfoL<< "got video track: H265";
                auto track = std::make_shared<H265Track>();
                onTrack(track);
            }
            if (codecid != _codecid_video) {
                WarnL<< "video track change to H265 from codecid:" << getCodecName(_codecid_video);
                return;
            }
            auto frame = std::make_shared<H265FrameNoCacheAble>((char *) data, bytes, dts, pts, 0);
            _merger.inputFrame(frame,[this](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer) {
                onFrame(std::make_shared<H265FrameNoCacheAble>(buffer->data(), buffer->size(), dts, pts, prefixSize(buffer->data(), buffer->size())));
            });
            break;
        }

        case PSI_STREAM_AAC: {
            if (!_codecid_audio) {
                //获取到音频
                _codecid_audio = codecid;
                InfoL<< "got audio track: AAC";
                auto track = std::make_shared<AACTrack>();
                onTrack(track);
            }

            if (codecid != _codecid_audio) {
                WarnL<< "audio track change to AAC from codecid:" << getCodecName(_codecid_audio);
                return;
            }
            onFrame(std::make_shared<AACFrameNoCacheAble>((char *) data, bytes, dts, 0, 7));
            break;
        }

        case PSI_STREAM_AUDIO_G711A:
        case PSI_STREAM_AUDIO_G711U: {
            auto codec = codecid  == PSI_STREAM_AUDIO_G711A ? CodecG711A : CodecG711U;
            if (!_codecid_audio) {
                //获取到音频
                _codecid_audio = codecid;
                InfoL<< "got audio track: G711";
                //G711传统只支持 8000/1/16的规格，FFmpeg貌似做了扩展，但是这里不管它了
                auto track = std::make_shared<G711Track>(codec, 8000, 1, 16);
                onTrack(track);
            }

            if (codecid != _codecid_audio) {
                WarnL<< "audio track change to G711 from codecid:" << getCodecName(_codecid_audio);
                return;
            }
            auto frame = std::make_shared<G711FrameNoCacheAble>((char *) data, bytes, dts);
            frame->setCodec(codec);
            onFrame(frame);
            break;
        }
        default:
            if(codecid != 0){
                WarnL<< "unsupported codec type:" << getCodecName(codecid) << " " << (int)codecid;
            }
            break;
    }
}

void DecoderImp::onTrack(const Track::Ptr &track) {
    _sink->addTrack(track);
}

void DecoderImp::onFrame(const Frame::Ptr &frame) {
    _sink->inputFrame(frame);
}

}//namespace mediakit
