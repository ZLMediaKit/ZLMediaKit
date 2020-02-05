/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
#include "Extension/Frame.h"
#include "Extension/Track.h"
using namespace toolkit;

namespace mediakit {

class DemuxerBase : public TrackSource{
public:
	typedef std::shared_ptr<DemuxerBase> Ptr;

	/**
	 * 获取节目总时长，单位秒
	 * @return
	 */
	virtual float getDuration() const { return 0;}

	/**
	 * 是否初始化完毕，完毕后方可调用getTrack方法
	 * @param analysisMs 数据流最大分析时间 单位毫秒
	 * @return
	 */
	virtual bool isInited(int analysisMs) { return true; }
};


class PlayerBase : public DemuxerBase, public mINI{
public:
	typedef std::shared_ptr<PlayerBase> Ptr;
    static Ptr createPlayer(const EventPoller::Ptr &poller,const string &strUrl);

	PlayerBase();
	virtual ~PlayerBase(){}

	/**
	 * 开始播放
	 * @param strUrl 视频url，支持rtsp/rtmp
	 */
	virtual void play(const string &strUrl) {}

	/**
	 * 暂停或恢复
	 * @param bPause
	 */
	virtual void pause(bool bPause) {}

	/**
	 * 中断播放
	 */
	virtual void teardown() {}

	/**
	 * 设置异常中断回调
	 * @param cb
	 */
	virtual void setOnShutdown( const function<void(const SockException &)> &cb) {}

	/**
	 * 设置播放结果回调
	 * @param cb
	 */
	virtual void setOnPlayResult( const function<void(const SockException &ex)> &cb) {}

    /**
     * 设置播放恢复回调
     * @param cb
     */
    virtual void setOnResume( const function<void()> &cb) {}

	/**
	 * 获取播放进度，取值 0.0 ~ 1.0
	 * @return
	 */
    virtual float getProgress() const { return 0;}

    /**
     * 拖动进度条
     * @param fProgress 进度，取值 0.0 ~ 1.0
     */
    virtual void seekTo(float fProgress) {}

    /**
     * 设置一个MediaSource，直接生产rtsp/rtmp代理
     * @param src
     */
    virtual void setMediaSouce(const MediaSource::Ptr & src) {}

    /**
     * 获取丢包率，只支持rtsp
     * @param trackType 音频或视频，TrackInvalid时为总丢包率
     * @return
     */
	virtual float getPacketLossRate(TrackType trackType) const {return 0; }

    /**
     * 获取所有track
     */
    vector<Track::Ptr> getTracks(bool trackReady = true) const override{
        return vector<Track::Ptr>();
    }
protected:
    virtual void onShutdown(const SockException &ex) {}
    virtual void onPlayResult(const SockException &ex) {}
    /**
     * 暂停后恢复播放时间
     */
    virtual void onResume(){};
};

template<typename Parent,typename Delegate>
class PlayerImp : public Parent {
public:
	typedef std::shared_ptr<PlayerImp> Ptr;

	template<typename ...ArgsType>
	PlayerImp(ArgsType &&...args):Parent(std::forward<ArgsType>(args)...){}

	virtual ~PlayerImp(){}
	void setOnShutdown(const function<void(const SockException &)> &cb) override {
		if (_delegate) {
			_delegate->setOnShutdown(cb);
		}
		_shutdownCB = cb;
	}
	void setOnPlayResult(const function<void(const SockException &ex)> &cb) override {
		if (_delegate) {
			_delegate->setOnPlayResult(cb);
		}
		_playResultCB = cb;
	}

    void setOnResume(const function<void()> &cb) override {
        if (_delegate) {
            _delegate->setOnResume(cb);
        }
        _resumeCB = cb;
    }

    bool isInited(int analysisMs) override{
        if (_delegate) {
            return _delegate->isInited(analysisMs);
        }
        return Parent::isInited(analysisMs);
    }
	float getDuration() const override {
		if (_delegate) {
			return _delegate->getDuration();
		}
		return Parent::getDuration();
	}
    float getProgress() const override{
        if (_delegate) {
            return _delegate->getProgress();
        }
        return Parent::getProgress();
    }
    void seekTo(float fProgress) override{
        if (_delegate) {
            return _delegate->seekTo(fProgress);
        }
        return Parent::seekTo(fProgress);
    }

    void setMediaSouce(const MediaSource::Ptr & src) override {
		if (_delegate) {
			_delegate->setMediaSouce(src);
		}
		_pMediaSrc = src;
    }

    vector<Track::Ptr> getTracks(bool trackReady = true) const override{
		if (_delegate) {
			return _delegate->getTracks(trackReady);
		}
		return Parent::getTracks(trackReady);
	}
protected:
	void onShutdown(const SockException &ex) override {
		if (_shutdownCB) {
			_shutdownCB(ex);
			_shutdownCB = nullptr;
		}
	}

	void onPlayResult(const SockException &ex) override {
		if(_playResultCB) {
			_playResultCB(ex);
			_playResultCB = nullptr;
		}
	}

	void onResume() override{
        if(_resumeCB){
            _resumeCB();
        }
    }
protected:
	function<void(const SockException &ex)> _shutdownCB;
	function<void(const SockException &ex)> _playResultCB;
    function<void()> _resumeCB;
    std::shared_ptr<Delegate> _delegate;
	MediaSource::Ptr _pMediaSrc;
};


class Demuxer : public PlayerBase{
public:
	class Listener{
	public:
		Listener() = default;
		virtual ~Listener() = default;
		virtual void onAddTrack(const Track::Ptr &track) = 0;
	};

	Demuxer(){};
	virtual ~Demuxer(){};

	/**
	 * 返回是否完成初始化完毕
	 * 在构造RtspDemuxer对象时有些rtsp的sdp不包含sps pps信息
	 * 所以要等待接收到到sps的rtp包后才能完成
	 *
	 * 在构造RtmpDemuxer对象时是无法获取sps pps aac_cfg等这些信息，
	 * 所以要调用inputRtmp后才会获取到这些信息，这时才初始化成功
	 * @param analysisMs 数据流最大分析时间 单位毫秒
	 * @return
	 */
	bool isInited(int analysisMs) override;

	/**
	 * 获取所有Track
	 * @return 所有Track
	 */
	vector<Track::Ptr> getTracks(bool trackReady = true) const override;

	/**
	 * 获取节目总时长
	 * @return 节目总时长,单位秒
	 */
	float getDuration() const override;

	/**
	 * 设置track监听器
	 */
	void setTrackListener(Listener *listener);
protected:
	void onAddTrack(const Track::Ptr &track);
protected:
	Listener *_listener = nullptr;
	AudioTrack::Ptr _audioTrack;
	VideoTrack::Ptr _videoTrack;
	Ticker _ticker;
	float _fDuration = 0;
};

} /* namespace mediakit */

#endif /* SRC_PLAYER_PLAYERBASE_H_ */
