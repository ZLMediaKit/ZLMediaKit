/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

class MediaHelper: public MediaSourceEvent, public std::enable_shared_from_this<MediaHelper> {
public:
    using Ptr = std::shared_ptr<MediaHelper>;
    MediaHelper(const char *vhost, const char *app, const char *stream, float duration, const ProtocolOption &option) {
        _poller = EventPollerPool::Instance().getPoller();
        // 在poller线程中创建DevChannel(MultiMediaSourceMuxer)对象，确保严格的线程安全限制
        _poller->sync([&]() { _channel = std::make_shared<DevChannel>(vhost, app, stream, duration, option); });
    }

    ~MediaHelper() = default;

    void attachEvent() { _channel->setMediaListener(shared_from_this()); }

    DevChannel::Ptr &getChannel() { return _channel; }

    void setOnClose(on_mk_media_close cb, std::shared_ptr<void> user_data) {
        _on_close = cb;
        _on_close_data = std::move(user_data);
    }

    void setOnSeek(on_mk_media_seek cb, std::shared_ptr<void> user_data) {
        _on_seek = cb;
        _on_seek_data = std::move(user_data);
    }

    void setOnPause(on_mk_media_pause cb, std::shared_ptr<void> user_data) {
        _on_pause = cb;
        _on_pause_data = std::move(user_data);
    }

    void setOnSpeed(on_mk_media_speed cb, std::shared_ptr<void> user_data) {
        _on_speed = cb;
        _on_speed_data = std::move(user_data);
    }

    void setOnRegist(on_mk_media_source_regist cb, std::shared_ptr<void> user_data) {
        _on_regist = cb;
        _on_regist_data = std::move(user_data);
    }

protected:
    // 通知其停止推流
    bool close(MediaSource &sender) override {
        if (!_on_close) {
            //未设置回调，没法关闭
            WarnL << "请使用mk_media_set_on_close函数设置回调函数!";
            return false;
        }
        //请在回调中调用mk_media_release函数释放资源,否则MediaSource::close()操作不会生效
        _on_close(_on_close_data.get());
        WarnL << "close media: " << sender.getUrl();
        return true;
    }

    bool seekTo(MediaSource &sender, uint32_t stamp) override {
        if (!_on_seek) {
            return false;
        }
        return _on_seek(_on_seek_data.get(), stamp);
    }

    // 通知暂停或恢复
    bool pause(MediaSource &sender, bool pause) override {
        if (!_on_pause) {
            return false;
        }
        return _on_pause(_on_pause_data.get(), pause);
    }

    //通知倍数播放
    bool speed(MediaSource &sender, float speed) override {
        if (!_on_speed) {
            return false;
        }
        return _on_speed(_on_speed_data.get(), speed);
    }

    void onRegist(MediaSource &sender, bool regist) override {
        if (_on_regist) {
            _on_regist(_on_regist_data.get(), (mk_media_source)&sender, regist);
        }
    }

    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override { return _poller; }

private:
    EventPoller::Ptr _poller;
    DevChannel::Ptr _channel;
    on_mk_media_close _on_close = nullptr;
    on_mk_media_seek _on_seek = nullptr;
    on_mk_media_pause _on_pause = nullptr;
    on_mk_media_speed _on_speed = nullptr;
    on_mk_media_source_regist _on_regist = nullptr;
    std::shared_ptr<void> _on_seek_data;
    std::shared_ptr<void> _on_pause_data;
    std::shared_ptr<void> _on_speed_data;
    std::shared_ptr<void> _on_close_data;
    std::shared_ptr<void> _on_regist_data;
};

