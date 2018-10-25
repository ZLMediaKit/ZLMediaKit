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
#include "Player.h"
#include "Network/Socket.h"
#include "Util/mini.h"
#include "Util/RingBuffer.h"
#include "Common/MediaSource.h"
#include "Frame.h"
#include "Track.h"
using namespace toolkit;

namespace mediakit {

class DemuxerBase {
public:
	typedef std::shared_ptr<DemuxerBase> Ptr;

	virtual float getDuration() const { return 0;}
	virtual bool isInited() const { return true; }
	virtual vector<Track::Ptr> getTracks() const { return vector<Track::Ptr>();}
};


class PlayerBase : public DemuxerBase, public mINI{
public:
	typedef std::shared_ptr<PlayerBase> Ptr;
	typedef enum {
		RTP_TCP = 0,
		RTP_UDP = 1,
		RTP_MULTICAST = 2,
	} eRtpType;
    static Ptr createPlayer(const char* strUrl);

    //指定网卡ip
	static const char kNetAdapter[];
	//设置rtp传输类型，可选项有0(tcp，默认)、1(udp)、2(组播)
	//设置方法:player[PlayerBase::kRtpType] = 0/1/2;
	static const char kRtpType[];
	//rtsp认证用户名
	static const char kRtspUser[];
	//rtsp认证用用户密码，可以是明文也可以是md5,md5密码生成方式 md5(username:realm:password)
	static const char kRtspPwd[];
	//rtsp认证用用户密码是否为md5
	static const char kRtspPwdIsMD5[];

	PlayerBase(){}
	virtual ~PlayerBase(){}
	virtual void play(const char* strUrl) {}
	virtual void pause(bool bPause) {}
	virtual void teardown() {}

	virtual void setOnShutdown( const function<void(const SockException &)> &cb) {}
	virtual void setOnPlayResult( const function<void(const SockException &ex)> &cb) {}

    virtual float getProgress() const { return 0;}
    virtual void seekTo(float fProgress) {}
    virtual void setMediaSouce(const MediaSource::Ptr & src) {}
	//TrackVideo = 0, TrackAudio = 1
	virtual float getRtpLossRate(int trackType) const {return 0; }
protected:
    virtual void onShutdown(const SockException &ex) {}
    virtual void onPlayResult(const SockException &ex) {}
};

template<typename Parent,typename Parser>
class PlayerImp : public Parent
{
public:
	typedef std::shared_ptr<PlayerImp> Ptr;
	PlayerImp(){}
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

    bool isInited() const override{
        if (_parser) {
            return _parser->isInited();
        }
        return PlayerBase::isInited();
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

    vector<Track::Ptr> getTracks() const override{
		if (_parser) {
			return _parser->getTracks();
		}
		return PlayerBase::getTracks();
	}
protected:
	void onShutdown(const SockException &ex) override {
		if (_shutdownCB) {
			_shutdownCB(ex);
		}
	}
	void onPlayResult(const SockException &ex) override {
		if (_playResultCB) {
			_playResultCB(ex);
			_playResultCB = nullptr;
		}
	}
protected:
	function<void(const SockException &ex)> _shutdownCB;
	function<void(const SockException &ex)> _playResultCB;
	std::shared_ptr<Parser> _parser;
	MediaSource::Ptr _pMediaSrc;
};

} /* namespace mediakit */

#endif /* SRC_PLAYER_PLAYERBASE_H_ */
