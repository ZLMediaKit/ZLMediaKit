/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_media.h"
#include "Util/logger.h"
#include "Common/Device.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

class MediaHelper : public MediaSourceEvent , public std::enable_shared_from_this<MediaHelper> {
public:
    typedef std::shared_ptr<MediaHelper> Ptr;
    template<typename ...ArgsType>
    MediaHelper(ArgsType &&...args){
        _channel = std::make_shared<DevChannel>(std::forward<ArgsType>(args)...);
    }
    ~MediaHelper(){}

    void attachEvent(){
        _channel->setMediaListener(shared_from_this());
    }

    DevChannel::Ptr &getChannel(){
        return _channel;
    }

    void setOnClose(on_mk_media_close cb, void *user_data){
        _on_close = cb;
        _on_close_data = user_data;
    }

    void setOnSeek(on_mk_media_seek cb, void *user_data){
        _on_seek = cb;
        _on_seek_data = user_data;
    }
protected:
    // 通知其停止推流
    bool close(MediaSource &sender,bool force) override{
        if(!force && _channel->totalReaderCount()){
            //非强制关闭且正有人在观看该视频
            return false;
        }
        if(!_on_close){
            //未设置回调，没法关闭
            WarnL << "请使用mk_media_set_on_close函数设置回调函数!";
            return false;
        }
        //请在回调中调用mk_media_release函数释放资源,否则MediaSource::close()操作不会生效
        _on_close(_on_close_data);
        WarnL << "close media:" << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
        return true;
    }

    bool seekTo(MediaSource &sender,uint32_t ui32Stamp) override{
        if(!_on_seek){
            return false;
        }
        return _on_seek(_on_seek_data,ui32Stamp);
    }
    // 观看总人数
    int totalReaderCount(MediaSource &sender) override{
        return _channel->totalReaderCount();
    }
private:
    DevChannel::Ptr _channel;
    on_mk_media_close _on_close = nullptr;
    on_mk_media_seek _on_seek = nullptr;
    void *_on_seek_data;
    void *_on_close_data;
};

API_EXPORT void API_CALL mk_media_set_on_close(mk_media ctx, on_mk_media_close cb, void *user_data){
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->setOnClose(cb, user_data);
}

API_EXPORT void API_CALL mk_media_set_on_seek(mk_media ctx, on_mk_media_seek cb, void *user_data){
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->setOnSeek(cb, user_data);
}

API_EXPORT int API_CALL mk_media_total_reader_count(mk_media ctx){
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    return (*obj)->getChannel()->totalReaderCount();
}

API_EXPORT mk_media API_CALL mk_media_create(const char *vhost, const char *app, const char *stream, float duration,
                                             int rtsp_enabled, int rtmp_enabled, int hls_enabled, int mp4_enabled) {
    assert(vhost && app && stream);
    MediaHelper::Ptr *obj(new MediaHelper::Ptr(new MediaHelper(vhost, app, stream, duration,
                                                               rtsp_enabled, rtmp_enabled, hls_enabled, mp4_enabled)));
    (*obj)->attachEvent();
    return (mk_media) obj;
}

API_EXPORT void API_CALL mk_media_release(mk_media ctx) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_media_init_video(mk_media ctx, int track_id, int width, int height, int fps){
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    VideoInfo info;
    info.codecId = (CodecId)track_id;
    info.iFrameRate = fps;
    info.iWidth = width;
    info.iHeight = height;
    (*obj)->getChannel()->initVideo(info);
}

API_EXPORT void API_CALL mk_media_init_audio(mk_media ctx, int track_id, int sample_rate, int channels, int sample_bit){
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    AudioInfo info;
    info.codecId = (CodecId)track_id;
    info.iSampleRate = sample_rate;
    info.iChannel = channels;
    info.iSampleBit = sample_bit;
    (*obj)->getChannel()->initAudio(info);
}

API_EXPORT void API_CALL mk_media_init_complete(mk_media ctx){
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->addTrackCompleted();
}

API_EXPORT void API_CALL mk_media_input_h264(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts) {
    assert(ctx && data && len > 0);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->inputH264((char *) data, len, dts, pts);
}

API_EXPORT void API_CALL mk_media_input_h265(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts) {
    assert(ctx && data && len > 0);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->inputH265((char *) data, len, dts, pts);
}

API_EXPORT void API_CALL mk_media_input_aac(mk_media ctx, void *data, int len, uint32_t dts, void *adts) {
    assert(ctx && data && len > 0 && adts);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->inputAAC((char *) data, len, dts, (char *) adts);
}

API_EXPORT void API_CALL mk_media_input_g711(mk_media ctx, void* data, int len, uint32_t dts){
    assert(ctx && data && len > 0);
    MediaHelper::Ptr* obj = (MediaHelper::Ptr*) ctx;
    (*obj)->getChannel()->inputG711((char*)data, len, dts);
}
