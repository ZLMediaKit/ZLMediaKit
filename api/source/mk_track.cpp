/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_track.h"
#include "Extension/Track.h"
#include "Extension/Factory.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

class VideoTrackForC : public VideoTrack, public std::enable_shared_from_this<VideoTrackForC> {
public:
    VideoTrackForC(int codec_id, codec_args *args) {
        _codec_id = (CodecId) codec_id;
        if (args) {
            _args = *args;
        } else {
            memset(&_args, 0, sizeof(_args));
        }
    }

    int getVideoHeight() const override {
        return _args.video.height;
    }

    int getVideoWidth() const override {
        return _args.video.width;
    }

    float getVideoFps() const override {
        return _args.video.fps;
    }

    CodecId getCodecId() const override {
        return _codec_id;
    }

    bool ready() const override {
        return true;
    }

    Track::Ptr clone() const override {
        auto track_in = std::shared_ptr<Track>(const_cast<VideoTrackForC *>(this)->shared_from_this());
        return Factory::getTrackByAbstractTrack(track_in);
    }

    Sdp::Ptr getSdp(uint8_t) const override {
        return nullptr;
    }

private:
    CodecId _codec_id;
    codec_args _args;
};

class AudioTrackForC : public AudioTrackImp, public std::enable_shared_from_this<AudioTrackForC> {
public:
    AudioTrackForC(int codec_id, codec_args *args) :
        AudioTrackImp((CodecId) codec_id, args->audio.sample_rate, args->audio.channels, 16) {}

    Track::Ptr clone() const override {
        auto track_in = std::shared_ptr<Track>(const_cast<AudioTrackForC *>(this)->shared_from_this());
        return Factory::getTrackByAbstractTrack(track_in);
    }

    Sdp::Ptr getSdp(uint8_t payload_type) const override {
        return nullptr;
    }
};

API_EXPORT mk_track API_CALL mk_track_create(int codec_id, codec_args *args) {
    switch (getTrackType((CodecId) codec_id)) {
        case TrackVideo: return (mk_track)new Track::Ptr(std::make_shared<VideoTrackForC>(codec_id, args));
        case TrackAudio: return (mk_track)new Track::Ptr(std::make_shared<AudioTrackForC>(codec_id, args));
        default: WarnL << "unrecognized codec:" << codec_id; return nullptr;
    }
}

API_EXPORT void API_CALL mk_track_unref(mk_track track) {
    assert(track);
    delete (Track::Ptr *)track;
}

API_EXPORT mk_track API_CALL mk_track_ref(mk_track track) {
    assert(track);
    return (mk_track)new Track::Ptr(*( (Track::Ptr *)track));
}

API_EXPORT int API_CALL mk_track_codec_id(mk_track track) {
    assert(track);
    return (*((Track::Ptr *) track))->getCodecId();
}

API_EXPORT const char *API_CALL mk_track_codec_name(mk_track track) {
    assert(track);
    return (*((Track::Ptr *) track))->getCodecName();
}

API_EXPORT int API_CALL mk_track_bit_rate(mk_track track) {
    assert(track);
    return (*((Track::Ptr *) track))->getBitRate();
}

API_EXPORT void *API_CALL mk_track_add_delegate(mk_track track, on_mk_frame_out cb, void *user_data) {
    return mk_track_add_delegate2(track, cb, user_data, nullptr);
}

API_EXPORT void *API_CALL mk_track_add_delegate2(mk_track track, on_mk_frame_out cb, void *user_data, on_user_data_free user_data_free){
    assert(track && cb);
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    return (*((Track::Ptr *) track))->addDelegate([cb, ptr](const Frame::Ptr &frame) {
        cb(ptr.get(), (mk_frame) &frame);
        return true;
    });
}

API_EXPORT void API_CALL mk_track_del_delegate(mk_track track, void *tag) {
    assert(track && tag);
    (*((Track::Ptr *) track))->delDelegate((FrameWriterInterface *) tag);
}

API_EXPORT void API_CALL mk_track_input_frame(mk_track track, mk_frame frame) {
    assert(track && frame);
    (*((Track::Ptr *) track))->inputFrame((*((Frame::Ptr *) frame)));
}

API_EXPORT int API_CALL mk_track_is_video(mk_track track) {
    assert(track);
    return (*((Track::Ptr *) track))->getTrackType() == TrackVideo;
}

API_EXPORT int API_CALL mk_track_video_width(mk_track track) {
    assert(track);
    auto video = dynamic_pointer_cast<VideoTrack>((*((Track::Ptr *) track)));
    if (video) {
        return video->getVideoWidth();
    }
    WarnL << "not video track";
    return 0;
}

API_EXPORT int API_CALL mk_track_video_height(mk_track track) {
    assert(track);
    auto video = dynamic_pointer_cast<VideoTrack>((*((Track::Ptr *) track)));
    if (video) {
        return video->getVideoHeight();
    }
    WarnL << "not video track";
    return 0;
}

API_EXPORT int API_CALL mk_track_video_fps(mk_track track) {
    assert(track);
    auto video = dynamic_pointer_cast<VideoTrack>((*((Track::Ptr *) track)));
    if (video) {
        return video->getVideoFps();
    }
    WarnL << "not video track";
    return 0;
}

API_EXPORT int API_CALL mk_track_audio_sample_rate(mk_track track) {
    assert(track);
    auto audio = dynamic_pointer_cast<AudioTrack>((*((Track::Ptr *) track)));
    if (audio) {
        return audio->getAudioSampleRate();
    }
    WarnL << "not audio track";
    return 0;
}

API_EXPORT int API_CALL mk_track_audio_channel(mk_track track) {
    assert(track);
    auto audio = dynamic_pointer_cast<AudioTrack>((*((Track::Ptr *) track)));
    if (audio) {
        return audio->getAudioChannel();
    }
    WarnL << "not audio track";
    return 0;
}

API_EXPORT int API_CALL mk_track_audio_sample_bit(mk_track track) {
    assert(track);
    auto audio = dynamic_pointer_cast<AudioTrack>((*((Track::Ptr *) track)));
    if (audio) {
        return audio->getAudioSampleBit();
    }
    WarnL << "not audio track";
    return 0;
}