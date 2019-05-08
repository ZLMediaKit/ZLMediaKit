/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

class DemuxerBase {
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

	/**
	 * 获取全部的Track
	 * @param trackReady 是否获取全部已经准备好的Track
	 * @return
	 */
	virtual vector<Track::Ptr> getTracks(bool trackReady = true) const { return vector<Track::Ptr>();}

	/**
	 * 获取特定Track
	 * @param type track类型
	 * @param trackReady 是否获取全部已经准备好的Track
	 * @return
	 */
	virtual Track::Ptr getTrack(TrackType type , bool trackReady = true) const {
		auto tracks = getTracks(trackReady);
		for(auto &track : tracks){
			if(track->getTrackType() == type){
				return track;
			}
		}
		return nullptr;
	}
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
protected:
    virtual void onShutdown(const SockException &ex) {}
    virtual void onPlayResult(const SockException &ex) {}
    /**
     * 暂停后恢复播放时间
     */
    virtual void onResume(){};
};

template<typename Parent,typename Parser>
class PlayerImp : public Parent
{
public:
	typedef std::shared_ptr<PlayerImp> Ptr;

	template<typename ...ArgsType>
	PlayerImp(ArgsType &&...args):Parent(std::forward<ArgsType>(args)...){}

	virtual ~PlayerImp(){}
	void setOnShutdown(const function<void(const SockException &)> &cb) override {
		if (_parser) {
			_parser->setOnShutdown(cb);
		}
		_shutdownCB = cb;
	}
	void setOnPlayResult(const function<void(const SockException &ex)> &cb) override {
		if (_parser) {
			_parser->setOnPlayResult(cb);
		}
		_playResultCB = cb;
	}

    void setOnResume(const function<void()> &cb) override {
        if (_parser) {
            _parser->setOnResume(cb);
        }
        _resumeCB = cb;
    }

    bool isInited(int analysisMs) override{
        if (_parser) {
            return _parser->isInited(analysisMs);
        }
        return PlayerBase::isInited(analysisMs);
    }
	float getDuration() const override {
		if (_parser) {
			return _parser->getDuration();
		}
		return PlayerBase::getDuration();
	}
    float getProgress() const override{
        if (_parser) {
            return _parser->getProgress();
        }
        return PlayerBase::getProgress();
    }
    void seekTo(float fProgress) override{
        if (_parser) {
            return _parser->seekTo(fProgress);
        }
        return PlayerBase::seekTo(fProgress);
    }

    void setMediaSouce(const MediaSource::Ptr & src) override {
		if (_parser) {
			return _parser->setMediaSouce(src);
		}
		_pMediaSrc = src;
    }

    vector<Track::Ptr> getTracks(bool trackReady = true) const override{
		if (_parser) {
			return _parser->getTracks(trackReady);
		}
		return PlayerBase::getTracks(trackReady);
	}
protected:
	void onShutdown(const SockException &ex) override {
		if (_shutdownCB) {
			_shutdownCB(ex);
			_shutdownCB = nullptr;
		}
	}

	void onPlayResult(const SockException &ex) override {
		if(!_playResultCB){
			return;
		}
		if(ex){
			//播放失败，则立即回调
			_playResultCB(ex);
			_playResultCB = nullptr;
			return;
		}
		//播放成功后，我们还必须等待各个Track初始化完毕才能回调告知已经初始化完毕
		if(isInited(0xFFFF)){
			//初始化完毕则立即回调
			_playResultCB(ex);
			_playResultCB = nullptr;
			return;
		}
		//播放成功却未初始化完毕，这个时候不回调汇报播放成功
	}

	void onResume() override{
        if(_resumeCB){
            _resumeCB();
        }
    }

    void checkInited(int analysisMs){
		if(!_playResultCB){
			return;
		}
		if(isInited(analysisMs)){
			_playResultCB(SockException(Err_success,"play success"));
			_playResultCB = nullptr;
		}
	}
protected:
	function<void(const SockException &ex)> _shutdownCB;
	function<void(const SockException &ex)> _playResultCB;
    function<void()> _resumeCB;
    std::shared_ptr<Parser> _parser;
	MediaSource::Ptr _pMediaSrc;
};


class Demuxer : public PlayerBase{
public:
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
	 * 获取所有可用Track，请在isInited()返回true时调用
	 * @return
	 */
	vector<Track::Ptr> getTracks(bool trackReady = true) const override;

	/**
	 * 获取节目总时长
	 * @return
	 */
	float getDuration() const override;
protected:
	AudioTrack::Ptr _audioTrack;
	VideoTrack::Ptr _videoTrack;
	Ticker _ticker;
	float _fDuration = 0;
};

} /* namespace mediakit */

#endif /* SRC_PLAYER_PLAYERBASE_H_ */
