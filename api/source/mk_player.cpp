/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_player.h"
#include "Util/logger.h"
#include "Player/MediaPlayer.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

class MediaPlayerForC : public std::enable_shared_from_this<MediaPlayerForC>{
public:
    typedef std::shared_ptr<MediaPlayerForC> Ptr;

    MediaPlayerForC(){
        _player = std::make_shared<MediaPlayer>();
    }
    ~MediaPlayerForC(){}

    MediaPlayer *operator->(){
        return _player.get();
    }

    void setup(){
        weak_ptr<MediaPlayerForC> weak_self = shared_from_this();
        _player->setOnPlayResult([weak_self](const SockException &ex){
            auto strong_self = weak_self.lock();
            if(strong_self){
                strong_self->onEvent(false,ex);
            }
        });

        _player->setOnShutdown([weak_self](const SockException &ex){
            auto strong_self = weak_self.lock();
            if(strong_self){
                strong_self->onEvent(true,ex);
            }
        });
    }

    void unset(){
        lock_guard<recursive_mutex> lck(_mtx);
        _on_play = nullptr;
        _on_shutdown = nullptr;
        _on_data = nullptr;
    }

    void onEvent(bool is_shutdown, const SockException &ex){
        lock_guard<recursive_mutex> lck(_mtx);
        if(is_shutdown){
            //播放中断
            if(_on_shutdown){
                _on_shutdown(_on_shutdown_data,ex.getErrCode(),ex.what());
            }
            return;
        }

        //播放结果
        if(_on_play){
            _on_play(_on_play_data,ex.getErrCode(),ex.what());
        }

        if(ex){
            //播放失败
            return;
        }

        //播放成功,添加事件回调
        weak_ptr<MediaPlayerForC> weak_self = shared_from_this();
        auto delegate = std::make_shared<FrameWriterInterfaceHelper>([weak_self](const Frame::Ptr &frame) {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                strong_self->onData(frame);
            }
        });
        for (auto &track : _player->getTracks()) {
            track->addDelegate(delegate);
        }
    }

    void onData(const Frame::Ptr &frame){
        lock_guard<recursive_mutex> lck(_mtx);
        if(_on_data){
            _on_data(_on_data_data,frame->getTrackType(),frame->getCodecId(),frame->data(),frame->size(),frame->dts(),frame->pts());
        }
    }

    void setOnEvent(on_mk_play_event cb, void *user_data, int type) {
        lock_guard<recursive_mutex> lck(_mtx);
        if(type == 0){
            _on_play_data = user_data;
            _on_play = cb;
        }else{
            _on_shutdown_data = user_data;
            _on_shutdown = cb;
        }
    }

    void setOnData(on_mk_play_data cb, void *user_data) {
        lock_guard<recursive_mutex> lck(_mtx);
        _on_data_data = user_data;
        _on_data = cb;
    }

    MediaPlayer::Ptr& getPlayer(){
        return _player;
    }
private:
    MediaPlayer::Ptr _player;
    recursive_mutex _mtx;
    on_mk_play_event _on_play = nullptr;
    on_mk_play_data _on_data = nullptr;
    on_mk_play_event _on_shutdown = nullptr;

    void *_on_play_data = nullptr;
    void *_on_shutdown_data = nullptr;
    void *_on_data_data = nullptr;
};

API_EXPORT mk_player API_CALL mk_player_create() {
    MediaPlayerForC::Ptr *obj = new MediaPlayerForC::Ptr(new MediaPlayerForC());
    (*obj)->setup();
    return obj;
}
API_EXPORT void API_CALL mk_player_release(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC::Ptr *obj = (MediaPlayerForC::Ptr *)ctx;
    (*obj)->unset();
    delete obj;
}

API_EXPORT void API_CALL mk_player_set_option(mk_player ctx,const char* key,const char *val){
    assert(ctx && key && val);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto player = obj.getPlayer();
    string key_str(key), val_str(val);
    player->getPoller()->async([key_str,val_str,player](){
        //切换线程后再操作
        (*player)[key_str] = val_str;
    });
}
API_EXPORT void API_CALL mk_player_play(mk_player ctx, const char *url) {
    assert(ctx && url);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto player = obj.getPlayer();
    string url_str(url);
    player->getPoller()->async([url_str,player](){
        //切换线程后再操作
        player->play(url_str);
    });
}

API_EXPORT void API_CALL mk_player_pause(mk_player ctx, int pause) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto player = obj.getPlayer();
    player->getPoller()->async([pause,player](){
        //切换线程后再操作
        player->pause(pause);
    });
}

API_EXPORT void API_CALL mk_player_seekto(mk_player ctx, float progress) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto player = obj.getPlayer();
    player->getPoller()->async([progress,player](){
        //切换线程后再操作
        player->seekTo(progress);
    });
}

static void mk_player_set_on_event(mk_player ctx, on_mk_play_event cb, void *user_data, int type) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    obj.setOnEvent(cb,user_data, type);
}

API_EXPORT void API_CALL mk_player_set_on_result(mk_player ctx, on_mk_play_event cb, void *user_data) {
    mk_player_set_on_event(ctx,cb,user_data,0);
}

API_EXPORT void API_CALL mk_player_set_on_shutdown(mk_player ctx, on_mk_play_event cb, void *user_data) {
    mk_player_set_on_event(ctx,cb,user_data,1);
}

API_EXPORT void API_CALL mk_player_set_on_data(mk_player ctx, on_mk_play_data cb, void *user_data) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    obj.setOnData(cb,user_data);
}

API_EXPORT int API_CALL mk_player_video_codecId(mk_player ctx){
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto track = dynamic_pointer_cast<VideoTrack>(obj->getTrack(TrackVideo));
    return track ? track->getCodecId() : CodecInvalid;
}

API_EXPORT int API_CALL mk_player_video_width(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto track = dynamic_pointer_cast<VideoTrack>(obj->getTrack(TrackVideo));
    return track ? track->getVideoWidth() : 0;
}

API_EXPORT int API_CALL mk_player_video_height(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto track = dynamic_pointer_cast<VideoTrack>(obj->getTrack(TrackVideo));
    return track ? track->getVideoHeight() : 0;
}

API_EXPORT int API_CALL mk_player_video_fps(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto track = dynamic_pointer_cast<VideoTrack>(obj->getTrack(TrackVideo));
    return track ? track->getVideoFps() : 0;
}

API_EXPORT int API_CALL mk_player_audio_codecId(mk_player ctx){
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto track = dynamic_pointer_cast<AudioTrack>(obj->getTrack(TrackAudio));
    return track ? track->getCodecId() : CodecInvalid;
}

API_EXPORT int API_CALL mk_player_audio_samplerate(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto track = dynamic_pointer_cast<AudioTrack>(obj->getTrack(TrackAudio));
    return track ? track->getAudioSampleRate() : 0;
}

API_EXPORT int API_CALL mk_player_audio_bit(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto track = dynamic_pointer_cast<AudioTrack>(obj->getTrack(TrackAudio));
    return track ? track->getAudioSampleBit() : 0;
}

API_EXPORT int API_CALL mk_player_audio_channel(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    auto track = dynamic_pointer_cast<AudioTrack>(obj->getTrack(TrackAudio));
    return track ? track->getAudioChannel() : 0;
}

API_EXPORT float API_CALL mk_player_duration(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    return obj->getDuration();
}

API_EXPORT float API_CALL mk_player_progress(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    return obj->getProgress();
}

API_EXPORT float API_CALL mk_player_loss_rate(mk_player ctx, int track_type) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    return obj->getPacketLossRate((TrackType)track_type);
}