API_EXPORT void API_CALL mk_media_set_on_close(mk_media ctx, on_mk_media_close cb, void *user_data) {
    mk_media_set_on_close2(ctx, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_media_set_on_close2(mk_media ctx, on_mk_media_close cb, void *user_data, on_user_data_free user_data_free) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    (*obj)->setOnClose(cb, std::move(ptr));
}

API_EXPORT void API_CALL mk_media_set_on_seek(mk_media ctx, on_mk_media_seek cb, void *user_data) {
    mk_media_set_on_seek2(ctx, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_media_set_on_seek2(mk_media ctx, on_mk_media_seek cb, void *user_data, on_user_data_free user_data_free) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    (*obj)->setOnSeek(cb, std::move(ptr));
}

API_EXPORT void API_CALL mk_media_set_on_pause(mk_media ctx, on_mk_media_pause cb, void *user_data) {
    mk_media_set_on_pause2(ctx, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_media_set_on_pause2(mk_media ctx, on_mk_media_pause cb, void *user_data, on_user_data_free user_data_free) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    (*obj)->setOnPause(cb, std::move(ptr));
}

API_EXPORT void API_CALL mk_media_set_on_speed(mk_media ctx, on_mk_media_speed cb, void *user_data) {
    mk_media_set_on_speed2(ctx, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_media_set_on_speed2(mk_media ctx, on_mk_media_speed cb, void *user_data, on_user_data_free user_data_free) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    (*obj)->setOnSpeed(cb, std::move(ptr));
}

API_EXPORT void API_CALL mk_media_set_on_regist(mk_media ctx, on_mk_media_source_regist cb, void *user_data) {
    mk_media_set_on_regist2(ctx, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_media_set_on_regist2(mk_media ctx, on_mk_media_source_regist cb, void *user_data, on_user_data_free user_data_free) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    (*obj)->setOnRegist(cb, std::move(ptr));
}

API_EXPORT int API_CALL mk_media_total_reader_count(mk_media ctx) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    return (*obj)->getChannel()->totalReaderCount();
}

API_EXPORT mk_media API_CALL mk_media_create(const char *vhost, const char *app, const char *stream,
                                             float duration, int hls_enabled, int mp4_enabled) {
    assert(vhost && app && stream);
    ProtocolOption option;
    option.enable_hls = hls_enabled;
    option.enable_mp4 = mp4_enabled;

    MediaHelper::Ptr *obj(new MediaHelper::Ptr(new MediaHelper(vhost, app, stream, duration, option)));
    (*obj)->attachEvent();
    return (mk_media) obj;
}

API_EXPORT mk_media API_CALL mk_media_create2(const char *vhost, const char *app, const char *stream, float duration, mk_ini ini) {
    assert(vhost && app && stream && ini);
    ProtocolOption option(*((mINI *)ini));
    MediaHelper::Ptr *obj(new MediaHelper::Ptr(new MediaHelper(vhost, app, stream, duration, option)));
    (*obj)->attachEvent();
    return (mk_media) obj;
}

API_EXPORT void API_CALL mk_media_release(mk_media ctx) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    delete obj;
}

API_EXPORT int API_CALL mk_media_init_video(mk_media ctx, int codec_id, int width, int height, float fps, int bit_rate) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    VideoInfo info;
    info.codecId = (CodecId)codec_id;
    info.iFrameRate = fps;
    info.iWidth = width;
    info.iHeight = height;
    info.iBitRate = bit_rate;
    return (*obj)->getChannel()->initVideo(info);
}

API_EXPORT int API_CALL mk_media_init_audio(mk_media ctx, int codec_id, int sample_rate, int channels, int sample_bit) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    AudioInfo info;
    info.codecId = (CodecId)codec_id;
    info.iSampleRate = sample_rate;
    info.iChannel = channels;
    info.iSampleBit = sample_bit;
    return (*obj)->getChannel()->initAudio(info);
}

API_EXPORT void API_CALL mk_media_init_track(mk_media ctx, mk_track track) {
    assert(ctx && track);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->addTrack(*((Track::Ptr *) track));
}

API_EXPORT void API_CALL mk_media_init_complete(mk_media ctx) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->addTrackCompleted();
}

API_EXPORT int API_CALL mk_media_input_frame(mk_media ctx, mk_frame frame) {
    assert(ctx && frame);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    return (*obj)->getChannel()->inputFrame(*((Frame::Ptr *) frame));
}

