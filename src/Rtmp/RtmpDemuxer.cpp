/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtmpDemuxer.h"
#include "Extension/Factory.h"

using namespace std;

namespace mediakit {

size_t RtmpDemuxer::trackCount(const AMFValue &metadata) {
    size_t ret = 0;
    metadata.object_for_each([&](const string &key, const AMFValue &val) {
        if (key == "videocodecid") {
            //找到视频
            ++ret;
            return;
        }
        if (key == "audiocodecid") {
            //找到音频
            ++ret;
            return;
        }
    });
    return ret;
}

bool RtmpDemuxer::loadMetaData(const AMFValue &val){
    bool ret = false;
    try {
        int audiosamplerate = 0;
        int audiochannels = 0;
        int audiosamplesize = 0;
        int videodatarate = 0;
        int audiodatarate = 0;
        const AMFValue *audiocodecid = nullptr;
        const AMFValue *videocodecid = nullptr;
        val.object_for_each([&](const string &key, const AMFValue &val) {
            if (key == "duration") {
                _duration = (float)val.as_number();
                return;
            }
            if (key == "audiosamplerate") {
                audiosamplerate = val.as_integer();
                return;
            }
            if (key == "audiosamplesize") {
                audiosamplesize = val.as_integer();
                return;
            }
            if (key == "stereo") {
                audiochannels = val.as_boolean() ? 2 : 1;
                return;
            }
            if (key == "videocodecid") {
                //找到视频
                videocodecid = &val;
                return;
            }
            if (key == "audiocodecid") {
                //找到音频
                audiocodecid = &val;
                return;
            }
            if (key == "audiodatarate") {
                audiodatarate = val.as_integer();
                return;
            }
            if (key == "videodatarate") {
                videodatarate = val.as_integer();
                return;
            }
        });
        if (videocodecid) {
            //有视频
            ret = true;
            makeVideoTrack(*videocodecid, videodatarate * 1024);
        }
        if (audiocodecid) {
            //有音频
            ret = true;
            makeAudioTrack(*audiocodecid, audiosamplerate, audiochannels, audiosamplesize, audiodatarate * 1024);
        }
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }

    if (ret) {
        //metadata中存在track相关的描述，那么我们根据metadata判断有多少个track
        addTrackCompleted();
    }
    return ret;
}

float RtmpDemuxer::getDuration() const {
    return _duration;
}

void RtmpDemuxer::inputRtmp(const RtmpPacket::Ptr &pkt) {
    switch (pkt->type_id) {
        case MSG_VIDEO: {
            if (!_try_get_video_track) {
                _try_get_video_track = true;
                auto codec = AMFValue(pkt->getMediaType());
                makeVideoTrack(codec, 0);
            }
            if (_video_rtmp_decoder) {
                _video_rtmp_decoder->inputRtmp(pkt);
            }
            break;
        }

        case MSG_AUDIO: {
            if (!_try_get_audio_track) {
                _try_get_audio_track = true;
                auto codec = AMFValue(pkt->getMediaType());
                makeAudioTrack(codec, pkt->getAudioSampleRate(), pkt->getAudioChannel(), pkt->getAudioSampleBit(), 0);
            }
            if (_audio_rtmp_decoder) {
                _audio_rtmp_decoder->inputRtmp(pkt);
            }
            break;
        }
        default : break;
    }
}

void RtmpDemuxer::makeVideoTrack(const AMFValue &videoCodec, int bit_rate) {
    if (_video_rtmp_decoder) {
        return;
    }
    //生成Track对象
    _video_track = dynamic_pointer_cast<VideoTrack>(Factory::getVideoTrackByAmf(videoCodec));
    if (!_video_track) {
        return;
    }
    //生成rtmpCodec对象以便解码rtmp
    _video_rtmp_decoder = Factory::getRtmpCodecByTrack(_video_track, false);
    if (!_video_rtmp_decoder) {
        //找不到相应的rtmp解码器，该track无效
        _video_track.reset();
        return;
    }
    _video_track->setBitRate(bit_rate);
    //设置rtmp解码器代理，生成的frame写入该Track
    _video_rtmp_decoder->addDelegate(_video_track);
    addTrack(_video_track);
    _try_get_video_track = true;
}

void RtmpDemuxer::makeAudioTrack(const AMFValue &audioCodec,int sample_rate, int channels, int sample_bit, int bit_rate) {
    if (_audio_rtmp_decoder) {
        return;
    }
    //生成Track对象
    _audio_track = dynamic_pointer_cast<AudioTrack>(Factory::getAudioTrackByAmf(audioCodec, sample_rate, channels, sample_bit));
    if (!_audio_track) {
        return;
    }
    //生成rtmpCodec对象以便解码rtmp
    _audio_rtmp_decoder = Factory::getRtmpCodecByTrack(_audio_track, false);
    if (!_audio_rtmp_decoder) {
        //找不到相应的rtmp解码器，该track无效
        _audio_track.reset();
        return;
    }
    _audio_track->setBitRate(bit_rate);
    //设置rtmp解码器代理，生成的frame写入该Track
    _audio_rtmp_decoder->addDelegate(_audio_track);
    addTrack(_audio_track);
    _try_get_audio_track = true;
}

} /* namespace mediakit */