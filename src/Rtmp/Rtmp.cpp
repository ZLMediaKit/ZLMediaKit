/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Rtmp.h"
#include "Extension/Factory.h"
namespace mediakit{

VideoMeta::VideoMeta(const VideoTrack::Ptr &video,int datarate ){
    if(video->getVideoWidth() > 0 ){
        _metadata.set("width", video->getVideoWidth());
    }
    if(video->getVideoHeight() > 0 ){
        _metadata.set("height", video->getVideoHeight());
    }
    if(video->getVideoFps() > 0 ){
        _metadata.set("framerate", video->getVideoFps());
    }
    _metadata.set("videodatarate", datarate);
    _codecId = video->getCodecId();
    _metadata.set("videocodecid", Factory::getAmfByCodecId(_codecId));
}

AudioMeta::AudioMeta(const AudioTrack::Ptr &audio,int datarate){
    _metadata.set("audiodatarate", datarate);
    if(audio->getAudioSampleRate() > 0){
        _metadata.set("audiosamplerate", audio->getAudioSampleRate());
    }
    if(audio->getAudioSampleBit() > 0){
        _metadata.set("audiosamplesize", audio->getAudioSampleBit());
    }
    if(audio->getAudioChannel() > 0){
        _metadata.set("stereo", audio->getAudioChannel() > 1);
    }
    _codecId = audio->getCodecId();
    _metadata.set("audiocodecid", Factory::getAmfByCodecId(_codecId));
}

uint8_t getAudioRtmpFlags(const Track::Ptr &track){
    switch (track->getTrackType()){
        case TrackAudio : {
            auto audioTrack = dynamic_pointer_cast<AudioTrack>(track);
            if (!audioTrack) {
                WarnL << "获取AudioTrack失败";
                return 0;
            }
            auto iSampleRate = audioTrack->getAudioSampleRate();
            auto iChannel = audioTrack->getAudioChannel();
            auto iSampleBit = audioTrack->getAudioSampleBit();

            uint8_t flvAudioType ;
            switch (track->getCodecId()){
                case CodecG711A : flvAudioType = FLV_CODEC_G711A; break;
                case CodecG711U : flvAudioType = FLV_CODEC_G711U; break;
                case CodecAAC : {
                    flvAudioType = FLV_CODEC_AAC;
                    //aac不通过flags获取音频相关信息
                    iSampleRate = 44100;
                    iSampleBit = 16;
                    iChannel = 2;
                    break;
                }
                default: WarnL << "该编码格式不支持转换为RTMP: " << track->getCodecName(); return 0;
            }

            uint8_t flvSampleRate;
            switch (iSampleRate) {
                case 44100:
                    flvSampleRate = 3;
                    break;
                case 22050:
                    flvSampleRate = 2;
                    break;
                case 11025:
                    flvSampleRate = 1;
                    break;
                case 16000: // nellymoser only
                case 8000: // nellymoser only
                case 5512: // not MP3
                    flvSampleRate = 0;
                    break;
                default:
                    WarnL << "FLV does not support sample rate " << iSampleRate << " ,choose from (44100, 22050, 11025)";
                    return 0;
            }

            uint8_t flvStereoOrMono = (iChannel > 1);
            uint8_t flvSampleBit = iSampleBit == 16;
            return (flvAudioType << 4) | (flvSampleRate << 2) | (flvSampleBit << 1) | flvStereoOrMono;
        }

        default : return 0;
    }
}


void Metadata::addTrack(AMFValue &metadata, const Track::Ptr &track) {
    Metadata::Ptr new_metadata;
    switch (track->getTrackType()) {
        case TrackVideo: {
            new_metadata = std::make_shared<VideoMeta>(dynamic_pointer_cast<VideoTrack>(track));
        }
            break;
        case TrackAudio: {
            new_metadata = std::make_shared<AudioMeta>(dynamic_pointer_cast<AudioTrack>(track));
        }
            break;
        default:
            return;
    }

    new_metadata->getMetadata().object_for_each([&](const std::string &key, const AMFValue &value) {
        metadata.set(key, value);
    });
}
}//namespace mediakit