/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "mk_player.h"
#include "Util/logger.h"
#include "Player/MediaPlayer.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

API_EXPORT mk_player API_CALL mk_player_create() {
    MediaPlayer::Ptr *obj = new MediaPlayer::Ptr(new MediaPlayer());
    return obj;
}
API_EXPORT void API_CALL mk_player_release(mk_player ctx) {
    assert(ctx);
    MediaPlayer::Ptr *obj = (MediaPlayer::Ptr *)ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_player_set_option(mk_player ctx,const char* key,const char *val){
    assert(ctx && key && val);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    string key_str(key), val_str(val);
    player->getPoller()->async([key_str,val_str,player](){
        //切换线程后再操作
        (*player)[key_str] = val_str;
    });
}
API_EXPORT void API_CALL mk_player_play(mk_player ctx, const char *url) {
    assert(ctx && url);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    string url_str(url);
    player->getPoller()->async([url_str,player](){
        //切换线程后再操作
        player->play(url_str);
    });
}

API_EXPORT void API_CALL mk_player_pause(mk_player ctx, int pause) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    player->getPoller()->async([pause,player](){
        //切换线程后再操作
        player->pause(pause);
    });
}

API_EXPORT void API_CALL mk_player_seekto(mk_player ctx, float progress) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    player->getPoller()->async([progress,player](){
        //切换线程后再操作
        player->seekTo(progress);
    });
}

static void mk_player_set_on_event(mk_player ctx, on_mk_play_event cb, void *user_data, int type) {
    assert(ctx && cb);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    player->getPoller()->async([cb,user_data,type,player](){
        //切换线程后再操作
        if(type == 0){
            player->setOnPlayResult([cb,user_data](const SockException &ex){
                cb(user_data,ex.getErrCode(),ex.what());
            });
        }else{
            player->setOnShutdown([cb,user_data](const SockException &ex){
                cb(user_data,ex.getErrCode(),ex.what());
            });
        }
    });
}

API_EXPORT void API_CALL mk_player_set_on_result(mk_player ctx, on_mk_play_event cb, void *user_data) {
    mk_player_set_on_event(ctx,cb,user_data,0);
}

API_EXPORT void API_CALL mk_player_set_on_shutdown(mk_player ctx, on_mk_play_event cb, void *user_data) {
    mk_player_set_on_event(ctx,cb,user_data,1);
}

API_EXPORT void API_CALL mk_player_set_on_data(mk_player ctx, on_mk_play_data cb, void *user_data) {
    assert(ctx && cb);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    player->getPoller()->async([player,cb,user_data](){
        //切换线程后再操作
        auto delegate = std::make_shared<FrameWriterInterfaceHelper>([cb,user_data](const Frame::Ptr &frame){
            cb(user_data,frame->getTrackType(),frame->getCodecId(),frame->data(),frame->size(),frame->dts(),frame->pts());
        });
        for(auto &track : player->getTracks()){
            track->addDelegate(delegate);
        }
    });
}

API_EXPORT int API_CALL mk_player_video_width(mk_player ctx) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    auto track = dynamic_pointer_cast<VideoTrack>(player->getTrack(TrackVideo));
    return track ? track->getVideoWidth() : 0;
}

API_EXPORT int API_CALL mk_player_video_height(mk_player ctx) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    auto track = dynamic_pointer_cast<VideoTrack>(player->getTrack(TrackVideo));
    return track ? track->getVideoHeight() : 0;
}

API_EXPORT int API_CALL mk_player_video_fps(mk_player ctx) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    auto track = dynamic_pointer_cast<VideoTrack>(player->getTrack(TrackVideo));
    return track ? track->getVideoFps() : 0;
}

API_EXPORT int API_CALL mk_player_audio_samplerate(mk_player ctx) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    auto track = dynamic_pointer_cast<AudioTrack>(player->getTrack(TrackAudio));
    return track ? track->getAudioSampleRate() : 0;
}

API_EXPORT int API_CALL mk_player_audio_bit(mk_player ctx) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    auto track = dynamic_pointer_cast<AudioTrack>(player->getTrack(TrackAudio));
    return track ? track->getAudioSampleBit() : 0;
}

API_EXPORT int API_CALL mk_player_audio_channel(mk_player ctx) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    auto track = dynamic_pointer_cast<AudioTrack>(player->getTrack(TrackAudio));
    return track ? track->getAudioChannel() : 0;
}

API_EXPORT float API_CALL mk_player_duration(mk_player ctx) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    return player->getDuration();
}

API_EXPORT float API_CALL mk_player_progress(mk_player ctx) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    return player->getProgress();
}

API_EXPORT float API_CALL mk_player_loss_rate(mk_player ctx, int track_type) {
    assert(ctx);
    MediaPlayer::Ptr &player = *((MediaPlayer::Ptr *)ctx);
    return player->getPacketLossRate((TrackType)track_type);
}
