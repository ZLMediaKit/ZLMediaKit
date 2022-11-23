/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Rtmp.h"
#include "Extension/Factory.h"
namespace mediakit{

TitleMeta::TitleMeta(float dur_sec, size_t fileSize, const std::map<std::string, std::string> &header)
{
    _metadata.set("duration", dur_sec);
    _metadata.set("fileSize", (int)fileSize);
    _metadata.set("server", kServerName);
    for (auto &pr : header) {
        _metadata.set(pr.first, pr.second);
    }
}

VideoMeta::VideoMeta(const VideoTrack::Ptr &video){
    if(video->getVideoWidth() > 0 ){
        _metadata.set("width", video->getVideoWidth());
    }
    if(video->getVideoHeight() > 0 ){
        _metadata.set("height", video->getVideoHeight());
    }
    if(video->getVideoFps() > 0 ){
        _metadata.set("framerate", video->getVideoFps());
    }
    if (video->getBitRate()) {
        _metadata.set("videodatarate", video->getBitRate() / 1024);
    }
    _codecId = video->getCodecId();
    _metadata.set("videocodecid", Factory::getAmfByCodecId(_codecId));
}

AudioMeta::AudioMeta(const AudioTrack::Ptr &audio){
    if (audio->getBitRate()) {
        _metadata.set("audiodatarate", audio->getBitRate() / 1024);
    }
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
            auto audioTrack = std::dynamic_pointer_cast<AudioTrack>(track);
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
                case CodecOpus : {
                    flvAudioType = FLV_CODEC_OPUS;
                    //opus不通过flags获取音频相关信息
                    iSampleRate = 44100;
                    iSampleBit = 16;
                    iChannel = 2;
                    break;
                }
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
            new_metadata = std::make_shared<VideoMeta>(std::dynamic_pointer_cast<VideoTrack>(track));
        }
            break;
        case TrackAudio: {
            new_metadata = std::make_shared<AudioMeta>(std::dynamic_pointer_cast<AudioTrack>(track));
        }
            break;
        default:
            return;
    }

    new_metadata->getMetadata().object_for_each([&](const std::string &key, const AMFValue &value) {
        metadata.set(key, value);
    });
}

RtmpPacket::Ptr RtmpPacket::create(){
#if 0
    static ResourcePool<RtmpPacket> packet_pool;
    static onceToken token([]() {
        packet_pool.setSize(1024);
    });
    auto ret = packet_pool.obtain2();
    ret->clear();
    return ret;
#else
    return Ptr(new RtmpPacket);
#endif
}

void RtmpPacket::clear()
{
    is_abs_stamp = false;
    time_stamp = 0;
    ts_field = 0;
    body_size = 0;
    buffer.clear();
}

bool RtmpPacket::isVideoKeyFrame() const
{
    return type_id == MSG_VIDEO && (uint8_t)buffer[0] >> 4 == FLV_KEY_FRAME && (uint8_t)buffer[1] == 1;
}

bool RtmpPacket::isCfgFrame() const
{
    switch (type_id) {
    case MSG_VIDEO: return buffer[1] == 0;
    case MSG_AUDIO: {
        switch (getMediaType()) {
        case FLV_CODEC_AAC: return buffer[1] == 0;
        default: return false;
        }
    }
    default: return false;
    }
}

int RtmpPacket::getMediaType() const
{
    switch (type_id) {
    case MSG_VIDEO: return (uint8_t)buffer[0] & 0x0F;
    case MSG_AUDIO: return (uint8_t)buffer[0] >> 4;
    default: return 0;
    }
}

int RtmpPacket::getAudioSampleRate() const
{
    if (type_id != MSG_AUDIO) {
        return 0;
    }
    int flvSampleRate = ((uint8_t)buffer[0] & 0x0C) >> 2;
    const static int sampleRate[] = { 5512, 11025, 22050, 44100 };
    return sampleRate[flvSampleRate];
}

int RtmpPacket::getAudioSampleBit() const
{
    if (type_id != MSG_AUDIO) {
        return 0;
    }
    int flvSampleBit = ((uint8_t)buffer[0] & 0x02) >> 1;
    const static int sampleBit[] = { 8, 16 };
    return sampleBit[flvSampleBit];
}

int RtmpPacket::getAudioChannel() const
{
    if (type_id != MSG_AUDIO) {
        return 0;
    }
    int flvStereoOrMono = (uint8_t)buffer[0] & 0x01;
    const static int channel[] = { 1, 2 };
    return channel[flvStereoOrMono];
}

RtmpPacket & RtmpPacket::operator=(const RtmpPacket &that)
{
    is_abs_stamp = that.is_abs_stamp;
    stream_index = that.stream_index;
    body_size = that.body_size;
    type_id = that.type_id;
    ts_field = that.ts_field;
    time_stamp = that.time_stamp;
    return *this;
}

RtmpHandshake::RtmpHandshake(uint32_t _time, uint8_t *_random /*= nullptr*/)
{
    _time = htonl(_time);
    memcpy(time_stamp, &_time, 4);
    if (!_random) {
        random_generate((char *)random, sizeof(random));
    }
    else {
        memcpy(random, _random, sizeof(random));
    }
}

void RtmpHandshake::random_generate(char *bytes, int size)
{
    static char cdata[] = { 0x73, 0x69, 0x6d, 0x70, 0x6c, 0x65, 0x2d, 0x72,
                           0x74, 0x6d, 0x70, 0x2d, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72,
                           0x2d, 0x77, 0x69, 0x6e, 0x6c, 0x69, 0x6e, 0x2d, 0x77, 0x69,
                           0x6e, 0x74, 0x65, 0x72, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72,
                           0x40, 0x31, 0x32, 0x36, 0x2e, 0x63, 0x6f, 0x6d };
    for (int i = 0; i < size; i++) {
        bytes[i] = cdata[rand() % (sizeof(cdata) - 1)];
    }
}

}//namespace mediakit

namespace toolkit {
    StatisticImp(mediakit::RtmpPacket);
}