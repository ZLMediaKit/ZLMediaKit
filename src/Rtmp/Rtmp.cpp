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
        _metadata.set("audiochannels", audio->getAudioChannel());
        _metadata.set("stereo", audio->getAudioChannel() > 1);
    }
    _codecId = audio->getCodecId();
    _metadata.set("audiocodecid", Factory::getAmfByCodecId(_codecId));
}

}//namespace mediakit