API_EXPORT int API_CALL mk_media_input_h264(mk_media ctx, const void *data, int len, uint64_t dts, uint64_t pts) {
    assert(ctx && data && len > 0);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    return (*obj)->getChannel()->inputH264((const char *) data, len, dts, pts);
}

API_EXPORT int API_CALL mk_media_input_h265(mk_media ctx, const void *data, int len, uint64_t dts, uint64_t pts) {
    assert(ctx && data && len > 0);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    return (*obj)->getChannel()->inputH265((const char *) data, len, dts, pts);
}

API_EXPORT void API_CALL mk_media_input_yuv(mk_media ctx, const char *yuv[3], int linesize[3], uint64_t cts) {
    assert(ctx && yuv && linesize);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    (*obj)->getChannel()->inputYUV((char **) yuv, linesize, cts);
}

API_EXPORT int API_CALL mk_media_input_aac(mk_media ctx, const void *data, int len, uint64_t dts, void *adts) {
    assert(ctx && data && len > 0 && adts);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *) ctx;
    return (*obj)->getChannel()->inputAAC((const char *) data, len, dts, (char *) adts);
}

API_EXPORT int API_CALL mk_media_input_pcm(mk_media ctx, void *data, int len, uint64_t pts) {
    assert(ctx && data && len > 0);
    MediaHelper::Ptr* obj = (MediaHelper::Ptr*) ctx;
    return (*obj)->getChannel()->inputPCM((char*)data, len, pts);
}

API_EXPORT int API_CALL mk_media_input_audio(mk_media ctx, const void *data, int len, uint64_t dts) {
    assert(ctx && data && len > 0);
    MediaHelper::Ptr* obj = (MediaHelper::Ptr*) ctx;
    return (*obj)->getChannel()->inputAudio((const char*)data, len, dts);
}

API_EXPORT void API_CALL mk_media_start_send_rtp(mk_media ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int is_udp, on_mk_media_send_rtp_result cb, void *user_data) {
    mk_media_start_send_rtp2(ctx, dst_url, dst_port, ssrc, is_udp, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_media_start_send_rtp2(mk_media ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int is_udp, on_mk_media_send_rtp_result cb, void *user_data,
    on_user_data_free user_data_free) {
    assert(ctx && dst_url && ssrc);
    MediaHelper::Ptr* obj = (MediaHelper::Ptr*) ctx;

    MediaSourceEvent::SendRtpArgs args;
    args.dst_url = dst_url;
    args.dst_port = dst_port;
    args.ssrc = ssrc;
    args.is_udp = is_udp;

    // sender参数无用
    auto ref = *obj;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    (*obj)->getChannel()->getOwnerPoller(MediaSource::NullMediaSource())->async([args, ref, cb, ptr]() {
        ref->getChannel()->startSendRtp(MediaSource::NullMediaSource(), args, [cb, ptr](uint16_t local_port, const SockException &ex) {
            if (cb) {
                cb(ptr.get(), local_port, ex.getErrCode(), ex.what());
            }
        });
    });
}

API_EXPORT void API_CALL mk_media_stop_send_rtp(mk_media ctx, const char *ssrc) {
    assert(ctx);
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *)ctx;
    // sender参数无用
    auto ref = *obj;
    string ssrc_str = ssrc ? ssrc : "";
    (*obj)->getChannel()->getOwnerPoller(MediaSource::NullMediaSource())->async([ref, ssrc_str]() {
        ref->getChannel()->stopSendRtp(MediaSource::NullMediaSource(), ssrc_str);
    });
}

API_EXPORT mk_thread API_CALL mk_media_get_owner_thread(mk_media ctx) {
    MediaHelper::Ptr *obj = (MediaHelper::Ptr *)ctx;
    return (mk_thread)(*obj)->getChannel()->getOwnerPoller(MediaSource::NullMediaSource()).get();
}