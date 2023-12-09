/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
    using Ptr = std::shared_ptr<MediaPlayerForC>;

    MediaPlayerForC(){
        _player = std::make_shared<MediaPlayer>();
    }

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

    void unset() {
        for (auto &track : _player->getTracks(false)) {
            track->clear();
        }
        lock_guard<recursive_mutex> lck(_mtx);
        _on_play = nullptr;
        _on_shutdown = nullptr;
    }

    void onEvent(bool is_shutdown, const SockException &ex){
        lock_guard<recursive_mutex> lck(_mtx);
        if (is_shutdown) {
            //播放中断
            if (_on_shutdown) {
                _on_shutdown(_on_shutdown_data.get(), ex.getErrCode(), ex.what(), nullptr, 0);
            }
            return;
        }

        //播放结果
        if (_on_play) {
            auto cpp_tracks = _player->getTracks(false);
            mk_track tracks[TrackMax] = {nullptr};
            int track_count = 0;
            for (auto &track : cpp_tracks) {
                tracks[track_count++] = (mk_track) &track;
            }
            _on_play(_on_play_data.get(), ex.getErrCode(), ex.what(), tracks, track_count);
        }
    }

    void setOnEvent(on_mk_play_event cb, std::shared_ptr<void> user_data, int type) {
        lock_guard<recursive_mutex> lck(_mtx);
        if (type == 0) {
            _on_play_data = std::move(user_data);
            _on_play = cb;
        } else {
            _on_shutdown_data = std::move(user_data);
            _on_shutdown = cb;
        }
    }

    MediaPlayer::Ptr& getPlayer(){
        return _player;
    }
private:
    MediaPlayer::Ptr _player;
    recursive_mutex _mtx;
    on_mk_play_event _on_play = nullptr;
    on_mk_play_event _on_shutdown = nullptr;

    std::shared_ptr<void> _on_play_data;
    std::shared_ptr<void> _on_shutdown_data;
};

API_EXPORT mk_player API_CALL mk_player_create() {
    MediaPlayerForC::Ptr *obj = new MediaPlayerForC::Ptr(new MediaPlayerForC());
    (*obj)->setup();
    return (mk_player)obj;
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

API_EXPORT void API_CALL mk_player_speed(mk_player ctx, float speed) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *) ctx);
    auto player = obj.getPlayer();
    player->getPoller()->async([speed, player]() {
        //切换线程后再操作
        player->speed(speed);
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

API_EXPORT void API_CALL mk_player_seekto_pos(mk_player ctx, int seek_pos) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *) ctx);
    auto player = obj.getPlayer();
    player->getPoller()->async([seek_pos, player]() {
        //切换线程后再操作
        player->seekTo((uint32_t) seek_pos);
    });
}

static void mk_player_set_on_event(mk_player ctx, on_mk_play_event cb, std::shared_ptr<void> user_data, int type) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    obj.setOnEvent(cb, std::move(user_data), type);
}

API_EXPORT void API_CALL mk_player_set_on_result(mk_player ctx, on_mk_play_event cb, void *user_data) {
    mk_player_set_on_result2(ctx, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_player_set_on_result2(mk_player ctx, on_mk_play_event cb, void *user_data, on_user_data_free user_data_free) {
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    mk_player_set_on_event(ctx, cb, std::move(ptr), 0);
}

API_EXPORT void API_CALL mk_player_set_on_shutdown(mk_player ctx, on_mk_play_event cb, void *user_data) {
    mk_player_set_on_shutdown2(ctx, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_player_set_on_shutdown2(mk_player ctx, on_mk_play_event cb, void *user_data, on_user_data_free user_data_free){
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    mk_player_set_on_event(ctx, cb, std::move(ptr), 1);
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

API_EXPORT int API_CALL mk_player_progress_pos(mk_player ctx) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *) ctx);
    return obj->getProgressPos();
}

API_EXPORT float API_CALL mk_player_loss_rate(mk_player ctx, int track_type) {
    assert(ctx);
    MediaPlayerForC &obj = **((MediaPlayerForC::Ptr *)ctx);
    return obj->getPacketLossRate((TrackType)track_type);
}
