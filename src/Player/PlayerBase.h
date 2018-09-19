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
#include "Common/MediaSource.h"
#include "Util/RingBuffer.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Media;
using namespace ZL::Network;

namespace ZL {
namespace Player {

class TrackFrame : public Buffer {
public:
	typedef std::shared_ptr<TrackFrame> Ptr;
	virtual ~TrackFrame(){}
	virtual uint32_t stamp() = 0;
};

class TrackFormat {
public:
	typedef std::shared_ptr<TrackFormat> Ptr;
	typedef RingBuffer<TrackFrame::Ptr> RingType;
	typedef RingType::RingReader::Ptr ReaderType;

	typedef enum {
        VideoCodecInvalid = -1,
        VideoCodecH264 = 0,
        VideoCodecMax
    } VideoCodecID;

    typedef enum {
        AudioCodecInvalid = -1,
        AudioCodecAAC = 0,
        AudioCodecMax
    } AudioCodecID;
	TrackFormat(){
		_ring = std::make_shared<RingType>();
	}
    virtual ~TrackFormat(){}
    virtual TrackType getTrackType() const = 0;
    virtual int getCodecId() const = 0;

	ReaderType attachReader(bool useBuffer = false){
		return _ring->attach(useBuffer);
    }

	void writeFrame(const TrackFrame::Ptr &frame,bool keypos = true){
		_ring->write(frame, keypos);
	}
private:
	RingType::Ptr _ring;
};

class VideoTrackFormat : public TrackFormat {
public:
    TrackType getTrackType() const override { return TrackVideo;};
    virtual int getVideoHeight() const = 0;
    virtual int getVideoWidth() const  = 0;
    virtual float getVideoFps() const = 0;
};

class AudioTrackFormat : public TrackFormat {
public:
    TrackType getTrackType() const override { return TrackAudio;};
    virtual int getAudioSampleRate() const  = 0;
    virtual int getAudioSampleBit() const = 0;
    virtual int getAudioChannel() const = 0;
};

class H264TrackFormat : public VideoTrackFormat{
public:
	H264TrackFormat(const string &sps,const string &pps){
		_sps = sps;
		_pps = pps;
	}
	const string &getSps() const{
		return _sps;
	}
	const string &getPps() const{
		return _pps;
	}
	int getCodecId() const override{
		return TrackFormat::VideoCodecH264;
	}
private:
	string _sps;
	string _pps;
};

class AACTrackFormat : public AudioTrackFormat{
public:
	AACTrackFormat(const string &aac_cfg){
		_cfg = aac_cfg;
	}
	const string &getAacCfg() const{
		return _cfg;
	}
	int getCodecId() const override{
		return TrackFormat::AudioCodecAAC;
	}
private:
	string _cfg;
};


class MediaFormat {
public:
	virtual ~MediaFormat(){};
	virtual int getVideoHeight() const { return 0; };
	virtual int getVideoWidth() const { return 0; };
	virtual float getVideoFps() const { return 0; };

	virtual int getAudioSampleRate() const { return 0; };
	virtual int getAudioSampleBit() const { return 0; };
	virtual int getAudioChannel() const { return 0; };

	virtual const string& getPps() const { static string null;return null; };
	virtual const string& getSps() const { static string null;return null; };
	virtual const string& getAudioCfg() const { static string null;return null; };
};

class PlayerBase : public MediaFormat,public mINI{
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

	PlayerBase(){};
	virtual ~PlayerBase(){};
	virtual void play(const char* strUrl) {};
	virtual void pause(bool bPause) {};
	virtual void teardown() {};

	virtual void setOnShutdown( const function<void(const SockException &)> &cb) {};
	virtual void setOnPlayResult( const function<void(const SockException &ex)> &cb) {};
	virtual void setOnVideoCB( const function<void(const H264Frame &frame)> &cb) {};
	virtual void setOnAudioCB( const function<void(const AdtsFrame &frame)> &cb) {};
    
