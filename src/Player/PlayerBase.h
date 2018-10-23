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

using namespace std;
using namespace ZL::Util;
using namespace ZL::Media;
using namespace ZL::Network;

namespace ZL {
namespace Player {

class PlayerBase : public mINI{
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

	virtual bool isInited() const { return true; }
	//TrackVideo = 0, TrackAudio = 1
	virtual float getRtpLossRate(int trackType) const {return 0; }
    virtual float getDuration() const { return 0;}

    virtual int getTrackCount() const { return  0;}
	virtual Track::Ptr getTrack(int index) const {return nullptr;}
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
		if (m_parser) {
			m_parser->setOnShutdown(cb);
		}
		m_shutdownCB = cb;
	}
	void setOnPlayResult(const function<void(const SockException &ex)> &cb) override {
		if (m_parser) {
			m_parser->setOnPlayResult(cb);
		}
		m_playResultCB = cb;
	}

    bool isInited() const override{
        if (m_parser) {
            return m_parser->isInited();
        }
        return PlayerBase::isInited();
    }
	float getDuration() const override {
		if (m_parser) {
			return m_parser->getDuration();
		}
		return PlayerBase::getDuration();
	}
    float getProgress() const override{
        if (m_parser) {
            return m_parser->getProgress();
        }
        return PlayerBase::getProgress();
    }
    void seekTo(float fProgress) override{
        if (m_parser) {
            return m_parser->seekTo(fProgress);
        }
        return PlayerBase::seekTo(fProgress);
    }

    void setMediaSouce(const MediaSource::Ptr & src) override {
		if (m_parser) {
			return m_parser->setMediaSouce(src);
		}
		m_pMediaSrc = src;
    }

    virtual int getTrackCount() const override{
        if (m_parser) {
            return m_parser->getTrackCount();
        }
        return PlayerBase::getTrackCount();
    }
    virtual Track::Ptr getTrack(int index) const override{
        if (m_parser) {
            return m_parser->getTrack(index);
        }
        return PlayerBase::getTrack(index);
    }
protected:
	void onShutdown(const SockException &ex) override {
		if (m_shutdownCB) {
			m_shutdownCB(ex);
		}
	}
	void onPlayResult(const SockException &ex) override {
		if (m_playResultCB) {
			m_playResultCB(ex);
			m_playResultCB = nullptr;
		}
	}
protected:
	function<void(const SockException &ex)> m_shutdownCB;
	function<void(const SockException &ex)> m_playResultCB;
	std::shared_ptr<Parser> m_parser;
	MediaSource::Ptr m_pMediaSrc;
};

} /* namespace Player */
} /* namespace ZL */

#endif /* SRC_PLAYER_PLAYERBASE_H_ */
