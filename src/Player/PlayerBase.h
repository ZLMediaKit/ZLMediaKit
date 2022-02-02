/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_PLAYER_PLAYERBASE_H_
#define SRC_PLAYER_PLAYERBASE_H_

#include <map>
#include <memory>
#include <string>
#include <functional>
#include "Network/Socket.h"
#include "Util/mini.h"
#include "Util/RingBuffer.h"
#include "Common/MediaSource.h"
#include "Common/MediaSink.h"
#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

class PlayerBase : public TrackSource, public toolkit::mINI {
public:
    using Ptr = std::shared_ptr<PlayerBase>;
    using Event = std::function<void(const toolkit::SockException &ex)>;

    static Ptr createPlayer(const toolkit::EventPoller::Ptr &poller, const std::string &strUrl);

    PlayerBase();
    ~PlayerBase() override = default;

    /**
     * 开始播放
     * @param url 视频url，支持rtsp/rtmp
     */
    virtual void play(const std::string &url) {};

    /**
     * 暂停或恢复
     * @param flag true:暂停，false:恢复
     */
    virtual void pause(bool flag) {};

    /**
     * 获取节目总时长，单位秒
     */
    virtual float getDuration() const { return 0; };

    /**
     * 倍数播放
     * @param speed 1.0 2.0 0.5
     */
    virtual void speed(float speed) {};

    /**
     * 中断播放
     */
    virtual void teardown() {};

    /**
     * 获取播放进度，取值 0.0 ~ 1.0
     */
    virtual float getProgress() const { return 0; };

    /**
     * 获取播放进度pos，取值 相对开始时间增量 单位秒
     */
    virtual uint32_t getProgressPos() const { return 0; };

    /**
     * 拖动进度条
     * @param progress 进度，取值 0.0 ~ 1.0
     */
    virtual void seekTo(float progress) {};

    /**
     * 拖动进度条
     * @param pos 进度，取值 相对于开始时间的增量 单位秒
     */
    virtual void seekTo(uint32_t pos) {};

    /**
     * 获取丢包率，只支持rtsp
     * @param type 音频或视频，TrackInvalid时为总丢包率
     */
    virtual float getPacketLossRate(TrackType type) const { return 0; };

    /**
     * 获取所有track
     */
    std::vector<Track::Ptr> getTracks(bool ready = true) const override { return std::vector<Track::Ptr>(); };

    /**
     * 设置一个MediaSource，直接生产rtsp/rtmp代理
     */
    virtual void setMediaSource(const MediaSource::Ptr &src) = 0;

    /**
     * 设置异常中断回调
     */
    virtual void setOnShutdown(const Event &cb) = 0;

    /**
     * 设置播放结果回调
     */
    virtual void setOnPlayResult(const Event &cb) = 0;

    /**
     * 设置播放恢复回调
     */
    virtual void setOnResume(const std::function<void()> &cb) = 0;

protected:
    virtual void onResume() = 0;
    virtual void onShutdown(const toolkit::SockException &ex) = 0;
    virtual void onPlayResult(const toolkit::SockException &ex) = 0;
};

template<typename Parent, typename Delegate>
class PlayerImp : public Parent {
public:
    using Ptr = std::shared_ptr<PlayerImp>;

    template<typename ...ArgsType>
    PlayerImp(ArgsType &&...args) : Parent(std::forward<ArgsType>(args)...) {}
    ~PlayerImp() override = default;

    void play(const std::string &url) override {
        return _delegate ? _delegate->play(url) : Parent::play(url);
    }

    void pause(bool flag) override {
        return _delegate ? _delegate->pause(flag) : Parent::pause(flag);
    }

    void speed(float speed) override {
        return _delegate ? _delegate->speed(speed) : Parent::speed(speed);
    }

    void teardown() override {
        return _delegate ? _delegate->teardown() : Parent::teardown();
    }

    float getPacketLossRate(TrackType type) const override {
        return _delegate ? _delegate->getPacketLossRate(type) : Parent::getPacketLossRate(type);
    }

    float getDuration() const override {
        return _delegate ? _delegate->getDuration() : Parent::getDuration();
    }

    float getProgress() const override {
        return _delegate ? _delegate->getProgress() : Parent::getProgress();
    }

    uint32_t getProgressPos() const override {
        return _delegate ? _delegate->getProgressPos() : Parent::getProgressPos();
    }

    void seekTo(float progress) override {
        return _delegate ? _delegate->seekTo(progress) : Parent::seekTo(progress);
    }

    void seekTo(uint32_t pos) override {
        return _delegate ? _delegate->seekTo(pos) : Parent::seekTo(pos);
    }

    std::vector<Track::Ptr> getTracks(bool ready = true) const override {
        return _delegate ? _delegate->getTracks(ready) : Parent::getTracks(ready);
    }

    std::shared_ptr<toolkit::SockInfo> getSockInfo() const {
        return std::dynamic_pointer_cast<toolkit::SockInfo>(_delegate);
    }

    void setMediaSource(const MediaSource::Ptr &src) override {
        if (_delegate) {
            _delegate->setMediaSource(src);
        }
        _media_src = src;
    }

    void setOnShutdown(const std::function<void(const toolkit::SockException &)> &cb) override {
        if (_delegate) {
            _delegate->setOnShutdown(cb);
        }
        _on_shutdown = cb;
    }

    void setOnPlayResult(const std::function<void(const toolkit::SockException &ex)> &cb) override {
        if (_delegate) {
            _delegate->setOnPlayResult(cb);
        }
        _on_play_result = cb;
    }

    void setOnResume(const std::function<void()> &cb) override {
        if (_delegate) {
            _delegate->setOnResume(cb);
        }
        _on_resume = cb;
    }

protected:
    void onShutdown(const toolkit::SockException &ex) override {
        if (_on_shutdown) {
            _on_shutdown(ex);
            _on_shutdown = nullptr;
        }
    }

    void onPlayResult(const toolkit::SockException &ex) override {
        if (_on_play_result) {
            _on_play_result(ex);
            _on_play_result = nullptr;
        }
    }

    void onResume() override {
        if (_on_resume) {
            _on_resume();
        }
    }

protected:
    std::function<void()> _on_resume;
    PlayerBase::Event _on_shutdown;
    PlayerBase::Event _on_play_result;
    MediaSource::Ptr _media_src;
    std::shared_ptr<Delegate> _delegate;
};

class MediaSinkDelegate : public MediaSink {
public:
    MediaSinkDelegate() = default;
    ~MediaSinkDelegate() override = default;

    /**
     * 设置track监听器
     */
    void setTrackListener(TrackListener *listener);

protected:
    void resetTracks() override;
    bool onTrackReady(const Track::Ptr & track) override;
    void onAllTrackReady() override;

private:
    TrackListener *_listener = nullptr;
};

class Demuxer : protected TrackListener, public TrackSource {
public:
    Demuxer() = default;
    ~Demuxer() override = default;

    void setTrackListener(TrackListener *listener, bool wait_track_ready = false);
    std::vector<Track::Ptr> getTracks(bool trackReady = true) const override;

protected:
    bool addTrack(const Track::Ptr &track) override;
    void addTrackCompleted() override;
    void resetTracks() override;

private:
    MediaSink::Ptr _sink;
    TrackListener *_listener = nullptr;
    std::vector<Track::Ptr> _origin_track;
};

} /* namespace mediakit */

#endif /* SRC_PLAYER_PLAYERBASE_H_ */