    virtual float getProgress() const { return 0;};
    virtual void seekTo(float fProgress) {};
    virtual void setMediaSouce(const MediaSource::Ptr & src) {};

	virtual bool isInited() const { return true; };
	//TrackVideo = 0, TrackAudio = 1
	virtual float getRtpLossRate(int trackType) const {return 0; };
    virtual float getDuration() const { return 0;};

    virtual bool containAudio() const { return false; };
    virtual bool containVideo() const { return false; };

    virtual int getTrackCount() const { return  0;};
	virtual TrackFormat::Ptr getTrack(int index) const {return nullptr;};
protected:
    virtual void onShutdown(const SockException &ex) {};
    virtual void onPlayResult(const SockException &ex) {};
};

template<typename Parent,typename Parser>
class PlayerImp : public Parent
{
public:
	typedef std::shared_ptr<PlayerImp> Ptr;
	PlayerImp(){};
	virtual ~PlayerImp(){};
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
	void setOnVideoCB(const function<void(const H264Frame &frame)> &cb) override{
		if (m_parser) {
			m_parser->setOnVideoCB(cb);
		}
		m_onGetVideoCB = cb;
	}
	void setOnAudioCB(const function<void(const AdtsFrame &frame)> &cb) override{
		if (m_parser) {
			m_parser->setOnAudioCB(cb);
		}
		m_onGetAudioCB = cb;
	}
	int getVideoHeight() const override{
		if (m_parser) {
			return m_parser->getVideoHeight();
		}
		return PlayerBase::getVideoHeight();
	}

	int getVideoWidth() const override{
		if (m_parser) {
			return m_parser->getVideoWidth();
		}
		return PlayerBase::getVideoWidth();
	}

	float getVideoFps() const override{
		if (m_parser) {
			return m_parser->getVideoFps();
		}
		return PlayerBase::getVideoFps();
	}

	int getAudioSampleRate() const override{
		if (m_parser) {
			return m_parser->getAudioSampleRate();
		}
		return PlayerBase::getAudioSampleRate();
	}

	int getAudioSampleBit() const override{
		if (m_parser) {
			return m_parser->getAudioSampleBit();
		}
		return PlayerBase::getAudioSampleBit();
	}

	int getAudioChannel() const override{
		if (m_parser) {
			return m_parser->getAudioChannel();
		}
		return PlayerBase::getAudioChannel();
	}

	const string& getPps() const override{
		if (m_parser) {
			return m_parser->getPps();
		}
		return PlayerBase::getPps();
	}

	const string& getSps() const override{
		if (m_parser) {
			return m_parser->getSps();
		}
		return PlayerBase::getSps();
	}

	const string& getAudioCfg() const override{
		if (m_parser) {
			return m_parser->getAudioCfg();
		}
		return PlayerBase::getAudioCfg();
	}
	bool containAudio() const override{
		if (m_parser) {
			return m_parser->containAudio();
		}
		return PlayerBase::containAudio();
	}
    bool containVideo() const override{
        if (m_parser) {
            return m_parser->containVideo();
        }
        return PlayerBase::containVideo();
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
    };
    void seekTo(float fProgress) override{
        if (m_parser) {
            return m_parser->seekTo(fProgress);
        }
        return PlayerBase::seekTo(fProgress);
    };

    void setMediaSouce(const MediaSource::Ptr & src) override {
		if (m_parser) {
			return m_parser->setMediaSouce(src);
		}
		m_pMediaSrc = src;
    };

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
	function<void(const H264Frame &frame)> m_onGetVideoCB;
	function<void(const AdtsFrame &frame)> m_onGetAudioCB;
	MediaSource::Ptr m_pMediaSrc;

};
} /* namespace Player */
} /* namespace ZL */

#endif /* SRC_PLAYER_PLAYERBASE_H_ */
